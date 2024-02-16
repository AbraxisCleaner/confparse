#ifndef _CONFPARSE_H_
#define _CONFPARSE_H_
#include <stdlib.h>
#include <memory.h>

#define CFG_HEAP_SIZE 1024
#define CFG_SEARCH_DEPTH_MAX (uint)-1

namespace cfg
{
    inline size_t StringLength(char *str) {
	    auto c = str;
    	while (*c) ++c;
	    return (c - str);
    }

    inline bool StringCompare(char *str1, char *str2) {
	    auto c1 = str1;
    	auto c2 = str2;
	    while (*c1 && *c2) {
		    if (*c1 != *c2)
			    return false;
    		++c1;
	    	++c2;
    	}
    	return true;
    }

	typedef void *(*malloc_t)(size_t);
	typedef void *(*realloc_t)(void *, size_t);
	typedef void (*free_t)(void *);
	struct cbk {
		static malloc_t malloc;
		static realloc_t realloc;
		static free_t free;
	};

	struct Heap {
		char *base;
		char *ceiling;
		char *free;
		Heap *next;
	};
   
    inline void ReleaseHeap(Heap *h) {
        if (h->next) {
            ReleaseHeap(h->next);
            cbk::free(h->next);
        }
    }
    
    enum eFileType {
        eFileType_Unknown,
        eFileType_Ini,
        eFileType_Xml,
        eFileType_Json,
        eFileType_Yaml,
    };

    struct Node
    {
        char *name;
        char *str;
        Node *first_attribute; // Primarily for XML
        Node *first_child;
        Node *next;

        Node *GetChild( char *name, unsigned int depth );
        Node *GetChild( unsigned int idx );
        Node *GetAttribute( char *name ); // Primarily for XML
        Node *GetSibling( char *name );
        inline int AsInteger() { if (str) return atoi(str); return 0; }
        inline float AsFloat() { if (str) return (float)atof(str); return 0; }
    };

    struct Container
    {
        eFileType file_type;
        Heap  base_heap;
        Heap *heap;
        Node *first;

        bool   Parse( char *source, size_t len, eFileType type );
        void   Release();
        Node  *GetNode( char *name, unsigned int depth );

        // 'Print' outputs a correctly formatted document in the format specified by file_type.
        // Note: The file_type *MUST* match the file_type of the input document. This is subject to change.
        size_t Print( char **dst );
    };
}
#endif // _CONFPARSE_H_

#if defined(CFGPARSE_IMPLEMENTATION) && !defined(_CONFPARSE_CPP_)
#define _CONFPARSE_CPP_

#ifndef STB_SPRINTF_IMPLEMENTATION
    #define STB_SPRINTF_IMPLEMENTATION
#endif
#include "stb_sprintf.h"

cfg::malloc_t  cfg::cbk::malloc = ::malloc;
cfg::realloc_t cfg::cbk::realloc = ::realloc;
cfg::free_t    cfg::cbk::free = ::free;

#define CFG_PRINT_TABS(buf, num) for (auto i = num; i != 0; --i) { *buf = '\t'; ++buf; }
#define CFG_IS_LETTER(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define CFG_IS_NUMBER(c) (c >= '0' && c <= '9')
#define CFG_IS_WHITESPACE(c) (c == ' ' || c == '\n' || c == '\r' || c == '\t')
#define CFG_IS_SPECIAL(c) (!CFG_IS_LETTER(c) && CFG_IS_NUMBER(c) && CFG_IS_WHITESPACE(c))
#define CFG_SIZE(s, e) ((e - s) + 1)

/// --- Attribute --- ///
cfg::Node *cfg::Node::GetSibling(char *name) {
    auto s = next;
    while (s) {
        if (cfg::StringCompare(name, s->name))
            return s;
        s = s->next;
    }
    return nullptr;
}

/// ---- Node ---- ///
cfg::Node *cfg::Node::GetAttribute(char *name) {
    auto a = first_attribute;
    while (a) {
        if (cfg::StringCompare(name, a->name))
            return a;
        a = a->next;
    }
    return nullptr;
}

cfg::Node *cfg::Node::GetChild(char *name, unsigned int depth) {
    auto c = first_child;
    while (c) {
        if (cfg::StringCompare(name, c->name))
            return c;
        if (depth) {
            auto r = c->GetChild(name, depth - 1);
            if (r) 
                return r;
        }
        c = c->next;
    }
    return nullptr;
}

cfg::Node *cfg::Node::GetChild( unsigned int idx ) {
    for ( auto c = first_child; c; c = c->next ) {
        if ( idx == 0 )
            return c;
    }
    return nullptr;
}

cfg::Container *g_curr_container;
char *g_curr_dst_buffer;

// ---- Forward for simplicity
#if !defined(CFGPARSE_ALL)
#if !defined(CFGPARSE_INI)
bool ParseIni( cfg::Container *ctn, char *source, size_t len ) { return false; }
size_t PrintIni( cfg::Container *ctn, char **dst ) { return 0; }
#endif // CFGPARSE_INI
#if !defined(CFGPARSE_JSON)
bool ParseJson( cfg::Container *ctn, char *source, size_t len ) { return false; }
size_t PrintJson( cfg::Container *ctn, char **dst ) { return 0; }
#endif // CFGPARSE_JSON
#if !defined(CFGPARSE_XML)
bool ParseXml( cfg::Container *ctn, char *source, size_t len ) { return false; }
size_t PrintXml( cfg::Container *ctn, char **dst ) { return 0; }
#endif // CFGPARSE_XML
#if !defined(CFGPARSE_JSON)
bool ParseYaml( cfg::Container *ctn, char *source, size_t len ) { return false; }
size_t PrintYaml( cfg::Container *ctn, char **dst ) { return 0; }
#endif // CFGPARSE_JSON
#endif // CFGPARSE_ALL

#if defined(CFGPARSE_INI) || defined(CFGPARSE_ALL)
bool ParseIni(cfg::Container *ctn, char *source, size_t len) {
	auto c = source;
	auto tmp = c;

	size_t total_size = 0;

	// Measure strings.
	while (*c) {
		if (*c == '[') {
			total_size += sizeof(cfg::Node);
			
			++c;
			tmp = c;
			while (*c != ']') ++ c;
			total_size += (c - tmp) + 1;
		}
		else if (*c == '=') {
			total_size += sizeof(cfg::Node);

			--c;
			while (*c == ' ') --c;
			tmp = c + 1;
			while (CFG_IS_LETTER(*c) || CFG_IS_NUMBER(*c) || *c == '_') --c;
			++c;
			total_size += (tmp - c) + 1;

			while (*c != '=') ++c;
			++c;
			while (*c == ' ') ++c;
			tmp = c;

			while (CFG_IS_LETTER(*c) || CFG_IS_NUMBER(*c) || *c == '_' || *c == '-' || *c == '.') ++c;
			total_size += (c - tmp) + 1;
		}

		++c;
	}

	if (total_size == 0)
		return false;

	// Allocate
	ctn->base_heap.base = (char *)cfg::cbk::malloc(total_size);
	memset(ctn->base_heap.base, 0, total_size);

	ctn->base_heap.free = ctn->base_heap.base;
	char *&stack = ctn->base_heap.free;

	ctn->first = nullptr;

	// Copy strings.
	c = source;
	tmp = c;
    cfg::Node *active_section = nullptr;
    cfg::Node *active_value = nullptr;

	while (*c) {
		if (*c == '[') {
            auto section = (cfg::Node *)stack;
			stack += sizeof(cfg::Node);

			++c;
			tmp = c;

			while (*c != ']') ++c;
			memcpy(stack, tmp, (c - tmp));
			section->name = stack;
			stack += (c - tmp) + 1;

			if (!ctn->first)
				ctn->first = section;
			if (active_section)
				active_section->next = section;
			active_section = section;
		}
		else if (*c == '=') {
			auto val = (cfg::Node *)stack;
			stack += sizeof(cfg::Node);

			--c;
			while (*c == ' ') --c;
			tmp = (c + 1);

			while (CFG_IS_LETTER(*c) || CFG_IS_NUMBER(*c) || *c == '_') --c;
			++c;

			memcpy(stack, c, (tmp - c));
			val->name = stack;
			stack += (tmp - c) + 1;

			while (*c != '=') ++c;
			++c;
			while (*c == ' ') ++c;

			tmp = c;
			while (CFG_IS_LETTER(*c) || CFG_IS_NUMBER(*c) || *c == '_' || *c == '-' || *c == '.') ++c;
			memcpy(stack, tmp, (c - tmp));
			val->str = stack;
			stack += (c - tmp) + 1;

			if (!active_section->first_attribute)
				active_section->first_attribute = val;
			if (active_value)
				active_value->next = val;
			active_value = val;
		}

		++c;
	}

	// Setup heap for user-made allocations.
	ctn->base_heap.next = (cfg::Heap *)cfg::cbk::malloc(sizeof(cfg::Heap) + CFG_HEAP_SIZE);
	ctn->heap = ctn->base_heap.next;
	ctn->heap->base = (char *)(((uintptr_t)ctn->heap) + sizeof(cfg::Heap));
    ctn->heap->ceiling = (ctn->heap->base) + CFG_HEAP_SIZE;
	ctn->heap->free = ctn->heap->base;
	ctn->heap->next = nullptr;

    ctn->file_type = cfg::eFileType_Ini;
    return true;
}

size_t PrintIni(cfg::Container *ctn, char **dst) {
    // Measure
    size_t total = 1; // \0
    size_t tmp = 0;

    for (auto s = ctn->first; s; s = s->next) {
        total += cfg::StringLength(s->name) + 4; // [*name*]\n*attributes*\n

        for (auto a = s->first_attribute; a; a = a->next) {
            total += cfg::StringLength(a->name) + cfg::StringLength(a->str) + 2; // *name*=*str*\n
        }
    }

    *dst = (char *)cfg::cbk::malloc(total);
    memset(*dst, 0, total);

    auto c = *dst;

    for (auto s = ctn->first; s; s = s->next) {
        *c = '[';
        ++c;

        tmp = cfg::StringLength(s->name);
        memcpy(c, s->name, tmp);
        c += tmp;
        
        *c = ']';
        *(c + 1) = '\n';
        c += 2;

        for (auto a = s->first_attribute; a; a = a->next) {
            tmp = cfg::StringLength(a->name);
            memcpy(c, a->name, tmp);
            c += tmp;

            *c = '=';
            ++c;

            tmp = cfg::StringLength(a->str);
            memcpy(c, a->str, tmp);
            c += tmp;
            
            *c = '\n';
            ++c;
        }

        *c = '\n';
        ++c;
    }

    return total;
}
#endif // CFGPARSE_INI

#if defined(CFGPARSE_XML) || defined(CFGPARSE_ALL)
cfg::Node *ParseXmlNode( char *&c, char *&stack, cfg::Node *parent, cfg::Container *ctn ) {
    cfg::Node *node = (cfg::Node *)stack;
    stack += sizeof( cfg::Node );

    node->next = nullptr;
    node->name = nullptr;
    node->str = nullptr;
    node->first_child = nullptr;
    node->first_attribute = nullptr;

    // Store name.
    ++c;
    auto tmp = c;
    while (*c != ' ' && *c != '>' && *c != '/') ++c;
    memcpy(stack, tmp, (c - tmp));
    node->name = stack;
    stack += (c - tmp) + 1;

    // Store attributes.
    cfg::Node *prev_attribute = nullptr;

    while (*c != '>') {
        if (*c == '=') {
            cfg::Node *attrib = (cfg::Node *)stack;
            stack += sizeof(cfg::Node);

            --c;
            while (*c == ' ') ++c;
            tmp = (c + 1);
            while (*c != ' ') --c;
            ++c;

            memcpy(stack, c, (tmp - c));
            attrib->name = stack;
            stack += (tmp - c) + 1;

            while (*c != '"') ++c;
            ++c;
            tmp = c;
            while (*c != '"') ++c;

            memcpy(stack, tmp, (c - tmp));
            attrib->str = stack;
            stack += (c - tmp) + 1;

            if (prev_attribute)
                prev_attribute->next = attrib;
            prev_attribute = attrib;
            if (!node->first_attribute)
                node->first_attribute = attrib;
        }

        ++c;
    }

    // If the node self-terminates, return.
    if (*(c - 1) == '/') {
        while (*c != '<') ++c;
        return node;
    }

    // Look for the next *thing*.
    ++c;
    while (CFG_IS_WHITESPACE(*c)) {
        ++c;
    }

    // If the next *thing* is some kind of text.
    if (*c != '<') {
        // Go to the end of the string
        tmp = c;
        while (*c != '<') ++c;
        --c;
        while (CFG_IS_WHITESPACE(*c)) --c;
        ++c;

        // Store.
        memcpy(stack, tmp, (c - tmp));
        node->str = stack;
        stack += (c - tmp) + 1;

        // Skip to the start of the next node.
        while (*c != '>') ++c;
        while (*c != '<') ++c;
        return node;
    }

    // While the next node ISN'T a cap node...
    cfg::Node *prev_node = nullptr;

    while (*(c + 1) != '/') {
        cfg::Node *current_child = ParseXmlNode(c, stack, node, ctn);
        if (prev_node)
            prev_node->next = current_child;
        if (!node->first_child)
            node->first_child = current_child;
        prev_node = current_child;
    }

    while (*c != '>') ++c;
    while (*c != '<' && *c) ++c;
    return node;
}

bool ParseXml( cfg::Container *ctn, char *source, size_t len ) {
    auto c = source;
    auto tmp = c;
    size_t total_size = 0;

    // Do measurements.
    while (*c) {
        switch (*c) {
            case '<': 
                {
                    if (*(c + 1) == '/' || *(c + 1) == '?') {
                        while (*c != '>') ++c;
                        break;
                    }

                    total_size += sizeof(cfg::Node);

                    ++c;
                    tmp = c;
                    while (*c != ' ' && *c != '>' && *c != '/') ++c;
                    total_size += ((c - tmp) + 1);
                    continue;
                } break;

            case '>':
                {
                    if (*(c - 1) == '/')
                        break;
                    
                    ++c;

                    while (*c != '<') {
                        if (!CFG_IS_WHITESPACE(*c)) {
                            tmp = c;
                            while (*c != '<') ++c;
                            --c;
                            while (CFG_IS_WHITESPACE(*c)) --c;
                            ++c;
                            total_size += (c - tmp) + 1;
                            continue;
                        }
                        ++c;
                    }
                    --c;
                } break;

            case '=':
                {
                    total_size += sizeof(cfg::Node);

                    --c;
                    while (*c == ' ') --c;
                    tmp = c + 1;
                    while (*c != ' ') --c;
                    ++c;
                    total_size += ((tmp - c) + 1);

                    while (*c != '"') ++c;
                    ++c;
                    tmp = c;
                    while (*c != '"') ++c;
                    total_size += ((c - tmp) + 1);
                } break;
        }

        ++c;
    }

    // Allocate.
    ctn->base_heap.base = (char *)cfg::cbk::malloc(total_size);
    memset(ctn->base_heap.base, 0, total_size);
    ctn->base_heap.ceiling = (ctn->base_heap.base + total_size);
    ctn->base_heap.free = ctn->base_heap.ceiling;

    char *stack = ctn->base_heap.base;

    // Iterate and store strings.
    c = source;
    tmp = c;

    cfg::Node *prev = nullptr;
    
    while (*c) {
        if (*c == '<') {
            if (*(c + 1) == '?' || *(c + 1) == '/') {
                while (*c != '>') ++c;
                continue;
            }

            cfg::Node *node = ParseXmlNode(c, stack, prev, ctn);
            if (!ctn->first)
                ctn->first = node;
            if (prev)
                prev->next = node;
            prev = node;
        }

        ++c;
    }

    // Allocate growth heap.
    ctn->base_heap.next = (cfg::Heap *)cfg::cbk::malloc(sizeof(cfg::Heap) + CFG_HEAP_SIZE);
    ctn->heap = ctn->base_heap.next;
    ctn->heap->base = (char *)(((uintptr_t)ctn->heap) + sizeof(cfg::Heap));
    ctn->heap->ceiling = ctn->heap->base + CFG_HEAP_SIZE;
    ctn->heap->free = ctn->heap->base;
    ctn->heap->next = nullptr;
    memset(ctn->heap->base, 0, CFG_HEAP_SIZE);

    return true;
}

size_t MeasureXmlNodeStrings( cfg::Node *n, int depth ) {
    size_t total = depth; // '\t...'

    auto name_len = cfg::StringLength(n->name);
    total += name_len + 1; // '<*name*'
    if (n->str)
        total += cfg::StringLength(n->str) + name_len + 3; // '>*str*</*name*>'

    auto a = n->first_attribute;
    while (a) {
        total += (cfg::StringLength(a->name) + cfg::StringLength(a->str) + 4); // ' *name*="*str*"'
        a = a->next;
    }

    if (!n->first_child && !n->str) {
        total += 3; // '/>\n'
        return total;
    }

    total++; // '\n'

    for (auto c = n->first_child; c != nullptr; c = c->next) {
        total += MeasureXmlNodeStrings(c, depth + 1);
    }

    return total;
}

size_t MeasureXmlStrings( cfg::Container *ctn ) {
    size_t total = 1; 

#if 0
    for (auto n = ctn->first; n != nullptr; n = n->next) {
        total += MeasureXmlNodeStrings(n, 0);
    }
#endif

    auto heap = &ctn->base_heap;
    while (heap) {
        total += heap->free - heap->base;
        heap = heap->next;
    }

    return total;
}

void PrintXmlNode( cfg::Node *node, char *&c, int depth ) {
    CFG_PRINT_TABS(c, depth);

    stbsp_sprintf(c, "<%s", node->name);
    (c, 0);

    for (auto a = node->first_attribute; a != nullptr; a = a->next) {
        stbsp_sprintf(c, " %s=\"%s\"", a->name, a->str);
        (c, 0);
    }

    if (!node->first_child && !node->str) {
        *c = '/';
        *(c + 1) = '>';
        *(c + 2) = '\n';
        c += 3;
        return;
    }

    *c = '>';
    ++c;

    if (node->str) {
        stbsp_sprintf(c, "%s</%s>\n", node->str, node->name);
        (c, 0);
        return;
    }

    *c = '\n';
    ++c;

    for (auto child = node->first_child; child != nullptr; child = child->next) {
        PrintXmlNode(child, c, depth + 1);
    }

    CFG_PRINT_TABS(c, depth);
    stbsp_sprintf(c, "</%s>\n", node->name);
    (c, 0);
}

size_t PrintXml( cfg::Container *ctn, char **dst ) {
    size_t total = MeasureXmlStrings(ctn);

    *dst = (char *)cfg::cbk::malloc(total);
    memset(*dst, 0, total);

    char *c = *dst;

    for (auto n = ctn->first; n != nullptr; n = n->next) {
        PrintXmlNode(n, c, 0);
    }

    return total;
}
#endif // CFGPARSE_XML

#if defined(CFGPARSE_JSON) || defined(CFGPARSE_ALL)
cfg::Node *ParseJsonNode( char *&c, char *&stack, cfg::Node *parent ) {
    auto node = (cfg::Node *)stack;
    stack += sizeof(cfg::Node);

    if (*c == ':') {
        --c;
        while (*c != '"') --c;
        auto tmp = c;
        --c;
        while (*c != '"') --c;
        ++c;

        memcpy(stack, c, tmp - c);
        node->name = stack;
        stack += ((tmp - c) + 1);

        while (*c != ':') ++c;
        tmp = c;
        ++c;

        while (CFG_IS_WHITESPACE(*c)) ++c;

        // If the next thing is a string...
        if (*c == '"') {
            ++c;
            tmp = c;
            while (*c != '"') ++c;

            memcpy(stack, tmp, (c - tmp));
            node->str = stack;
            stack += ((c - tmp) + 1);

            return node;
        }

        // If the next thing is an array.
        if (*c == '[') {
            cfg::Node *prev_arr_node = nullptr;

            while (*c != ']') {
                if (*c == '"') {
                    cfg::Node *new_node = (cfg::Node *)stack;
                    stack += sizeof(cfg::Node);

                    ++c;
                    tmp = c;
                    while (*c != '"') ++c;
                    memcpy(stack, tmp, (c - tmp));
                    new_node->str = stack;
                    stack += ((c - tmp) + 1);

                    if (prev_arr_node)
                        prev_arr_node->next = new_node;
                    prev_arr_node = new_node;
                    if (!node->first_child)
                        node->first_child = new_node;
                }

                if (*c == '{') {
                    auto new_node = ParseJsonNode(c, stack, node);
                
                    if (prev_arr_node)
                        prev_arr_node->next = new_node;
                    prev_arr_node = new_node;
                    if (!node->first_child)
                        node->first_child = new_node;
                }

                ++c;
            }

            return node;
        }
    }

    // If the next thing is a set of children...
    if (*c == '{') {
        cfg::Node *prev_child = nullptr;        

        while (*c != '}') {
            if (*c == ':') {
                auto child = ParseJsonNode(c, stack, node);

                if (prev_child)
                    prev_child->next = child;
                prev_child = child;
                if (!node->first_child)
                    node->first_child = child;
            }
    
            ++c;
        }
    }

    return node;
}

bool ParseJson( cfg::Container *ctn, char *source, size_t len ) {
    auto c = source;
    auto tmp = c;
    size_t total = 0;

    // The first character MUST be a '{'.
    while (CFG_IS_WHITESPACE(*c)) ++c;
    if (*c != '{')
        return false;

    while (*c) {
        while (CFG_IS_WHITESPACE(*c)) ++c;

        switch (*c) {
            case ':': total += sizeof(cfg::Node); break;
            case '"':{
                         ++c;
                         tmp = c;
                         while (*c != '"') ++c;
                         total += CFG_SIZE(tmp, c);
                     } break;
            case '[': total += sizeof(cfg::Node); break;
        }

        ++c;
    }

    // FIXME: Something about the measurements isn't right, and at some point during parsing we end up overflowing the buffer.
    //        This addition is just a hack to ensure we have enough space. I have no idea if it scales with file size. 
    //        ~ dalex
    total += 512;

    // Allocate
    ctn->base_heap.base = (char *)cfg::cbk::malloc(total);
    memset(ctn->base_heap.base, 0, total);
    ctn->base_heap.ceiling = (ctn->base_heap.base + total);
    ctn->base_heap.free = ctn->base_heap.ceiling;
    ctn->base_heap.next = nullptr;

    c = source;
    char *stack = ctn->base_heap.base;

    ctn->first = nullptr;
    cfg::Node *prev_node = nullptr;

    while (*c) {
        if (*c == ':') {
            auto node = ParseJsonNode(c, stack, nullptr);
            if (prev_node)
                prev_node->next = node;
            prev_node = node;
            if (!ctn->first)
                ctn->first = node;
        }
        ++c;
    }

    // Allocate subsequent heap.
    ctn->base_heap.next = (cfg::Heap *)cfg::cbk::malloc(sizeof(cfg::Heap) + CFG_HEAP_SIZE);
    ctn->heap = ctn->base_heap.next;
    ctn->heap->base = (char *)(((uintptr_t)ctn->heap) + sizeof(cfg::Heap));
    ctn->heap->ceiling = (char *)(((uintptr_t)ctn->heap->base) + CFG_HEAP_SIZE);
    ctn->heap->free = ctn->heap->base;

    ctn->file_type = cfg::eFileType_Json;
    return true;
}

size_t MeasureJsonNodeStrings (cfg::Node *n, int depth ) {
    size_t total = depth + 6; // '"..." : ...\n'

    // The node wouldn't have a name if it was an element in an array.
    if (n->name)
        total += cfg::StringLength(n->name);

    // n->str and n->first_child are mutually exclusive.
    if (n->str)
        total += cfg::StringLength(n->str) + 2; // : '"..."'
    else if (n->first_child) {
        total += 2; // '{}' || '[]';

        auto c = n->first_child;
        while (c) {
            total += MeasureJsonNodeStrings(c, depth + 1);
            c = c->next;
        }
    }

    return total;
}

void PrintJsonNode( cfg::Node *n, char *&c, int depth ) {
    CFG_PRINT_TABS(c, depth);

    if (n->name)
        c += stbsp_sprintf(c, "\"%s\" : ", n->name);

    if (n->str) {
        c += stbsp_sprintf(c, "\"%s\"", n->str);
        if (n->next) {
            *c = ',';
            ++c;
        }
        *c = '\n';
        ++c;
        return;
    }

    if (n->first_child) {
        // If the children are part of an array, they won't have names.
        if (!n->first_child->name) {
            *c = '[';
            ++c;

            auto arr_child = n->first_child;
            while (arr_child) {

                if (arr_child->str) {
                    c += stbsp_sprintf(c, " \"%s\"", arr_child->str);

                    if ( arr_child->next ) {
                        *c = ',';
                        ++c;
                    }
                }
                // If the child has children, it's the start of a new set of nodes.
                else if (arr_child->first_child) {
                    *c = '{';
                    *(++c) = '\n';
                    ++c;

                    auto arr_element = arr_child->first_child;
                    while (arr_element) {
                        PrintJsonNode(arr_element, c, depth + 1);
                        arr_element = arr_element->next;
                    }
                }

                arr_child = arr_child->next;
            }

            *c = ']';
            ++c;
            if ( n->next ) {
                *c = ',';
                ++c;
            }
            *c = '\n';
            ++c;

            return;
        }
        // Default child behavior.
        else {
            *c = '{';
            *(++c) = '\n';
            ++c;

            auto child = n->first_child;
            while (child) {
                PrintJsonNode(child, c, depth + 1);
                child = child->next;
            }
        }
    }

    CFG_PRINT_TABS(c, depth);
    *c = '}';
    ++c;
    if ( n->next ) {
        *c = ',';
        ++c;
    }
    *c = '\n';
    ++c;
}

size_t PrintJson( cfg::Container *ctn, char **dst ) {
    size_t total = 1; // '\0'

    auto n = ctn->first;
    while (n) {
        total += MeasureJsonNodeStrings(n, 0);
        n = n->next;
    }

    // Do the printing.
    // FIXME: See the FIXME note in ParseJson for details on hacking this.
    total += 512;
    *dst = (char *)cfg::cbk::malloc(total);
    memset(*dst, 0, total);

    g_curr_dst_buffer = *dst;

    char *c = *dst;
    *c = '{';
    *(c + 1) = '\n';
    c += 2;

    n = ctn->first;
    while (n) {
        PrintJsonNode(n, c, 1);
        n = n->next;
    }

    *c = '}';
    return total;
}
#endif // CFGPARSE_JSON

#if defined(CFGPARSE_JSON) || defined(CFGPARSE_ALL)
bool ParseYaml( cfg::Container *ctn, char *source, size_t len ) {
    return true;
}

size_t PrintYaml( cfg::Container *ctn, char **dst ) {
    return 0;
}
#endif // CFGPARSE_JSON

/// ------------------- ///
/// ---- Container ---- ///
bool cfg::Container::Parse(char *source, size_t len, eFileType type) {
    g_curr_container = this;

    switch (type) {
        case eFileType_Ini: return ParseIni(this, source, len);
        case eFileType_Xml: return ParseXml(this, source, len);
        case eFileType_Json: return ParseJson(this, source, len);
        case eFileType_Yaml: return ParseYaml(this, source, len);
    }
    return false;
}

size_t cfg::Container::Print(char **dst) {
    switch (file_type) {
        case eFileType_Ini: return PrintIni(this, dst);
        case eFileType_Xml: return PrintXml(this, dst);
        case eFileType_Json: return PrintJson(this, dst);
        case eFileType_Yaml: return PrintYaml(this, dst);
    }
    *dst = nullptr;
    return 0;
}

void cfg::Container::Release() {
    ReleaseHeap(&base_heap);
    cbk::free(base_heap.base);
    heap = nullptr;
    first = nullptr;
    file_type = eFileType_Unknown;
}

cfg::Node *cfg::Container::GetNode(char *name, unsigned int depth) {
    auto n = first;
    while (n) {
        if (cfg::StringCompare(name, n->name))
            return n;
        if (depth) {
            auto r = n->GetChild(name, depth);
            if (r)
                return r;
        }
        n = n->next;
    }
    return nullptr;
}

#endif // CFGPARSE_IMPLEMENTATION

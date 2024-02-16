#pragma once
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

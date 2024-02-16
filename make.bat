@echo off
cls
pushd bin

cl.exe ..\src\main.cpp -DCFGPARSE_IMPLEMENTATION -DCFGPARSE_ALL -std:c++20 -Zi -permissive -link -nologo -incremental:no kernel32.lib shell32.lib user32.lib -out:test.exe
copy test.exe ..

popd

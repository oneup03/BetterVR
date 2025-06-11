REM rmdir /Q /S build
cmake -G "Visual Studio 17 2022" -A x64 --preset default -B ./build
cmake --build ./build --config Debug --target ALL_BUILD
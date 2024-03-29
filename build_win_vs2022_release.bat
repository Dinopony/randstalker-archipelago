git submodule update --init --recursive

cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -B "./build_release_win64" -G "Visual Studio 17 2022" -A x64
cmake --build ./build_release_win64 --config Release
xcopy /S /Y /E "release_package" "build_release_win64/Release"
del ".\build_release_win64\Release\seeds\.gitignore"

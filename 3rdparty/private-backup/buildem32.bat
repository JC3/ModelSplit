:: This is an example file to generate binaries using Windows Operating System
:: This script is configured to be executed from the source directory

:: Compiled binaries will be placed in BINARIES_DIR\code\CONFIG

:: NOTE
:: The build process will generate a config.h file that is placed in BINARIES_DIR\include
:: This file must be merged with SOURCE_DIR\include
:: You should write yourself a script that copies the files where you want them.
:: Also see: https://github.com/assimp/assimp/pull/2646

SET SOURCE_DIR=.

:: For generators see "cmake --help"
SET GENERATOR=Visual Studio 16 2019

rem SET BINARIES_DIR="./BINARIES/Win32"
rem cmake CMakeLists.txt -G "%GENERATOR%" -S %SOURCE_DIR% -B %BINARIES_DIR%
rem cmake --build %BINARIES_DIR% --config release

SET BINARIES_DIR="./BINARIES/x86"
echo "--------------------- generate"
cmake CMakeLists.txt -G "%GENERATOR%" -A Win32 -S %SOURCE_DIR% -B %BINARIES_DIR%
echo "--------------------- build"
cmake --build %BINARIES_DIR% --config release
cmake --build %BINARIES_DIR% --config debug

PAUSE

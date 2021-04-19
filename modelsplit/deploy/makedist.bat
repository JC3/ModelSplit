cp ..\release\modelsplit.exe .
cp ..\LICENSE .
cp ..\README.md .
cp ..\..\3rdparty\assimp\Windows-x86_64-release\bin\*.dll .
cp ..\..\3rdparty\assimp\LICENSE LICENSE-ASSIMP
cp ..\..\3rdparty\assimp\CREDITS CREDITS-ASSIMP
windeployqt modelsplit.exe --no-quick-import --no-system-d3d-compiler --no-virtualkeyboard --no-webkit2 --no-angle --no-opengl-sw

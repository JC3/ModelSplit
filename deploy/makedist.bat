cp ..\release\modelsplit.exe .
cp ..\LICENSE .
cp ..\README.md .
cp ..\3rdparty\assimp-5.0.1\Windows-x86_64-release\bin\*.dll .
windeployqt modelsplit.exe --no-quick-import --no-system-d3d-compiler --no-virtualkeyboard --no-webkit2 --no-angle --no-opengl-sw

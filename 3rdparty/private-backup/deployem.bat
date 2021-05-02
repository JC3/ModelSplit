SET BUILD_DIR=".\BINARIES"
SET DEPLOY_DIR="C:\Users\Jason\Projects\Qt\modelsplit\3rdparty\assimp"

MKDIR "%DEPLOY_DIR%"
XCOPY /Y /F .\CREDITS "%DEPLOY_DIR%\"
XCOPY /Y /F .\LICENSE "%DEPLOY_DIR%\"

MKDIR "%DEPLOY_DIR%\common\include"
XCOPY /Y /E /F .\include "%DEPLOY_DIR%\common\include"
DEL "%DEPLOY_DIR%\common\include\assimp\.editorconfig"


SET P_BUILD_DIR="%BUILD_DIR%\x64"

SET P_DEPLOY_DIR="%DEPLOY_DIR%\Windows-x86_64-release"
MKDIR "%P_DEPLOY_DIR%\include"
MKDIR "%P_DEPLOY_DIR%\bin"
XCOPY /Y /E /F "%P_BUILD_DIR%\include" "%P_DEPLOY_DIR%\include"
XCOPY /Y /F "%P_BUILD_DIR%\lib\Release\*" "%P_DEPLOY_DIR%\bin\"
XCOPY /Y /F "%P_BUILD_DIR%\bin\Release\*.dll" "%P_DEPLOY_DIR%\bin\"

SET P_DEPLOY_DIR="%DEPLOY_DIR%\Windows-x86_64-debug"
MKDIR "%P_DEPLOY_DIR%\include"
MKDIR "%P_DEPLOY_DIR%\bin"
XCOPY /Y /E /F "%P_BUILD_DIR%\include" "%P_DEPLOY_DIR%\include"
XCOPY /Y /F "%P_BUILD_DIR%\lib\Debug\*" "%P_DEPLOY_DIR%\bin\"
XCOPY /Y /F "%P_BUILD_DIR%\bin\Debug\assimp-vc142-*" "%P_DEPLOY_DIR%\bin\"
DEL "%P_DEPLOY_DIR%\bin\*.ilk"


SET P_BUILD_DIR="%BUILD_DIR%\x86"

SET P_DEPLOY_DIR="%DEPLOY_DIR%\Windows-x86-release"
MKDIR "%P_DEPLOY_DIR%\include"
MKDIR "%P_DEPLOY_DIR%\bin"
XCOPY /Y /E /F "%P_BUILD_DIR%\include" "%P_DEPLOY_DIR%\include"
XCOPY /Y /F "%P_BUILD_DIR%\lib\Release\*" "%P_DEPLOY_DIR%\bin\"
XCOPY /Y /F "%P_BUILD_DIR%\bin\Release\*.dll" "%P_DEPLOY_DIR%\bin\"

SET P_DEPLOY_DIR="%DEPLOY_DIR%\Windows-x86-debug"
MKDIR "%P_DEPLOY_DIR%\include"
MKDIR "%P_DEPLOY_DIR%\bin"
XCOPY /Y /E /F "%P_BUILD_DIR%\include" "%P_DEPLOY_DIR%\include"
XCOPY /Y /F "%P_BUILD_DIR%\lib\Debug\*" "%P_DEPLOY_DIR%\bin\"
XCOPY /Y /F "%P_BUILD_DIR%\bin\Debug\assimp-vc142-*" "%P_DEPLOY_DIR%\bin\"
DEL "%P_DEPLOY_DIR%\bin\*.ilk"

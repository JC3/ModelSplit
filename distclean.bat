del .qmake.stash
del Makefile Makefile.*
cd %~dp0/generate_test_models && call distclean.bat
cd %~dp0/modelsplit && call distclean.bat

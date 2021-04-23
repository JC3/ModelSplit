del .qmake.stash
del Makefile Makefile.*
cd %~dp0/glb_import_assertion_failure && call distclean.bat
cd %~dp0/x3d_importer_fallback && call distclean.bat
cd %~dp0/decomposition_check && call distclean.bat
cd %~dp0/assimp_rotation_tests && call distclean.bat
cd %~dp0/test_exporter_crashes && call distclean.bat

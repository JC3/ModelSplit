rmdir /S /Q debug
rmdir /S /Q release
rmdir /S /Q .qm
del .qmake.stash
del Makefile Makefile.*
del *.qm
del modelsplit_resource.rc
del splitme*-????.* "splitme*-???? (*).*"


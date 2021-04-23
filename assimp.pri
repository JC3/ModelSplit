CONFIG(debug, debug|release): BUILD_TYPE=debug
else: BUILD_TYPE=release
CONFIG(debug, debug|release): LIB_SUFFIX=d

EXTLIB_PATH = $$PWD/3rdparty
ASSIMP_PATH = $$EXTLIB_PATH/assimp
ASSIMP_ARCH_PATH = $$ASSIMP_PATH/$${QMAKE_HOST.os}-$${QMAKE_HOST.arch}-$$BUILD_TYPE

win32: LIBS += -L$$ASSIMP_ARCH_PATH/bin/ -lassimp-vc142-mt$$LIB_SUFFIX
win32: INCLUDEPATH += $$ASSIMP_ARCH_PATH/include
win32: DEPENDPATH += $$ASSIMP_ARCH_PATH/include

INCLUDEPATH += $$ASSIMP_PATH/common/include
DEPENDPATH += $$ASSIMP_PATH/common/include

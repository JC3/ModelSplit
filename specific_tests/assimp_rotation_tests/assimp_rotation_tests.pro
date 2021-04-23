TEMPLATE = app
CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG -= qt

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        util.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

CONFIG(debug, debug|release): BUILD_TYPE=debug
else: BUILD_TYPE=release
CONFIG(debug, debug|release): LIB_SUFFIX=d

EXTLIB_PATH = $$PWD/../../3rdparty
ASSIMP_PATH = $$EXTLIB_PATH/assimp
ASSIMP_ARCH_PATH = $$ASSIMP_PATH/$${QMAKE_HOST.os}-$${QMAKE_HOST.arch}-$$BUILD_TYPE

win32: LIBS += -L$$ASSIMP_ARCH_PATH/bin/ -lassimp-vc142-mt$$LIB_SUFFIX
win32: INCLUDEPATH += $$ASSIMP_ARCH_PATH/include
win32: DEPENDPATH += $$ASSIMP_ARCH_PATH/include

INCLUDEPATH += $$ASSIMP_PATH/common/include
DEPENDPATH += $$ASSIMP_PATH/common/include
win32-msvc*:QMAKE_CXXFLAGS += /FC

HEADERS += \
    util.hh

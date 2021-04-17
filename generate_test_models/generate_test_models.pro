TEMPLATE = app
CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG -= qt

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

EXTLIB_PATH = $$PWD/../3rdparty
ASSIMP_ARCH_PATH = $$EXTLIB_PATH/assimp-5.0.1/$${QMAKE_HOST.os}-$${QMAKE_HOST.arch}-release

win32: LIBS += -L$$ASSIMP_ARCH_PATH/bin/ -lassimp-vc142-mt
win32: INCLUDEPATH += $$ASSIMP_ARCH_PATH/include
win32: DEPENDPATH += $$ASSIMP_ARCH_PATH/include

INCLUDEPATH += $$EXTLIB_PATH/assimp-5.0.1/common/include
DEPENDPATH += $$EXTLIB_PATH/3rdparty/assimp-5.0.1/common/include
win32-msvc*:QMAKE_CXXFLAGS += /FC
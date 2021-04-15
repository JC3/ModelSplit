VERSION = 1.0.0.1

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp

HEADERS +=

FORMS +=

DISTFILES += \
    README.md

TRANSLATIONS += modelsplit_en.ts modelsplit_es.ts
CONFIG += lrelease embed_translations

# Platorm stuff
# todo: Incomplete; windows msvc 32- and 64-bit only for now

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

ASSIMP_ARCH_PATH = $$PWD/3rdparty/assimp-5.0.1/$${QMAKE_HOST.os}-$${QMAKE_HOST.arch}-release

win32: RC_ICONS = icon.ico
win32: LIBS += -L$$ASSIMP_ARCH_PATH/bin/ -lassimp-vc142-mt
win32: INCLUDEPATH += $$ASSIMP_ARCH_PATH/include
win32: DEPENDPATH += $$ASSIMP_ARCH_PATH/include

INCLUDEPATH += $$PWD/3rdparty/assimp-5.0.1/common/include
DEPENDPATH += $$PWD/3rdparty/assimp-5.0.1/common/include

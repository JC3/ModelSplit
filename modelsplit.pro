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

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: RC_ICONS = icon.ico
win32: LIBS += -L$$PWD/3rdparty/assimp-5.0.1/win64-release/bin/ -lassimp-vc142-mt
win32: INCLUDEPATH += $$PWD/3rdparty/assimp-5.0.1/win64-release/include
win32: DEPENDPATH += $$PWD/3rdparty/assimp-5.0.1/win64-release/include

INCLUDEPATH += $$PWD/3rdparty/assimp-5.0.1/common/include
DEPENDPATH += $$PWD/3rdparty/assimp-5.0.1/common/include

RESOURCES +=

TRANSLATIONS += modelsplit_en.ts modelsplit_es.ts


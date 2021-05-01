QT -= gui
QT += xml
CONFIG += c++17 console
CONFIG -= app_bundle
SOURCES += \
        main.cpp

include($$MODELSPLIT_ROOT/assimp.pri)

DISTFILES += \
    report.css

RESOURCES += \
    resources.qrc

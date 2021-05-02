#QT -= gui
QT += xml webenginewidgets
CONFIG += c++17 console
CONFIG -= app_bundle
SOURCES += \
        main.cpp

include($$MODELSPLIT_ROOT/assimp.pri)

DISTFILES += \
    report.css \
    report.js

RESOURCES += \
    resources.qrc

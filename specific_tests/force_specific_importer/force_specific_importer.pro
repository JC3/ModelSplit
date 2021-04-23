TEMPLATE = app
CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG -= qt
SOURCES += main.cpp \
    util.cpp

include($$MODELSPLIT_ROOT/assimp.pri)

HEADERS += \
    util.hh

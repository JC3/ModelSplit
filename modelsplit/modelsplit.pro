# --------------------------------------------------------------------------
#
# MIT License
#
# Copyright (c) 2021 Jason C
#
# Permission is  hereby granted,  free of charge,  to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without  restriction, including without limitation
# the rights to use,  copy, modify, merge, publish,  distribute, sublicense,
# and/or sell  copies of  the Software,  and to permit  persons to  whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED,  INCLUDING BUT NOT LIMITED TO  THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A  PARTICULAR PURPOSE  AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY,  WHETHER IN AN ACTION OF CONTRACT,  TORT OR OTHERWISE,  ARISING
# FROM,  OUT OF  OR IN  CONNECTION WITH  THE SOFTWARE  OR THE  USE OR  OTHER
# DEALINGS IN THE SOFTWARE.
#
# --------------------------------------------------------------------------

VERSION = 1.0.0.4

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
    Doxyfile \
    README.md \
    deploy/installer.nsi

TRANSLATIONS += modelsplit_en.ts modelsplit_es.ts
CONFIG += lrelease embed_translations

# Platorm stuff
# todo: Incomplete; windows msvc 32- and 64-bit only for now

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

EXTLIB_PATH = $$PWD/../3rdparty
ASSIMP_ARCH_PATH = $$EXTLIB_PATH/assimp-5.0.1/$${QMAKE_HOST.os}-$${QMAKE_HOST.arch}-release

win32: RC_ICONS = icon.ico
win32: LIBS += -L$$ASSIMP_ARCH_PATH/bin/ -lassimp-vc142-mt
win32: INCLUDEPATH += $$ASSIMP_ARCH_PATH/include
win32: DEPENDPATH += $$ASSIMP_ARCH_PATH/include

INCLUDEPATH += $$EXTLIB_PATH/assimp-5.0.1/common/include
DEPENDPATH += $$EXTLIB_PATH/3rdparty/assimp-5.0.1/common/include
win32-msvc*:QMAKE_CXXFLAGS += /FC

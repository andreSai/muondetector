#-------------------------------------------------
#
# Project created by QtCreator 2018-10-26T18:37:59
#
#-------------------------------------------------

QT       += network

QT       -= gui
VERSION = 1.0.3
CONFIG += c++11
CONFIG += warn_on
CONFIG += release

TARGET = muondetector-shared
TEMPLATE = lib

DEFINES += MUONDETECTOR_SHARED_LIBRARY

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

DESTDIR = bin
UI_DIR = build/ui
MOC_DIR = build/moc
RCC_DIR = build/rcc
unix:OBJECTS_DIR = build/o/unix
win32:OBJECTS_DIR = build/o/win32
macx:OBJECTS_DIR = build/o/mac

SOURCES += \
    src/tcpconnection.cpp \
    src/tcpmessage.cpp \
    #src/muondetector_structs.cpp

HEADERS += \
    #src/geodeticpos.h \
    src/gpio_pin_definitions.h \
    src/histogram.h \
    src/tcpconnection.h \
    src/tcpmessage.h \
    src/tcpmessage_keys.h \
    src/ublox_messages.h \
    src/ubx_msg_key_name_map.h \
    src/muondetector_shared_global.h \
    #src/calib_struct.h \
    #src/gnsssatellite.h \
    src/ublox_structs.h \
    src/muondetector_structs.h

#-------------------------------------------------
#
# Project created by QtCreator 2017-07-13T11:16:51
#
#-------------------------------------------------

QT       += core gui serialport
QT       -= opengl
DEFINES  += QT_MESSAGELOGCONTEXT

CONFIG   += warn_on

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = updater71
TEMPLATE = app

#include(../../qesp/github/qextserialport/src/qextserialport.pri)

SOURCES += main.cpp\
        mainwindow.cpp \
    hex.cpp \
    bytearray.cpp \
    mdrloader.cpp

HEADERS  += mainwindow.h \
    hex.h \
    bytearray.h \
    mdrloader.h \
    mcudescriptor.h

FORMS    += mainwindow.ui

RESOURCES += \
    mdrloader.qrc

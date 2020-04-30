#-------------------------------------------------
#
# Project created by QtCreator 2014-04-12T08:09:29
#
#-------------------------------------------------

QT       += core network

TARGET = qatemuploader
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

macx {
    INCLUDEPATH += /usr/local/include
    DEPENDPATH += /usr/local/include
    LIBS += -L/usr/local/lib
}

LIBS += -lqatemcontrol

SOURCES += main.cpp \
    qatemuploader.cpp

HEADERS += \
    qatemuploader.h

#-------------------------------------------------
#
# Project created by QtCreator 2013-10-06T09:35:23
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qatemswitcher
TEMPLATE = app

macx {
    INCLUDEPATH += /usr/local/include
    DEPENDPATH += /usr/local/include
    LIBS += -L/usr/local/lib
}

LIBS += -lqatemcontrol


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui

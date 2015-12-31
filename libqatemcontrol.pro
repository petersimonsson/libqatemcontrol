QT += network

TARGET = qatemcontrol
TEMPLATE = lib

DEFINES += LIBQATEMCONTROL_LIBRARY

SOURCES += qatemconnection.cpp \
    qatemmixeffect.cpp \
    qatemcameracontrol.cpp \
    qatemdownstreamkey.cpp

HEADERS += qatemconnection.h \
        libqatemcontrol_global.h \
    qupstreamkeysettings.h \
    qatemmixeffect.h \
    qatemtypes.h \
    qatemcameracontrol.h \
    qatemdownstreamkey.h

unix {
    target.path = /usr/lib
    INSTALLS += target

    header_files.files = $$HEADERS
    header_files.path = /usr/include
    INSTALLS += header_files
}

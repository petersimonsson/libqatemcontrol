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

macx {
    target.path = /usr/local/lib
    header_files.path = /usr/local/include
}
unix:!macx {
    target.path = /usr/lib
    header_files.path = /usr/include
}
unix {
    INSTALLS += target

    header_files.files = $$HEADERS
    INSTALLS += header_files
}

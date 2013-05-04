CONFIG += qt cross-win32
CONFIG -= debug_and_release
CONFIG( debug, debug|release )  {
   CONFIG -= release
} else {
   CONFIG -= debug
   CONFIG += release
}

bins.path = /$(DESTDIR)@sbindir@
bins.files = bareos-tray-monitor
confs.path = /$(DESTDIR)@sysconfdir@
confs.commands = ./install_conf_file

TEMPLATE     = app
TARGET       = bareos-tray-monitor
DEPENDPATH  += .
INCLUDEPATH += ../.. ../../include ../include ../compat/include ../../qt-console/tray-monitor ../qt-console
VPATH        = ../../qt-console/tray-monitor ../../qt-console

LIBS        += -mwindows ../lib/libbareos.a ../lib/libbareos.dll ../findlib/libbareosfind.dll -lwsock32
DEFINES     += HAVE_WIN32 HAVE_MINGW

RESOURCES    = ../main.qrc
MOC_DIR      = moc
OBJECTS_DIR  = obj
UI_DIR       = ui

# Main directory
HEADERS += tray_conf.h  tray-monitor.h  tray-ui.h
SOURCES += authenticate.cpp  tray_conf.cpp  tray-monitor.cpp

FORMS += ../run/run.ui

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
confs.path = /$(DESTDIR)@confdir@
confs.commands = ./install_conf_file

TEMPLATE     = app
TARGET       = bareos-tray-monitor
PRE_TARGETDEPS += ../lib/libbareos.a ../lib/libbareos.dll
DEPENDPATH  += .
INCLUDEPATH += ../.. ../../include ../include ../compat/include \
               ../../qt-tray-monitor
VPATH        = ../../qt-tray-monitor

LIBS        += -mwindows ../lib/libbareos.a ../lib/libbareos.dll libjansson.dll -lwsock32
DEFINES     += HAVE_WIN32 HAVE_MINGW _STAT_DEFINED=1

RESOURCES    = main.qrc
MOC_DIR      = moc
OBJECTS_DIR  = obj
UI_DIR       = ui

# Main directory
HEADERS += tray_conf.h tray-monitor.h traymenu.h \
           systemtrayicon.h mainwindow.h authenticate.h monitoritem.h \
           monitoritemthread.h monitortab.h
SOURCES += authenticate.cpp tray_conf.cpp tray-monitor.cpp \
           traymenu.cpp systemtrayicon.cpp mainwindow.cpp monitoritem.cpp \
           monitoritemthread.cpp

FORMS += mainwindow.ui

# Application icon and info
win32:RC_FILE = traymon.rc


CONFIG += qt debug

TEMPLATE = app
TARGET = main
DEPENDPATH += .
INCLUDEPATH += ..
MOC_DIR = moc
OBJECTS_DIR = obj
UI_DIR = ui

# Main window
#RESOURCES = dockwidgets.qrc
HEADERS += mainwindow.h
SOURCES += mainwindow.cpp main.cpp


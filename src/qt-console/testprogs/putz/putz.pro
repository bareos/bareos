CONFIG += qt debug

TEMPLATE = app
TARGET = putz
DEPENDPATH += .
INCLUDEPATH += ..
MOC_DIR = moc
OBJECTS_DIR = obj
UI_DIR = ui

# Main window
FORMS += putz.ui

HEADERS += putz.h
SOURCES += putz.cpp main.cpp

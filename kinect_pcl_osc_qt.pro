######################################################################
# Automatically generated by qmake (2.01a) Mon Mar 3 14:52:34 2014
######################################################################

TEMPLATE = app
TARGET = 
DEPENDPATH += . KPO_Base KPO_Curses KPO_GUI
INCLUDEPATH += . KPO_Base KPO_Curses KPO_GUI

# Input
HEADERS += KPO_Base/BlobFinder.h \
           KPO_Base/kpo_base.h \
           KPO_Base/KPO_Base_global.h \
           KPO_Base/kpo_types.h \
           KPO_Base/kpoAnalyzerThread.h \
           KPO_Base/kpoBaseApp.h \
           KPO_Base/kpoMatcherThread.h \
           KPO_Base/kpoOscSender.h \
           KPO_Curses/kpoAppCurses.h \
           KPO_GUI/BlobRenderer.h \
           KPO_GUI/kpoAppGui.h
FORMS += KPO_GUI/kpoAppGui.ui
SOURCES += KPO_Base/kpo_base.cpp \
           KPO_Base/kpoAnalyzerThread.cpp \
           KPO_Base/kpoBaseApp.cpp \
           KPO_Base/kpoMatcherThread.cpp \
           KPO_Base/kpoOscSender.cpp \
           KPO_Curses/kpoAppCurses.cpp \
           KPO_Curses/main.cpp \
           KPO_GUI/BlobRenderer.cpp \
           KPO_GUI/kpoAppGui.cpp

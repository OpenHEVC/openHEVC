#-------------------------------------------------
#
# Project created by QtCreator 2017-04-26T14:28:32
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ohvisualizer
TEMPLATE = app

DEFINES += "OHVZ"

LIBS += -L../build -lLibOpenHevcWrapper

INCLUDEPATH += ..\
        ../gpac/modules/openhevc_dec

SOURCES += main.cpp\
        mainohvisualizer.cpp\
        ../ohplay_utils/ohtimer_sys.c \
    ohvzmodel.cpp \
    ohvzframeview.cpp \
    ohvzglframeview.cpp \
    ohvznavigationview.cpp

HEADERS  += mainohvisualizer.h \
    ohvzmodel.h \
    ohvzframeview.h \
    ohvzglframeview.h \
    ohvznavigationview.h
#    hevc_struct.h

FORMS    += mainohvisualizer.ui

#-------------------------------------------------
#
# Project created by QtCreator 2018-11-19T18:14:53
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = petcol_test
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# petcol lives in the repo's canonical petcol/ directory (one source of truth,
# shared with the firmware and the Python host tool).
PETCOL = $$PWD/../../petcol
INCLUDEPATH += $$PETCOL

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        $$PETCOL/petprotocol.cpp

HEADERS += \
        mainwindow.h \
        $$PETCOL/petprotocol.h \
        $$PETCOL/buf.h

FORMS += \
        mainwindow.ui

#-------------------------------------------------
#
# Project created by QtCreator 2019-10-07T14:22:31
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = fdb
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS "LOGLEVEL_FACTINJECT=12"

INCLUDEPATH += ../src

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

#CONFIG +=
QMAKE_CXXFLAGS += -std=c++17


SOURCES += \
        ../src/Inject.cpp \
        ../src/lua_imports.cpp \
        ../src/util/log.cpp \
        FactorioProcess.cpp \
        gui/QOptionsDock.cpp \
        gui/QTerminalDock.cpp \
        gui/QTerminalIOEvent.cpp \
        main.cpp \
        mainwindow.cpp \
        mainwindow_actions.cpp \
        util/GDBProcess.cpp

HEADERS += \
        ../src/Inject.hpp \
        ../src/lua_imports.hpp \
        ../src/util/errno_exception.hpp \
        ../src/util/exceptions.hpp \
        ../src/util/log.hpp \
        ../src/util/safe_deque.hpp \
        FactorioProcess.h \
        gui/QOptionsDock.h \
        gui/QTerminalDock.h \
        gui/QTerminalIOEvent.h \
        mainwindow.h \
        util/GDBProcess.h

LIBS += -lutil

FORMS += \
        gui/settings.ui \
        mainwindow.ui

# Default rules for deployment.
debug {
  DEFINES += DEBUG
}

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    fdbresources.qrc

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
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += ../src

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

#CONFIG +=
QMAKE_CXXFLAGS += -std=c++17 -Wno-format-security -O0


SOURCES += \
        ../src/Inject.cpp \
        ../src/io/EPollDispatcher.cpp \
        ../src/io/autoclosing_fd.cpp \
        ../src/util/log.cpp \
        GdbMI.cpp \
        TraceeProcess.cpp \
        fdbapp.cpp \
        gui/QMIDock.cpp \
        gui/QOptionsDock.cpp \
        gui/QTerminalDock.cpp \
        io/AsyncPty.cpp \
        main.cpp \
        util/GDBProcess.cpp \
        util/QTextEvent.cpp

HEADERS += \
        ../src/GdbMI.h \
        ../src/Inject.hpp \
        ../src/io/EPollDispatcher.h \
        ../src/io/autoclosing_fd.h \
        ../src/ipc/mqueue.hpp \
        ../src/util/co_work_queue.hpp \
        ../src/util/errno_exception.hpp \
        ../src/util/exceptions.hpp \
        ../src/util/log.hpp \
        ../src/util/safe_deque.hpp \
        ../src/util/text_response.hpp \
        GdbMI.h \
        TraceeProcess.h \
        fdbapp.h \
        gui/QMIDock.h \
        gui/QOptionsDock.h \
        gui/QTerminalDock.h \
        gui/common.h \
        io/AsyncPty.h \
        util/GDBProcess.h \
        util/QTextEvent.h

LIBS += -lutil -lboost_coroutine -lboost_context

FORMS += \
        gui/settings.ui \
        mainwindow.ui

# Default rules for deployment.
debug {
  DEFINES += DEBUG
  DEFINES += LOGLEVEL=100
}

SOURCES_GENERIC_NEEDLOGGER =
needlogger.name = needlogger
needlogger.input = SOURCES_GENERIC_NEEDLOGGER
needlogger.dependency_type = TYPE_C
needlogger.variable_out = OBJECTS
needlogger.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
needlogger.commands = $${QMAKE_CXX} $(CXXFLAGS) -O0 $(INCPATH) -include ../../fdb_logger.hpp -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT} # Note the -O0
QMAKE_EXTRA_COMPILERS += needlogger

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    fdbresources.qrc

DISTFILES += \
    gdbmi.y \
    resources/icons/gdb-button.svg

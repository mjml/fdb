TEMPLATE = lib
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    ../src/Inject.cpp \
    ../src/io/EPollDispatcher.cpp \
    ../src/util/log.cpp \
    lua_imports.cpp \
    fdbstub.cpp

HEADERS += \
    ../ext/lua-5.2.1/src/lapi.h \
    ../ext/lua-5.2.1/src/lauxlib.h \
    ../ext/lua-5.2.1/src/lcode.h \
    ../ext/lua-5.2.1/src/lctype.h \
    ../ext/lua-5.2.1/src/ldebug.h \
    ../ext/lua-5.2.1/src/ldo.h \
    ../ext/lua-5.2.1/src/lfunc.h \
    ../ext/lua-5.2.1/src/lgc.h \
    ../ext/lua-5.2.1/src/llex.h \
    ../ext/lua-5.2.1/src/llimits.h \
    ../ext/lua-5.2.1/src/lmem.h \
    ../ext/lua-5.2.1/src/lobject.h \
    ../ext/lua-5.2.1/src/lopcodes.h \
    ../ext/lua-5.2.1/src/lparser.h \
    ../ext/lua-5.2.1/src/lstate.h \
    ../ext/lua-5.2.1/src/lstring.h \
    ../ext/lua-5.2.1/src/ltable.h \
    ../ext/lua-5.2.1/src/ltm.h \
    ../ext/lua-5.2.1/src/lua.h \
    ../ext/lua-5.2.1/src/luaconf.h \
    ../ext/lua-5.2.1/src/lualib.h \
    ../ext/lua-5.2.1/src/lundump.h \
    ../ext/lua-5.2.1/src/lvm.h \
    ../ext/lua-5.2.1/src/lzio.h \
    ../src/fdbstub_logger.hpp \
    ../src/io/EPollDispatcher.hpp \
    ../src/util/co_work_queue.hpp \
    ../src/util/errno_exception.hpp \
    ../src/util/exceptions.hpp \
    ../src/util/log.hpp \
    ../src/util/microsleep.h \
    ../src/util/safe_deque.hpp \
    ../src/util/text_response.hpp \
    lua_imports.hpp \
    fdbstub.h \
    fdbstub_logger.hpp

LIBS += -lpthread

INCLUDEPATH += $$PWD/../ext/lua-5.2.1/src $$PWD/../src
DEPENDPATH += $$PWD/../ext/lua-5.2.1/src

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
needlogger.commands = $${QMAKE_CXX} $(CXXFLAGS) $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT} # Note the -O0
QMAKE_EXTRA_COMPILERS += needlogger

#unix:!macx: LIBS += $$PWD/../ext/lua-5.2.1/src/liblua.a
unix: QMAKE_LFLAGS += -rdynamic

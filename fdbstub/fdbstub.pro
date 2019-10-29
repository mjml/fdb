TEMPLATE = lib
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
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
    fdbstub.h

INCLUDEPATH += $$PWD/../ext/lua-5.2.1/src
DEPENDPATH += $$PWD/../ext/lua-5.2.1/src


unix:!macx: LIBS += $$PWD/../ext/lua-5.2.1/src/liblua.a
unix: QMAKE_LFLAGS += -rdynamic

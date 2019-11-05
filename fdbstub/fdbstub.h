#ifndef FDBSTUB_H
#define FDBSTUB_H

#include <QtCore/QtGlobal>
#include <lua.h>

#define FDBSTUB_EXPORT Q_DECL_EXPORT

FDBSTUB_EXPORT int stub_init ();
FDBSTUB_EXPORT int stub_finalize ();

FDBSTUB_EXPORT int register_lua_state (lua_State* state);

#endif // FDBSTUB_H

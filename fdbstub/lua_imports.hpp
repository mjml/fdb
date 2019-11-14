#pragma once
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#define LUA_IDSIZE      60

#ifndef LUA_IMPORTS_CPP
#define EXTERN   extern
#else
#define EXTERN
#endif

namespace lua_imports
{

typedef int64_t diffptr_t;

// Core types
typedef void lua_State;
typedef int (*lua_CFunction) (lua_State*);
typedef void* (*lua_Alloc) (void* ud, void* ptr, size_t osize, size_t nsize);
typedef diffptr_t lua_Integer; // probably int64_t
typedef double lua_Number;
typedef const char* (*lua_Reader) (lua_State* L, void* data, size_t* size);
typedef unsigned long lua_Unsigned;
typedef int (*lua_Writer) (lua_State* L, const void* p, size_t sz, void* ud);

// defines and constants
/*
** basic types
*/
#define LUA_TNONE		(-1)

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8

#define LUA_NUMTAGS		9


// lua_absindex
EXTERN void (*lua_arith) (lua_State* L, int op);
EXTERN lua_CFunction (*lua_atpanic) (lua_State* L, lua_CFunction panicf);
//void (*lua_call) (lua_State* L, int nargs, int nresults); // not compiled in!
EXTERN void (*lua_callk) (lua_State* L, int nargs, int nresults, int ctx, lua_CFunction k);
EXTERN int (*lua_checkstack) (lua_State* L, int extra);
EXTERN void (*lua_close) (lua_State* L);
EXTERN int  (*lua_compare) (lua_State* L, int index1, int index2, int op);
EXTERN void (*lua_concat) (lua_State* L, int n);
EXTERN void (*lua_copy) (lua_State* L, int fromidx, int toidx);
EXTERN void (*lua_createtable) (lua_State* L, int narr, int nrec);
EXTERN int (*lua_dump) (lua_State* L, lua_Writer writer, void* data);
EXTERN int (*lua_error) (lua_State* L);
EXTERN int (*lua_gc) (lua_State* L, int what, int data);
EXTERN lua_Alloc (*lua_getallocf) (lua_State* L, void** ud);
EXTERN int (*lua_getctx) (lua_State* L, int* ctx);
EXTERN void (*lua_getfield) (lua_State* L, int index, const char* k);
EXTERN void (*lua_getglobal) (lua_State* L, const char* name);
EXTERN int (*lua_getmetatable) (lua_State* L, int index);
EXTERN void (*lua_gettable) (lua_State* L, int index);
EXTERN int (*lua_gettop) (lua_State* L);
EXTERN void (*lua_getuservalue) (lua_State* L, int index);
EXTERN void (*lua_insert) (lua_State* L, int index);
//int  (*lua_isboolean) (lua_State* L, int index); // not compiled in!
EXTERN int (*lua_iscfunction) (lua_State* L, int index);
EXTERN int (*lua_isfunction) (lua_State* L, int index);
//int (*lua_islightuserdata) (lua_State* L, int index); // not compiled in!
//int lua_isnil (lua_State* L, int index);   // not compiled in!
//int lua_isnone (lua_State* L, int index);  // not compiled in!
//int lua_isnoneornil (lua_State* L, int index); // not compiled in!
EXTERN int (*lua_isnumber) (lua_State* L, int index);
EXTERN int (*lua_isstring) (lua_State* L, int index);
//int lua_istable (lua_State* L, int index);  // not compiled in! (wow)
//int (*lua_isthread) (lua_State* L, int index); // not compiled in
EXTERN int (*lua_isuserdata) (lua_State* L, int index);
EXTERN int (*lua_len) (lua_State* L, int index);
EXTERN int (*lua_load) (lua_State* L,
                        lua_Reader reader,
                        void* data,
                        const char* source,
                        const char* mode);
EXTERN int (*lua_newstate) (lua_Alloc f, void* ud);
//void (*lua_newtable) (lua_State* L);
EXTERN lua_State* (*lua_newthread) (lua_State* L);
EXTERN void* (*lua_newuserdata) (lua_State* L, size_t size);
EXTERN int (*lua_next) (lua_State* L, int index);
//int lua_pcall (lua_State* L, int nargs, int nresults, int msgh);
EXTERN int (*lua_pcallk) (lua_State* L, int nargs, int nresults, int errfunc, int ctx, lua_CFunction k);
//void (*lua_pop) (lua_State* L, int n);
EXTERN void (*lua_pushboolean) (lua_State* L, int b);
EXTERN void (*lua_pushcclosure) (lua_State* L, lua_CFunction fn, int n);
//void lua_pushcfunction (lua_State* L, lua_CFunction f); // not compiled in (yikes)
EXTERN void (*lua_pushfstring) (lua_State *L, const char* fmt, ...); // uses sprintf yay!
//void lua_pushglobaltable (lua_State* L); // not compiled in!
EXTERN void (*lua_pushinteger) (lua_State* L, lua_Integer n);
EXTERN void (*lua_pushlightuserdata) (lua_State* L, void* p);
//const char* lua_pushliteral (lua_State* L, const char* s);  // not compiled in
EXTERN const char* (*lua_pushlstring) (lua_State* L, const char* s, size_t len);
EXTERN void (*lua_pushnil) (lua_State* L);
EXTERN void (*lua_pushnumber) (lua_State* L, lua_Number n);
EXTERN const char* (*lua_pushstring) (lua_State* L, const char* s);
EXTERN int (*lua_pushthread) (lua_State* L);
EXTERN void (*lua_pushunsigned) (lua_State* L, lua_Unsigned n);
EXTERN void (*lua_pushvalue) (lua_State* L, int index);
EXTERN const char* (*lua_pushvfstring) (lua_State* L, const char* fmt, va_list argp);
EXTERN int (*lua_rawequal) (lua_State* L, int index1, int index2);
EXTERN void (*lua_rawget) (lua_State* L, int index);
EXTERN void (*lua_rawgeti) (lua_State* L, int index, int n);
EXTERN void (*lua_rawgetp) (lua_State* L, int index, const void* p);
EXTERN size_t (*lua_rawlen) (lua_State* L, int index);
EXTERN void (*lua_rawset) (lua_State* L, int index);
EXTERN void (*lua_rawseti) (lua_State* L, int index, int n);
EXTERN void (*lua_rawsetp) (lua_State* L, int index, const void* p);
EXTERN void (*lua_remove) (lua_State* L, int index);
EXTERN void (*lua_replace) (lua_State* L, int index);
//int (*lua_resume) (lua_State* L, lua_State* from, int nargs); // not compiled in
EXTERN void (*lua_setallocf) (lua_State* L, lua_Alloc f, void* ud);
EXTERN void (*lua_setfield) (lua_State* L, int index, const char* k);
EXTERN void (*lua_setglobal) (lua_State* L, const char* name);
EXTERN void (*lua_setmetatable) (lua_State* L, int index);
EXTERN void (*lua_settable) (lua_State* L, int index);
EXTERN void (*lua_settop) (lua_State* L, int index);
EXTERN void (*lua_setuservalue) (lua_State* L, int index);
EXTERN int (*lua_status) (lua_State* L);
// lua_tablesize wube-provided
// lua_tableresize wube-provided
EXTERN int (*lua_toboolean) (lua_State* L);
EXTERN lua_CFunction (*lua_tocfunction) (lua_State* L, int index);
//lua_Integer lua_tointeger (lua_State* L, int index); // not compiled in
EXTERN lua_Integer (*lua_tointegerx) (lua_State* L, int index, int* isnum);
EXTERN const char* (*lua_tolstring) (lua_State* L, int index, size_t* len);
//lua_Number lua_tonumber (lua_State* L, int index); // not compiled in
EXTERN lua_Number  (*lua_tonumberx) (lua_State* L, int index, int* isnum);
EXTERN const void* (*lua_topointer) (lua_State* L, int index);
//const char* lua_tostring (lua_State* L, int index); // not compiled in
EXTERN lua_State* (*lua_tothread) (lua_State* L, int index);
//lua_Unsigned lua_tounsigned (lua_State* L, int index); // not compiled in
EXTERN lua_Unsigned (*lua_tounsignedx) (lua_State* L, int index, int* isnum);
EXTERN void* (*lua_touserdata) (lua_State* L, int index);
// lua_traceandabort wube-provided
EXTERN int (*lua_type) (lua_State* L, int index);
EXTERN const char* (*lua_typename) (lua_State* L, int tp);
EXTERN int (*lua_upvalueindex) (int i);
EXTERN const lua_Number* (*lua_version) (lua_State* L);
EXTERN void (*lua_xmove) (lua_State* from, lua_State* to, int n);
//int (*lua_yield) (lua_State* L, int nresults);
//int (*lua_yieldk) (lua_State* L, int nresults, int ctx, lua_CFunction k);


// Debug library
typedef struct lua_Debug {
  int event;
  const char *name;           /* (n) */
  const char *namewhat;       /* (n) */
  const char *what;           /* (S) */
  const char *source;         /* (S) */
  int currentline;            /* (l) */
  int linedefined;            /* (S) */
  int lastlinedefined;        /* (S) */
  unsigned char nups;         /* (u) number of upvalues */
  unsigned char nparams;      /* (u) number of parameters */
  char isvararg;              /* (u) */
  char istailcall;            /* (t) */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  //other fields
} lua_Debug;

typedef void (*lua_Hook) (lua_State* L, lua_Debug* ar);

EXTERN lua_Hook (*lua_gethook) (lua_State* L);
EXTERN int (*lua_gethookcount) (lua_State* L);
EXTERN int (*lua_gethookmask) (lua_State* L);
EXTERN int (*lua_getinfo) (lua_State* L, const char* what, lua_Debug* ar);
EXTERN const char* (*lua_getlocal) (lua_State* L);
EXTERN int (*lua_getstack) (lua_State* L);
EXTERN const char* (*lua_getupvalue) (lua_State* L, int funcindex, int n);
EXTERN int (*lua_sethook) (lua_State* L, lua_Hook f, int mask, int count);
EXTERN const char* (*lua_setlocal) (lua_State* L, lua_Debug* ar, int n);
EXTERN const char* (*lua_setupvalue) (lua_State* L, int funcindex, int n);
EXTERN void* (*lua_upvalueid) (lua_State* L, int funcindex, int n);
EXTERN void (*lua_upvaluejoin) (lua_State* L,
                                int funcindex1, int n1,
                                int funcindex2, int n2);

// The Auxiliary Library
typedef struct luaL_Buffer luaL_Buffer;
typedef struct luaL_Reg {
  const char *name;
  lua_CFunction func;
} luaL_Reg;

EXTERN void (*luaL_addlstring) (luaL_Buffer* B, const char* s, size_t l);
EXTERN void (*luaL_addstring) (luaL_Buffer* B, const char* s);
EXTERN void (*luaL_addvalue) (luaL_Buffer* B);
EXTERN int (*luaL_argerror) (lua_State* L, int arg, const char* extramsg);
EXTERN void (*luaL_buffinit) (luaL_Buffer* B);
EXTERN char* (*luaL_buffinitsize) (lua_State* L, luaL_Buffer* B, size_t sz);
EXTERN int (*luaL_callmeta) (lua_State* L, int obj, const char* e);
EXTERN int (*luaL_checkany) (lua_State* L, int arg);
EXTERN lua_Integer (*luaL_checkinteger) (lua_State* L, int arg);
EXTERN const char* (*luaL_checklstring) (lua_State* L, int arg, size_t* l);
EXTERN lua_Number (*luaL_checknumber) (lua_State* L, int arg, size_t *l);
EXTERN int (*luaL_checkoption) (lua_State* L, int arg, const char* def, const char* const lst[]);
EXTERN void (*luaL_checkstack) (lua_State* L, int sz, const char* msg);
EXTERN void (*luaL_checktype) (lua_State* L, int arg, int t);
EXTERN void* (*luaL_checkudata) (lua_State* L, int arg, const char* tname);
EXTERN lua_Unsigned (*luaL_checkunsigned) (lua_State* L, int arg);
EXTERN void (*luaL_checkversion_) (lua_State* L);
EXTERN int (*luaL_error) (lua_State* L, const char* fmt, ...);
EXTERN int (*luaL_execresult) (lua_State* L, int stat);
EXTERN int (*luaL_fileresult) (lua_State* L, int stat);
EXTERN int (*luaL_getmetafield) (lua_State* L, int obk, const char* e);
// void luaL_getmetatable (lua_State* L, const char* tname) // not compiled in!
EXTERN int (*luaL_getsubtable) (lua_State* L, int idx, const char* fname);
EXTERN const char* (*luaL_gsub) (lua_State* L, const char* s, const char* p, const char* r);
EXTERN int (*luaL_len) (lua_State* L, int index);
// int luaL_loadbuffer (lua_State* L, const char* buff, size_t sz, const char* name) // not compiled in!
EXTERN int (*luaL_loadbufferx) (lua_State* L, const char* buff, size_t sz, const char* name, const char* mode);
// int luaL_loadfile (lua_State* L, const char* filename) // not compiled in!
EXTERN int (*luaL_loadfilex) (lua_State* L, const char* filename, const char* mode);
EXTERN int (*luaL_loadstring) (lua_State* L, const char* s);
// void luaL_newlib (lua_State* L, const luaL_Reg* l) // not compiled in!
// void luaL_newlibtable (lua_State* L, const luaL_Reg l[]); // not compiled in!
EXTERN int (*luaL_newmetatable) (lua_State* L, const char* tname);
EXTERN lua_State* (*luaL_newstate) (void);
// luaL_openlib wube-provided, no spec
EXTERN void (*luaL_openlibs) (lua_State* L);
// int luaL_optint (lua_State* L, int arg, int d) // not compiled in!
EXTERN lua_Integer (*luaL_optinteger) (lua_State* L, int arg, lua_Integer d);
EXTERN const char* (*luaL_optlstring) (lua_State* L, int arg, const char* d, size_t* l);
EXTERN lua_Number (*luaL_optnumber) (lua_State* L, int arg, lua_Number d);
EXTERN lua_Unsigned (*luaL_optunsigned) (lua_State* L, int arg, lua_Unsigned u);
// char* lua_prepbuffer (luaL_Buffer* B) // not compiled in!
EXTERN char* (*luaL_prepbuffsize) (luaL_Buffer* B, size_t sz);
// luaL_pushmodule wube-provided, no spec
EXTERN void (*luaL_pushresult) (luaL_Buffer* B);
EXTERN void (*luaL_pushresultsize) (luaL_Buffer* B, size_t sz);
EXTERN int (*luaL_ref) (lua_State *L, int t);
EXTERN void (*luaL_requiref) (lua_State* L, const char* modname, lua_CFunction openf, int glb);
EXTERN void (*luaL_setfuncs) (lua_State* L, const luaL_Reg* l, int nup);
EXTERN void (*luaL_setmetatable) (lua_State* L, const char* tname);
EXTERN void* (*luaL_testudata) (lua_State* L, int arg, const char* tname);
EXTERN const char* (*luaL_tolstring) (lua_State* L, int idx, size_t* len);
EXTERN void (*luaL_traceback) (lua_State* L, lua_State* L1, const char* msg, int level);
// const char* luaL_typename (lua_State* L, int index) // not compiled in!
EXTERN void (*luaL_unref) (lua_State* L, int t, int ref);
EXTERN void (*luaL_where) (lua_State* L, int lvl);

#define lua_tonumber(L,i)	(*lua_tonumberx)(L,i,NULL)
#define lua_tointeger(L,i)	(*lua_tointegerx)(L,i,NULL)
#define lua_tounsigned(L,i)	(*lua_tounsignedx)(L,i,NULL)

#define lua_pop(L,n)		(*lua_settop)(L, -(n)-1)

#define lua_newtable(L)		(*lua_createtable)(L, 0, 0)

#define lua_register(L,n,f) ((*lua_pushcfunction)(L, (f)), (*lua_setglobal)(L, (n)))

#define lua_pushcfunction(L,f)	(*lua_pushcclosure)(L, (f), 0)

#define lua_isfunction(L,n)	((*lua_type)(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L,n)	((*lua_type)(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L,n)	((*lua_type)(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)		((*lua_type)(L, (n)) == LUA_TNIL)
#define lua_isboolean(L,n)	((*lua_type)(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L,n)	((*lua_type)(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L,n)		((*lua_type)(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n)	((*lua_type)(L, (n)) <= 0)


};

// Uses nm to get addresses for all these functions.
void bind_imports ();

#undef EXTERN

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

// not sure which of these strategies will work best
#include "lua_imports.hpp"  // manual linking (tedious, possibly incomplete)
//#include <lua.h>          // include our own binaries (dangerous, possibly fatal)

#include "fdbstub_logger.hpp"

using namespace lua_imports;

const char stdioname[] = "stdio";
template class Log<100,stdioname,FILE>;

const char applogname[] = "fdbstb";
template class Log<LOGLEVEL_FDBSTUB,applogname,StdioSink>;

using namespace std;

int write_shared (lua_State* state);
int write_shared_impl (lua_State* state);

lua_State* gState = nullptr;
int fdbsock = 0;


/**
 * @brief This is called at the end of factorio startup.
 * @return
 */
int stub_init ()
{
  Logger::print("fdbstub stub_init()");

  // open unix socket to fdb
  fdbsock = socket(AF_UNIX,SOCK_STREAM,0);
  sockaddr_un sa;
  memset(&sa, 0, sizeof(sockaddr_un));
  sa.sun_family = AF_UNIX;
  const char* fdbsocket_name = "fdbsocket";
  strncpy(sa.sun_path+1, fdbsocket_name,107);

  int r = connect(fdbsock,
                  reinterpret_cast<const struct sockaddr*>(&sa),
                  static_cast<socklen_t>(sizeof(sa_family_t) + strlen(fdbsocket_name) + 1));

  if (r) {
    Logger::error("Couldn't connect to fdb socket. Is fdb running? Errmsg: %s", strerror(errno));
    return 1;
  }

  return 0;
}

void stub_finalize ()
{
  if (fdbsock) close(fdbsock);
}


void register_lua_state (lua_State* s)
{
  gState = s;

  lua_pushcclosure(s,write_shared,0);
  lua_setglobal(s, "write_shared");
}

/**
 * syntax:
 *
 * write_shared(t)
 *
 * t = { type=..., data=... }
 *
 **/
int write_shared (lua_State* s)
{
  if (lua_isnil(s,-1)) {
    lua_pop(s,1);
    Logger::warning("write_shared called with nil argument");
    return 0;
  }

  if (!lua_istable(s,-1)) {
    lua_pop(s,1);
    Logger::warning("write_shared should receive a table as argument");
    return 0;
  }

  return write_shared_impl(s);
}

int write_shared_impl (lua_State* s)
{
  lua_pushstring(s, "type");
  lua_rawget(s, -2); // kv type-value
  size_t sztype_len;

  if (lua_isstring(s,-1)) {
    const char* sztype = lua_tolstring(s,-1,&sztype_len); // check type-value
    if (strncmp(sztype, "simple", sztype_len)) {
      lua_pop(s,1); // pop type-value
      lua_pushstring(s, "data"); // push data-key
      lua_rawget(s,-2); // kv data-value
      size_t szdata_len = 0;
      const char* szdata = lua_tolstring(s,-1,&szdata_len); // get data-value

      // write it to fdbsocket
      ssize_t r = 0;
      size_t bytes = 0;
      do {
        r = write(fdbsock, szdata, szdata_len);
        if (r > 0) {
          bytes += static_cast<size_t>(r);
        } else {
          Logger::error("I/O error while writing to the fdbsocket [%d]: %s", errno, strerror(errno));
          break;
        }
      } while (bytes < szdata_len);

      lua_pop(s,1); // pop data-value
    } else {
      lua_pop(s,1); // pop type-value
    }
  } else {
    lua_pop(s,1); // pop type-value
  }
  lua_pop(s,1); // pop table

  return 0;
}

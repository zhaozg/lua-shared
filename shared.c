#include <uv.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "ltable.h"

static uv_mutex_t _mutex = {0};
static uv_once_t _once = UV_ONCE_INIT;
static struct ltable* _table = NULL;

typedef struct value_s {
    int type;
    union {
        double n;
        int i;
        struct {
          int   l;
          char* p;
        }s;
        void* p;
    }v;
}Value;

static void shared_freevalue(Value* val)
{
}

static void shared_init_once()
{
  int ret = uv_mutex_init(&_mutex);
  if (ret==0)
  {
    _table = ltable_create(sizeof(Value), 0);
  }
}

static int shared_pushval(lua_State *L, Value *val)
{
  if (val!=NULL)
  {
    switch(val->type)
    {
    case LUA_TBOOLEAN:
      lua_pushboolean(L, val->v.i);
      break;
    case LUA_TNUMBER:
      lua_pushnumber(L, val->v.n);
      break;
    case LUA_TSTRING:
      lua_pushlstring(L, val->v.s.p, val->v.s.l);
      break;
    case LUA_TLIGHTUSERDATA:
      *(void**)lua_newuserdata(L, sizeof(void*)) = val->v.p;
      luaL_getmetatable(L, "lua.shared");
      lua_setmetatable(L, -2);
      break;
    default:
      lua_pushnil(L);
    }
    return 1;
  }
  return 0;
}

static Value* shared_tovalue(lua_State *L, int idx, Value *val)
{
  if (lua_isboolean(L, idx))
  {
    val->v.i = lua_toboolean(L, idx);
    val->type = LUA_TBOOLEAN;
  }
  else if(lua_isnumber(L, idx))
  {
    val->v.n = lua_tonumber(L, idx);
    val->type = LUA_TNUMBER;
  }
  else if(lua_isstring(L, idx))
  {
    size_t l;
    const char *p = lua_tolstring(L, idx, &l);
    val->type = LUA_TSTRING;
    val->v.s.l = l;
    val->v.s.p = malloc(l);
    memcpy(val->v.s.p, p, l);
  }
  else if(lua_islightuserdata(L, idx))
  {
    val->type = LUA_TLIGHTUSERDATA;
    val->v.p = lua_touserdata(L, idx);
    printf("shared: %p:%d\n", val->v.p, ltable_len(val->v.p));

  }else
    val->type = LUA_TNIL;
  return val;
}

static int shared_gc(lua_State *L)
{
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  ltable_release(t);
  return 0;
}

static int shared_set(lua_State *L)
{
  int type;
  struct ltable_key skey, *key;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  type = lua_type(L, 2);
  luaL_argcheck(L, type==LUA_TNUMBER || type==LUA_TSTRING, 2,
                "only accpet number or string as key");
  type = lua_type(L, 3);
  luaL_argcheck(L, type==LUA_TNIL || type==LUA_TNUMBER || type==LUA_TBOOLEAN ||
                type==LUA_TSTRING || type==LUA_TLIGHTUSERDATA, 3,
                "only accpet number or string as key");

  uv_mutex_lock(&_mutex);
  if (lua_isnumber(L, 2))
  {
    int ik = lua_tointeger(L, 2);
    key = ltable_intkey(&skey, ik);
  } else {
    const char* sk = lua_tostring(L, 2);
    key = ltable_strkey(&skey, sk);
  }
  if (lua_isnil(L, 3))
    ltable_del(t, key);
  else
  {
    Value *val = ltable_set(t, key);
    shared_tovalue(L, 3, val);
  }
  uv_mutex_unlock(&_mutex);
  return 0;
}

static int shared_get(lua_State *L)
{
  int type;
  struct ltable_key skey, *key;
  Value *val;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  type = lua_type(L, 2);
  luaL_argcheck(L, type==LUA_TNUMBER || type==LUA_TSTRING, 2,
                "only accpet number or string as key");

  uv_mutex_lock(&_mutex);
  if (lua_isnumber(L, 2))
  {
    int ik = lua_tointeger(L, 2);
    key = ltable_intkey(&skey, ik);
  } else {
    const char* sk = lua_tostring(L, 2);
    key = ltable_strkey(&skey, sk);
  }

  val = ltable_get(t, key);
  uv_mutex_unlock(&_mutex);
  return shared_pushval(L, val);
}

static int shared_length(lua_State *L)
{
  int len = 0;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  uv_mutex_lock(&_mutex);
  len = ltable_len(t);
  uv_mutex_unlock(&_mutex);
  lua_pushinteger(L, len);
  return 1;
}

static int shared_insert(lua_State *L)
{
  struct ltable_key skey, *key;
  Value *val;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  int len = lua_gettop(L);
  int type = lua_type(L, len);

  luaL_argcheck(L, len>1 && len<4, len, "with invalid paramaters");
  luaL_argcheck(L, type==LUA_TNUMBER || type==LUA_TBOOLEAN ||
                type==LUA_TSTRING || type==LUA_TLIGHTUSERDATA, len,
                "only accpet number, boolean, string or lightuserdata as value");

  uv_mutex_lock(&_mutex);
  if (len==2) {
    len = ltable_len(t);
    lua_pushinteger(L, len);
    lua_insert(L, 2);
  }
  len = (int)luaL_checkinteger(L, 2);
  key = ltable_intkey(&skey, len);
  val = ltable_set(t, key);
  shared_tovalue(L, 3, val);
  uv_mutex_unlock(&_mutex);
  return 0;
}

static int shared_remove(lua_State *L)
{
  int ret, len;
  struct ltable_key skey, *key;
  Value *val;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  int i = (int)luaL_optinteger(L, 2, 0);

  key = ltable_intkey(&skey, i);
  uv_mutex_lock(&_mutex);
  val = ltable_set(t, key);
  ret = shared_pushval(L, val);
  shared_freevalue(val);

  len = ltable_len(t);
  while(++i < len)
  {
    val = ltable_getn(t, i);
    *(Value*)ltable_set(t, key) = *val;
    key = ltable_intkey(&skey, i);
  }
  ltable_del(t, key);
  uv_mutex_unlock(&_mutex);
  return ret;
}

static int shared_tostring(lua_State *L) {
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  lua_pushfstring(L, "lua.shared: %p", t);
  return 1;
}


static const luaL_Reg classR[] =
{
  { "__index", shared_get},
  { "__newindex", shared_set},
  { "__len", shared_length},
  { "__tostring", shared_tostring },

  { NULL,  NULL}
};

static int shared_global(lua_State *L)
{
  *(void**)lua_newuserdata(L, sizeof(void*)) = _table;
  luaL_getmetatable(L, "lua.shared");
  lua_setmetatable(L, -2);
  return 1;
}

static int shared_push(lua_State *L)
{
  int off;
  struct ltable_key skey, *key;
  Value *val;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  struct ltable *n = ltable_create(sizeof(Value), 0);

  uv_mutex_lock(&_mutex);
  off = ltable_len(t);
  key = ltable_intkey(&skey, off);
  val = ltable_set(t, key);
  uv_mutex_unlock(&_mutex);
  val->type = LUA_TLIGHTUSERDATA;
  val->v.p = n;

  return shared_pushval(L, val);
}

static int shared_pop(lua_State *L)
{
  int off, len, i, ret;
  struct ltable_key skey, *key;
  struct ltable *t = *(struct ltable**)luaL_checkudata(L, 1, "lua.shared");
  Value *val;

  ret = 0;
  uv_mutex_lock(&_mutex);
  val = ltable_getn(t, 0);
  if (val) {
    i = 0;
    ret = shared_pushval(L, val);

    len = ltable_len(t);
    key = ltable_intkey(&skey, 0);
    while(++i < len)
    {
      val = ltable_getn(t, i);
      *(Value*)ltable_set(t, key) = *val;
      key = ltable_intkey(&skey, i);
    }
    ltable_del(t, key);
  }
  uv_mutex_unlock(&_mutex);
  return ret;
}

static const luaL_Reg sharedR[] =
{
  { "get", shared_get},
  { "set", shared_set},

  { "insert", shared_insert},
  { "remove", shared_remove},
  { "len", shared_length},

  { "global", shared_global},

  { "push", shared_push},
  { "pop", shared_pop},
  { "release", shared_gc},

  { NULL,  NULL}
};

LUALIB_API int luaopen_shared(lua_State *L)
{
  luaL_Reg *func;
  uv_once(&_once, shared_init_once);

  luaL_newmetatable(L, "lua.shared");
  for (func = (struct luaL_Reg *)classR; func->name; func++) {
    lua_pushstring(L, func->name);
    lua_pushcfunction(L, func->func);
    lua_rawset(L, -3);
  }
  lua_pop(L, 1);

  lua_newtable(L);
  luaL_register(L, NULL, sharedR);
  return 1;
}

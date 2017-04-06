#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <assert.h>


#if LUA_VERSION_NUM < 502
# ifndef luaL_newlib
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
# endif
# ifndef lua_setuservalue
#  define lua_setuservalue(L, n) lua_setfenv(L, n)
# endif
# ifndef lua_getuservalue
#  define lua_getuservalue(L, n) lua_getfenv(L, n)
# endif
#endif

#define MASK_DIS (1<<31)


typedef void *QUEUE[2];

/* Private macros. */
#define QUEUE_NEXT(q)       (*(QUEUE **) &((*(q))[0]))
#define QUEUE_PREV(q)       (*(QUEUE **) &((*(q))[1]))
#define QUEUE_PREV_NEXT(q)  (QUEUE_NEXT(QUEUE_PREV(q)))
#define QUEUE_NEXT_PREV(q)  (QUEUE_PREV(QUEUE_NEXT(q)))

/* Public macros. */
#define QUEUE_DATA(ptr, type, field)                                          \
  ((type *) ((char *) (ptr) - ((char *) &((type *) 0)->field)))

#define QUEUE_FOREACH(q, h)                                                   \
  for ((q) = QUEUE_NEXT(h); (q) != (h); (q) = QUEUE_NEXT(q))

#define QUEUE_EMPTY(q)                                                        \
  ((const QUEUE *) (q) == (const QUEUE *) QUEUE_NEXT(q))

#define QUEUE_HEAD(q)                                                         \
  (QUEUE_NEXT(q))

#define QUEUE_TAIL(q)                                                         \
  (QUEUE_PREV(q))

#define QUEUE_INIT(q)                                                         \
  do {                                                                        \
    QUEUE_NEXT(q) = (q);                                                      \
    QUEUE_PREV(q) = (q);                                                      \
  }                                                                           \
  while (0)

#define QUEUE_INSERT_HEAD(h, q)                                               \
  do {                                                                        \
    QUEUE_NEXT(q) = QUEUE_NEXT(h);                                            \
    QUEUE_PREV(q) = (h);                                                      \
    QUEUE_NEXT_PREV(q) = (q);                                                 \
    QUEUE_NEXT(h) = (q);                                                      \
  }                                                                           \
  while (0)

#define QUEUE_INSERT_TAIL(h, q)                                               \
  do {                                                                        \
    QUEUE_NEXT(q) = (h);                                                      \
    QUEUE_PREV(q) = QUEUE_PREV(h);                                            \
    QUEUE_PREV_NEXT(q) = (q);                                                 \
    QUEUE_PREV(h) = (q);                                                      \
  }                                                                           \
  while (0)


#define QUEUE_REMOVE(q)                                                       \
  do {                                                                        \
    QUEUE_PREV_NEXT(q) = QUEUE_NEXT(q);                                       \
    QUEUE_NEXT_PREV(q) = QUEUE_PREV(q);                                       \
  }                                                                           \
  while (0)


#define AOI_CLASS_MAP "cls{aoi_map_t}"
#define AOI_CLASS_UNIT "cls{aoi_unit_t}"

#define CHECK_MAP(L, idx)\
	((aoi_map_t *) luaL_checkudata(L, idx, AOI_CLASS_MAP))

#define CHECK_UNIT(L, idx)\
	((aoi_unit_t *) luaL_checkudata(L, idx, AOI_CLASS_UNIT))

typedef int32_t aoi_eid_t;

typedef struct {
	size_t unit_cnt;
	int width;
	int height;
	int ngrid_width;
	int ngrid_height;
	int grid_sz;
	int grid_cnt;
	QUEUE grid_list[1];
} aoi_map_t;

typedef struct {
	aoi_eid_t id;
	int grid_id;
	QUEUE qnode;
	int mask;
	int aoi_r;
	int x;
	int y;
} aoi_unit_t;

static void cal_grid(int width, int height,
		     int grid_sz,
		     int *ngrid_width, int *ngrid_height)
{
	*ngrid_width = width / grid_sz + (width % grid_sz > 0 ? 1 : 0);
	*ngrid_height = height / grid_sz + (height % grid_sz > 0 ? 1 : 0);
}

static int pos2idx(int ngrid_width, int ngrid_height, int grid_sz, int x, int y)
{
	int nx, ny;
	int idx;
	cal_grid(x, y, grid_sz, &nx, &ny);
	idx = ny * ngrid_width + nx;
	return idx;
}

static int mpos2idx(aoi_map_t *map, int x, int y)
{
	return pos2idx(map->ngrid_width,
		       map->ngrid_height,
		       map->grid_sz,
		       x, y);
}

static int get_neighbor_idx(aoi_map_t *map, int idx, int *arr, int arr_sz)
{
	int i;
	int n = 0;
	int offset[9] = {0};
	offset[0] = 0;
	offset[1] = -1;
	offset[2] = 1;
	offset[3] = -map->ngrid_width; 
	offset[4] = map->ngrid_width;
	offset[5] = -map->ngrid_width - 1;
	offset[6] = -map->ngrid_width + 1;
	offset[7] = map->ngrid_width - 1;
	offset[8] = map->ngrid_width + 1;
	for (i = 0; i < sizeof(offset)/sizeof(offset[0]); i++) {
		int new_idx = idx + offset[i];
		if (new_idx >= 0 && new_idx < map->grid_cnt) {
			arr[n++] = new_idx;
		}
	}
	return n;
}

static void init_map(aoi_map_t *map,
		    int width, int height,
		    int ngrid_width, int ngrid_height,
		    int grid_cnt, int grid_sz)
{
	int i;
	map->unit_cnt = 0;
	map->width = width;
	map->height = height;
	map->ngrid_width = ngrid_width;
	map->ngrid_height = ngrid_height;
	map->grid_cnt = grid_cnt;
	map->grid_sz = grid_sz;
	for (i = 0; i < grid_cnt; i++) {
		QUEUE_INIT(&map->grid_list[i]);
	}
}


static int lua__new_map(lua_State *L)
{
	int ngrid_width, ngrid_height;
	int grid_cnt;
	aoi_map_t *map;
	int width = luaL_checkinteger(L, 1);
	int height = luaL_checkinteger(L, 2);
	int grid_sz = luaL_checkinteger(L, 3);

	assert(grid_sz > 0);
	cal_grid(width, height, grid_sz, &ngrid_width, &ngrid_height);
	grid_cnt = ngrid_width * ngrid_height;

	map = (aoi_map_t *)lua_newuserdata(L, sizeof(aoi_map_t) + sizeof(QUEUE) * (grid_cnt - 1));
	init_map(map,
		 width, height,
		 ngrid_width, ngrid_height,
		 grid_cnt, grid_sz);

	lua_newtable(L);
	lua_setuservalue(L, -2);

	luaL_getmetatable(L, AOI_CLASS_MAP);
	lua_setmetatable(L, -2);

	return 1;
}

static int lua__add_unit(lua_State *L)
{
	int idx;
	QUEUE *q;
	aoi_map_t *map = CHECK_MAP(L, 1);
	aoi_unit_t *unit = CHECK_UNIT(L, 2);
	unit->x = (int)luaL_checknumber(L, 3);
	unit->y = (int)luaL_checknumber(L, 4);

	if (!QUEUE_EMPTY(&unit->qnode)) {
		return luaL_error(L, "unit already in map");
	}

	lua_getuservalue(L, 1);
	lua_pushinteger(L, unit->id);
	lua_rawget(L, -2);
	if (!lua_isnoneornil(L, -1)) {
		return luaL_error(L, "unit id dumplicate id=%d", unit->id);
	}
	lua_pop(L, 2);

	idx = mpos2idx(map, unit->x, unit->y);
	unit->grid_id = idx;
	q = &map->grid_list[idx];
	QUEUE_INSERT_TAIL(q, &unit->qnode);

	/* add to uservalue */
	lua_getuservalue(L, 1);
	lua_pushinteger(L, unit->id);
	lua_pushvalue(L, 2);
	lua_rawset(L, -3);

	map->unit_cnt++;
	lua_pushinteger(L, idx);
	return 1;
}

static int lua__del_unit(lua_State *L)
{
	void *p = NULL;
	int idx;
	aoi_map_t *map = CHECK_MAP(L, 1);
	aoi_unit_t *unit = CHECK_UNIT(L, 2);
	if (QUEUE_EMPTY(&unit->qnode)) {
		return luaL_error(L, "unit not in map");
	}
	lua_getuservalue(L, 1);
	lua_pushinteger(L, unit->id);
	lua_rawget(L, -2);
	p = lua_touserdata(L, -1);
	if (p != (void *)unit) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "unit not in this map");
		return 2;
	}
	lua_pop(L, 1);
	lua_pushinteger(L, unit->id);
	lua_pushnil(L);
	lua_rawset(L, -3);

	QUEUE_REMOVE(&unit->qnode);
	QUEUE_INIT(&unit->qnode);
	map->unit_cnt--;

	idx = unit->grid_id;
	lua_pushinteger(L, idx);
	return 1;
}

static int lua__move_unit(lua_State *L)
{
	int oid, nid;
	void *p = NULL;
	aoi_map_t *map = CHECK_MAP(L, 1);
	aoi_unit_t *unit = CHECK_UNIT(L, 2);
	float x = (float)luaL_checknumber(L, 3);
	float y = (float)luaL_checknumber(L, 4);
	if (QUEUE_EMPTY(&unit->qnode)) {
		return luaL_error(L, "unit not in map");
	}
	lua_getuservalue(L, 1);
	lua_pushinteger(L, unit->id);
	lua_rawget(L, -2);
	p = lua_touserdata(L, -1);
	if (p != (void *)unit) {
		return luaL_error(L, "unit not in this map");
	}

	oid = unit->grid_id;
	nid = mpos2idx(map, (int)x, (int)y);
	unit->x = (int)x;
	unit->y = (int)y;
	if (oid != nid) {
		unit->grid_id = nid;
		QUEUE_REMOVE(&unit->qnode);
		QUEUE_INIT(&unit->qnode);
		QUEUE_INSERT_TAIL(&map->grid_list[nid], &unit->qnode);
	} else {
		lua_pushboolean(L, 0);
		lua_pushinteger(L, nid);
		return 2;
	}
	lua_pushboolean(L, 1);
	lua_pushinteger(L, nid);
	lua_pushinteger(L, oid);
	return 3;
}

static int lua__get_gid_by_pos(lua_State *L)
{
	int idx;
	aoi_map_t *map = CHECK_MAP(L, 1);
	int x = (int)luaL_checknumber(L, 2);
	int y = (int)luaL_checknumber(L, 3);
	if (x < 0 || x >= map->width)
		return luaL_error(L, "x(%f) error![0, %d]", x, map->width);
	if (y < 0 || y >= map->height)
		return luaL_error(L, "y(%f) error![0, %d]", y, map->height);
	idx = mpos2idx(map, x, y);
	lua_pushinteger(L, idx);
	return 1;
}


static int lua__get_unit(lua_State *L)
{
	aoi_map_t *map = CHECK_MAP(L, 1);
	aoi_eid_t id = (aoi_eid_t)luaL_checkinteger(L, 2);
	(void)map;
	lua_getuservalue(L, 1);
	lua_pushinteger(L, id);
	lua_gettable(L, -2);
	return 1;
}


static int lua__get_units_by_gid(lua_State *L)
{
	QUEUE *l;
	QUEUE *q;
	aoi_map_t *map = CHECK_MAP(L, 1);
	int idx = luaL_checkinteger(L, 2);
	int mask = (int)luaL_optinteger(L, 3, 0);
	int x = (int)luaL_optnumber(L, 4, 0.0);
	int y = (int)luaL_optnumber(L, 5, 0.0);
	float dis = luaL_optnumber(L, 6, 0.0);
	float ddis = dis * dis;
	if (idx >= map->grid_cnt || idx < 0)
		return luaL_error(L, "idx error,[0, %d)", map->grid_cnt);
	l = &map->grid_list[idx];
	lua_newtable(L);
	QUEUE_FOREACH(q, l) {
		aoi_unit_t *unit = QUEUE_DATA(q, aoi_unit_t, qnode);
		int my_mask = unit->mask;
		if (mask == 0) {
			lua_pushboolean(L, 1);
			lua_rawseti(L, -2, unit->id);
			continue;
		}
		if ((mask & MASK_DIS)
		    && ((mask | MASK_DIS) == MASK_DIS || mask & my_mask)) {
			int dx = x - unit->x;
			int dy = y - unit->y;
			int dpower = dx * dx + dy * dy;
			if (dis > 0 ? dpower <= ddis : dpower <= unit->aoi_r * unit->aoi_r) {
				lua_pushboolean(L, 1);
				lua_rawseti(L, -2, unit->id);
			}
			continue;
		}
	}
	return 1;
}

static int lua__get_neighbor_grids(lua_State *L)
{
	int i,n;
	int idx_list[9] = {0};
	aoi_map_t *map = CHECK_MAP(L, 1);
	int idx = luaL_checkinteger(L, 2);
	if (idx >= map->grid_cnt || idx < 0)
		return luaL_error(L, "idx error,[0, %d)", map->grid_cnt);
	n = get_neighbor_idx(map, idx, idx_list, 9);
	lua_newtable(L);
	for (i = 0; i < n; i++) {
		lua_pushboolean(L, 1);
		lua_rawseti(L, -2, idx_list[i]);
	}
	lua_pushinteger(L, n);
	return 2;
}

static int lua__new_unit(lua_State *L)
{
	aoi_unit_t *unit = NULL;
	aoi_eid_t id = (aoi_eid_t)luaL_checkinteger(L, 1);
	float aoi_r = (float)luaL_optnumber(L, 2, 15.0);
	int mask = (int)luaL_optinteger(L, 3, 0);
	unit = (aoi_unit_t *)lua_newuserdata(L, sizeof(aoi_unit_t));
	unit->id = id;
	unit->aoi_r = aoi_r;
	unit->mask = mask;
	QUEUE_INIT(&unit->qnode);

	luaL_getmetatable(L, AOI_CLASS_UNIT);
	lua_setmetatable(L, -2);

	return 1;
}

static int lua__get_aoi(lua_State *L)
{
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	lua_pushnumber(L, (lua_Number)unit->aoi_r);
	return 1;
}

static int lua__set_aoi(lua_State *L)
{
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	float aoi_r = (float)luaL_checknumber(L, 2);
	unit->aoi_r = aoi_r;
	return 0;
}

static int lua__get_mask(lua_State *L)
{
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	lua_pushinteger(L, (lua_Integer)unit->mask);
	return 1;
}

static int lua__get_id(lua_State *L)
{
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	lua_pushinteger(L, (lua_Integer)unit->id);
	return 1;
}

static int lua__get_gid(lua_State *L)
{
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	if (QUEUE_EMPTY(&unit->qnode)) {
		lua_pushnil(L);
		lua_pushstring(L, "unit not in a scene");
		return 2;
	}
	lua_pushinteger(L, unit->grid_id);
	return 1;
}

static int lua__get_pos(lua_State *L)
{
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	lua_pushnumber(L, unit->x);
	lua_pushnumber(L, unit->y);
	return 2;
}


static int lua__set_mask(lua_State *L)
{
	int i;
	int top = lua_gettop(L);
	aoi_unit_t *unit = CHECK_UNIT(L, 1);
	int mask = 0;
	for (i = 2; i <= top; i++) {
		mask |= (int)luaL_checkinteger(L, i);
	}
	unit->mask = mask;
	lua_settop(L, 1);
	lua_pushinteger(L, mask);
	return 1;
}


static int opencls__unit(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"set_aoi", lua__set_aoi},
		{"get_aoi", lua__get_aoi},
		{"set_mask", lua__set_mask},
		{"get_mask", lua__get_mask},
		{"get_pos", lua__get_pos},
		{"get_id", lua__get_id},
		{"get_gid", lua__get_gid},
		{NULL, NULL},
	};
	luaL_newmetatable(L, AOI_CLASS_UNIT);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	return 1;
}

static int lua__map_gc(lua_State *L)
{
	aoi_unit_t *unit;
	aoi_map_t *map = CHECK_MAP(L, 1);
	(void)map;

	lua_getuservalue(L, 1);
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		unit = CHECK_UNIT(L, -1);
		QUEUE_REMOVE(&unit->qnode);
		QUEUE_INIT(&unit->qnode);
		lua_pop(L, 1);
	}
	return 0;
}

static int opencls__map(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"add_unit", lua__add_unit},
		{"del_unit", lua__del_unit},
		{"move_unit", lua__move_unit},
		{"get_neighbor_grids", lua__get_neighbor_grids},
		{"get_units_by_gid", lua__get_units_by_gid},
		{"get_unit", lua__get_unit},
		{"get_gid_by_pos", lua__get_gid_by_pos},
		{NULL, NULL},
	};
	luaL_newmetatable(L, AOI_CLASS_MAP);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__map_gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}

int luaopen_lgaoi(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new_map", lua__new_map},
		{"new_unit", lua__new_unit},
		{NULL, NULL},
	};
	opencls__unit(L);
	opencls__map(L);
	luaL_newlib(L, lfuncs);
	return 1;
}

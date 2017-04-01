local lgaoi = require "lgaoi"


function printf(fmt, ...)
	print(string.format(fmt, ...))
end

function get_all_neighbor_unit_id_list(map, gid)
	local t = {}
	for idx, _ in pairs(map:get_neighbor_grids(gid)) do
		for id, _ in pairs(map:get_units_by_gid(idx)) do
			t[id] = true
		end
	end
	return t
end

function get_neighbor_diff(map, ngid, ogid)
	local nmap = map:get_neighbor_grids(ngid)
	local omap = map:get_neighbor_grids(ogid)
	local add = {}
	local sub = {}
	for k, _ in pairs(nmap) do
		if not omap[k] then
			add[k] = true
		end
	end
	for k, _ in pairs(omap) do
		if not nmap[k] then
			sub[k] = true
		end
	end
	return add, sub
end

--[[
function cb(map, maker_id, watcher_id_map, event, ...)
end
--]]
function enter_with_event(map, unit, x, y, cb)
	assert(cb)
	local gid = map:get_gid_by_pos(x, y)
	local neighbor_map = get_all_neighbor_unit_id_list(map, gid)
	map:add_unit(unit, x, y)
	local maker_id = unit:get_id()
	cb(map, unit:get_id(), neighbor_map, "enter", x, y)
end

function exit_with_event(map, unit, cb)
	assert(cb)
	local gid = unit:get_gid()
	local x, y = unit:get_pos()
	map:del_unit(unit)
	local neighbor_map = get_all_neighbor_unit_id_list(map, gid)
	local maker_id = unit:get_id()
	cb(map, unit:get_id(), neighbor_map, "exit", x, y)
end

function move_with_event(map, unit, x, y, cb)
	local ox, oy = unit:get_pos()
	local ok, ngid, ogid = map:move_unit(unit, x, y);
	if not ok then
		return
	end
	local enter_gids, exit_gids = get_neighbor_diff(map, ngid, ogid)
	local enter_watcher_map = {}
	local exit_watcher_map = {}
	for gid in pairs(enter_gids) do
		for id, _ in pairs(map:get_units_by_gid(gid)) do
			enter_watcher_map[id] = true
		end
	end
	for gid in pairs(exit_gids) do
		for id, _ in pairs(map:get_units_by_gid(gid)) do
			exit_watcher_map[id] = true
		end
	end
	local maker_id = unit:get_id()
	enter_watcher_map[maker_id] = nil
	cb(map, maker_id, exit_watcher_map, "exit", x, y, ox, oy)
	cb(map, maker_id, enter_watcher_map, "enter", x, y, ox, oy)
end


local map = lgaoi.new_map(100, 100, 10)
local list = {
	{25, 28},
	{24, 26},
	{35, 32},
	{9, 7},
	{17, 19},
	{12, 60},
	{15, 57},
}

function test_enter()
	for i, pos in pairs(list) do
		local unit = lgaoi.new_unit(i)
		local x, y = unpack(pos)
		printf("enter========,id=%d,(%d,%d)", i, x, y)
		enter_with_event(map, unit, x, y, function(map, maker_id, watch_map, event, nx, ny)
			for watcher_id in pairs(watch_map) do
				local watcher = map:get_unit(watcher_id)
				local wx, wy = watcher:get_pos()
				local wgid = map:get_gid_by_pos(wx, wy)
				local mgid = map:get_gid_by_pos(nx, ny)
				printf("watcher=%d(%d,%d,%d) maker=%d(%d,%d,%d)%s",
				       watcher_id, wx, wy, wgid,
				       maker_id, nx, ny, mgid, event)
			end
		end)
	end
end


function test_move()
	local unit1 = map:get_unit(1)
	printf("move========,id=%d,from(%d,%d)to(15, 55)", 1, unit1:get_pos())
	move_with_event(map, unit1, 15, 55, function(map, maker_id, watch_map, event, x, y, ox, oy)
		for watcher_id in pairs(watch_map) do
			local watcher = map:get_unit(watcher_id)
			local wx, wy = watcher:get_pos()
			printf("watcher=%d(%d, %d) maker=%d %s from(%d,%d)to(%d,%d)",
			       watcher_id, wx, wy,
			       maker_id, event,
			       ox, oy, x, y)
		end
	end)
end

function test_exit()
	local unit1 = map:get_unit(1)
	printf("exit========,id=%d,from(%d,%d)to(15, 55)", 1, unit1:get_pos())
	exit_with_event(map, unit1, function(map, maker_id, watch_map, event, x, y)
		for watcher_id in pairs(watch_map) do
			local watcher = map:get_unit(watcher_id)
			local wx, wy = watcher:get_pos()
			printf("watcher=%d(%d,%d) maker=%d(%d,%d) %s",
			       watcher_id, wx, wy,
			       maker_id, x, y,
			       event)
		end
	end)
end

test_enter()
test_move()
test_exit()

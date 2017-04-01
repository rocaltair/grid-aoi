local lgaoi = require "lgaoi"

local map = lgaoi.new_map(100, 100, 10)

function printf(fmt, ...)
	print(string.format(fmt, ...))
end

local list = {
	{25, 28},
	{24, 26},
	{35, 32},
	{9, 7},
	{17, 19},
	{12, 60},
	{15, 57},
}


for i, pos in pairs(list) do
	local unit = lgaoi.new_unit(i)
	map:add_unit(unit, unpack(pos))
	printf("add,id=%d,(%d, %d)", i, unpack(pos))
end

unit = map:get_unit(1)
local gid1 = unit:get_gid()
printf("id=%d,gid=%d,(%d, %d)", unit:get_id(), gid1, unit:get_pos())

for id, _ in pairs(map:get_units_by_gid(gid1)) do
	local unit = map:get_unit(id)
	printf("neighbors in %d,id=%d,(%d,%d)", gid1, id, unit:get_pos())
end

for idx, _ in pairs(map:get_neighbor_grids(gid1)) do
	for id, _ in pairs(map:get_units_by_gid(idx)) do
		local unit = map:get_unit(id)
		printf("all neighbors in %d,id=%d,(%d,%d)", idx, id, unit:get_pos())
	end
end



#!/usr/bin/lua
dofile"base.lua"

local s = "1 3 5 7 6 5 4 3 5 7 9 11 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 3 5 7 9 11 10 9 8 7 6 5 4 3 2 1 3 5 7 9 11 13 12 11 10 9 8 7 6 5 4 3 2 1 3 5 7 9 8 7 6 5 4 3 5 7 9 11 10 9 8 7 6 5 7 9 11 13 12 11 10 9 8 7 6 5 4 3 5 7 9 11 13 12 11 10 9 11 13"

local conf = read_conf(s)

local cmatrix = {}
setmetatable(cmatrix, cmatrix_mt)
cmatrix:init(conf:get_N())

cmatrix:print()

for i, g in ipairs(conf) do
	cmatrix:apply_gen(g)
	--print("gen " + (g - 1))
	
	cmatrix:print()
end

dofile"base.lua"

--[[mt = {
[1] = {6, 7, 4, 5, 2, 3},
[2] = {6, 4, 7, 5, 1, 3},
[3] = {4, 6, 5, 7, 1, 2},
[4] = {3, 6, 2, 7, 1, 5},
[5] = {6, 3, 7, 2, 1, 4},
[6] = {5, 3, 4, 2, 1, 7},
[7] = {3, 5, 2, 4, 1, 6}
}
--]]

function init_d(mt)
	local p = {tau = {}, a = {}, base = {}}
	local N = table.getn(mt)
	for i = 1, N do
		local ang = (i-1)/N * math.pi
		p.tau[i] = {x = math.cos(ang), y = -math.sin(ang)}
		p.a[i] = 0
		p.base[i] = {}
		if i ~= 1 and i ~= mt[1][1] then
			for k = 1, table.getn(mt[i]) do
				local supline = mt[i][k]
				for l = 1, table.getn(mt[supline]) do
					if mt[supline][l] == i then
						if i < supline and mt[supline][l+1] then
							table.insert(p.base[i], {sline = supline, bline = mt[supline][l+1], dir = 1})
						elseif i > supline and mt[supline][l-1] then
							table.insert(p.base[i], {sline = supline, bline = mt[supline][l-1], dir = -1})
						end
						break
					end
				end
			end
		end
	end
	return p
end

function step2(p, rob)
	local function getxy(l1, l2)
		local det = -p.tau[l1].y*p.tau[l2].x + p.tau[l1].x*p.tau[l2].y
		assert (det ~= 0)
		local x = (p.a[l1] * p.tau[l2].x - p.a[l2] * p.tau[l1].x) / det
		local y = (-p.tau[l1].y * p.a[l2] + p.tau[l2].y * p.a[l1]) / det
		return x, y
	end
	local a1, a2, x1, x2, y1, y2 = {}, {}, {}, {}, {}, {} 
	for _ = 1, table.getn(p.tau) do
		a1[_], a2[_], x1[_], y1[_], x2[_], y2[_] = 0, 0, 0, 0, 0, 0
	end
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			for _, v in ipairs(k) do
				local x, y = getxy(v.sline, v.bline)
				x = x + p.tau[v.sline].y * v.dir
				y = y - p.tau[v.sline].x * v.dir
				local a = -x*p.tau[i].y + y*p.tau[i].x
				if a > a1[i] then
					a1[i], a2[i], x1[i], y1[i], x2[i], y2[i] = a, a1[i], x, y, x1[i], y1[i]
				end
			end
		end
	end
	local change = 0
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			local olda = p.a[i]
			p.a[i] = p.a[i] + (a1[i] - p.a[i]) * rob
			local x, y = x1[i] - x2[i], y1[i] - y2[i]
			if p.tau[i].x * x + p.tau[i].y * y > 0 then
				p.tau[i].x, p.tau[i].y = p.tau[i].x + (x - p.tau[i].x) * rob * 0.001, p.tau[i].y + (y - p.tau[i].y) * rob * 0.001
			else
				p.tau[i].x, p.tau[i].y = p.tau[i].x + (-x - p.tau[i].x) * rob * 0.001, p.tau[i].y + (-y - p.tau[i].y) * rob * 0.001
			end --]]
			local len = math.sqrt(p.tau[i].x ^ 2 + p.tau[i].y ^ 2)
			if len ~= 0 then
				p.tau[i].x, p.tau[i].y = p.tau[i].x / len, p.tau[i].y / len
			end --]]
			if (p.a[i] < 0) then p.a[i] = 0 end
			change = change + (olda - p.a[i]) ^ 2
		end
	end
	local rez = 0
	local isValid = true
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			for _, v in ipairs(k) do
				local x, y = getxy(v.sline, v.bline)
				local xo, yo = getxy(v.sline, i)
				local dist = ((x - xo)*p.tau[v.sline].x + (y - yo)*p.tau[v.sline].y)*v.dir
				if dist < 0 then
					rez = rez - dist
					isValid = false
				end
			end
		end
	end
	return change, isValid, rez
end

function step(p, rob)
	local function getxy(l1, l2)
		local det = -p.tau[l1].y*p.tau[l2].x + p.tau[l1].x*p.tau[l2].y
		assert (det ~= 0)
		local x = (p.a[l1] * p.tau[l2].x - p.a[l2] * p.tau[l1].x) / det
		local y = (-p.tau[l1].y * p.a[l2] + p.tau[l2].y * p.a[l1]) / det
		return x, y
	end
	local change = 0
	local vel = {}
	local angvel = {}
	for _ = 1, table.getn(p.tau) do
		vel[_] = 0
		angvel[_] = 0
	end	
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			local olda = p.a[i]
			--p.a[i] = 0
			for _, v in ipairs(k) do
				local l1 = v.sline
				local l2 = v.bline
				local det = -p.tau[l1].y*p.tau[l2].x + p.tau[l1].x*p.tau[l2].y
				assert (det ~= 0)
				local x = (p.a[l1] * p.tau[l2].x - p.a[l2] * p.tau[l1].x) / det + p.tau[v.sline].y * v.dir
				local y = (-p.tau[l1].y * p.a[l2] + p.tau[l2].y * p.a[l1]) / det - p.tau[v.sline].x * v.dir
				
				local a = -x*p.tau[i].y + y*p.tau[i].x
				if a > p.a[i] then
					--p.a[i] = a 
					local deltaa = 1 --a - p.a[i]
					vel[l1] = vel[l1] + (-p.tau[l2].x * p.tau[i].y + p.tau[l2].y * p.tau[i].x) / det * deltaa
					vel[l2] = vel[l2] + (p.tau[l1].x * p.tau[i].y - p.tau[l1].y * p.tau[i].x) / det * deltaa
					vel[i] = vel[i] - deltaa
					angvel[l1] = angvel[l1] + (
						((p.a[l2] * p.tau[l1].y) / det + p.tau[l1].x * v.dir) * (-p.tau[i].y) + 
						((-p.tau[l1].x * p.a[l2]) / det + p.tau[l1].y * v.dir) * (p.tau[i].x)) --* deltaa
					angvel[l2] = angvel[l2] + (
						(-p.a[l1] * p.tau[l2].y) / det * (-p.tau[i].y) +
						(p.tau[l2].x * p.a[l1]) / det * (p.tau[i].x)) --* deltaa
					angvel[i] = (angvel[i] + x*p.tau[i].x + y*p.tau[i].y ) --* deltaa
				end
				
				--print("sline =", v.sline, "bline =", v.bline, "dir =", v.dir)
			end
			--p.a[i] = olda + (p.a[i] - olda)*0.05
			
		end
	end
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			olda = p.a[i]
			p.a[i] = p.a[i] - vel[i]*rob
			p.tau[i].x, p.tau[i].y = p.tau[i].x - angvel[i] * (-p.tau[i].y) * rob * 0.001, p.tau[i].y - angvel[i] * (p.tau[i].x) * rob * 0.001
			local len = math.sqrt(p.tau[i].x ^ 2 + p.tau[i].y ^ 2)
			if len ~= 0 then
				p.tau[i].x, p.tau[i].y = p.tau[i].x / len, p.tau[i].y / len
			end
			if (p.a[i] < 0) then p.a[i] = 0 end
			change = change + (olda - p.a[i]) ^ 2
		end
	end
	local rez = 0
	local isValid = true
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			for _, v in ipairs(k) do
				local x, y = getxy(v.sline, v.bline)
				local xo, yo = getxy(v.sline, i)
				local dist = ((x - xo)*p.tau[v.sline].x + (y - yo)*p.tau[v.sline].y)*v.dir
				if dist < 0 then
					rez = rez - dist
					isValid = false
				end
			end
		end
	end
	return change, isValid, rez
end

confs = load_confs("gen7.txt")
mt = get_Omatrix(confs[1])
print_matrix(mt)
print_conf(confs[1])
p = init_d(mt)

local iter = 1
local rob = 0.002
local change = 0.001
local isValid, rez
while true do
	if change ~= 0 and change < 0.001 then rob = 0.002 * 0.001/change else rob = 0.002 end
	change, isValid, rez = step(p, rob)
	print("\n iter =", iter, "change =", change, "isValid =", isValid, rez)
	iter = iter + 1
	for i, k in ipairs(p.base) do
		if table.getn(k) > 0 then
			print("\nline # ", i, "a =", p.a[i])
			--[[for _, v in ipairs(k) do
				print("sline =", v.sline, "bline =", v.bline, "dir =", v.dir)
			end --]]
		end
	end
	if change < 1e-6 and isValid == true then break end
	--break
end

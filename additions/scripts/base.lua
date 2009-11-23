-- N and k for all configurations
local configurations = {__mode = "k"}

local Omatrices = {__mode = "k"}
local polygon_nums = {__mode = "k"}

--[[

configuration {
	out:		int: N
	get_N()

	out:		int: k
	get_k()

	out:		int: s
	get_s()
	
	print_gens()

	print()

	out:		Omatrix
	get_Omatrix()

	out:		poligon_num
	get_polygon_num()
	
	operator ==
}

Omatrix {
	out:		this
	print_matrix()
	
	out:		array of generators
	get_gens()
	
	in:			int: line number to remove
	out:		this
	remove_line(l)
	
	operator ==
}

poligon_num {
	out:		this
	print()
	
	operator ==
}

--]]

-- in:		string: filename; string: end generators
-- out:		array of the configurations
function load_confs(filename, end_gens)
	io.input(filename)
	local confs = {}
	while true do
		local line = io.read()
		if line == nil then break end
		if end_gens then
			line = line .. " " .. end_gens
		end
		table.insert(confs, read_conf(line))
    end
	return confs
end

-- in:		array of generators
-- out:		configuration
function read_conf_from_table(gens_table)
	local conf = {}
	for i = 1, table.getn(gens_table) do
		conf[i] = gens_table[i]
	end
	
	configurations[conf] = {N = 0, k = 0}
	max_gen = conf[1]
	for _, w in ipairs(conf) do
		if max_gen < w then max_gen = w end
	end
	local N = max_gen + 1
	local k = N*(N-1)/2 - table.getn(conf)
	configurations[conf].N = N
	configurations[conf].k = k
	
	local s = - 1 + (N % 2);
	for i, k in ipairs(conf) do
		s = s - ((k-1) % 2)*2 + 1;
	end
	if s < 0 then s = -s end
	configurations[conf].s = s
		
	conf.get_N = function(this)
		return configurations[this].N
	end
	
	conf.get_k = function(this)
		return configurations[this].k
	end
	
	conf.get_s = function(this)
		return configurations[this].s
	end
	
	conf.print_gens = function(this)
		for i, k in ipairs(this) do
			io.write(string.format("%d ",k-1))
		end
		io.write("\n")	
	end
	
	conf.print = function(this) 
		--print("\ngenerators:")
		this:print_gens()
		
		print("N = ", configurations[this].N)
		print("k = ", configurations[this].k)
		print("s = ", configurations[this].s)
	end

	conf.get_Omatrix_indexed = function(this)
		local matrix = {}
		local lines = {}
		local positions = {}
		for i = 1, this:get_N() do
			matrix[i] = {}
			lines[i] = i
		end
		for i = 1, table.getn(this) do
			local gen = this[i]
			local first = lines[gen]
			local second = lines[gen+1]
			
			table.insert(matrix[first], i)
			table.insert(matrix[second], i)
			lines[gen] = second;
			lines[gen+1] = first;
		end
		return matrix
	end
	
	conf.get_Omatrix = function(this, recreate)
		if not recreate and Omatrices[this] then
			return Omatrices[this]
		end

		local matrix = {}
		local lines = {}
		local positions = {}
		matrix.parallels = {}
		for i = 1, this:get_N() do
			matrix[i] = {}
			lines[i] = i
			positions[i] = 0
			matrix.parallels[i] = i
		end
		for i = 1, table.getn(this) do
			local gen = this[i]
			local first = lines[gen]
			local second = lines[gen+1]
			positions[first] = positions[first] + 1
			positions[second] = positions[second] + 1
			
			matrix[first][positions[first]] = second;
			lines[gen] = second;
			
			matrix[second][positions[second]] = first;
			lines[gen+1] = first;
		end
		for i = 1, this:get_N()-1 do
			if lines[i] < lines[i+1] then
				matrix.parallels[lines[i]] = lines[i+1]
				matrix.parallels[lines[i+1]] = lines[i]
			end
		end
		--[[for i = 1, this:get_N() do
			for j, v in ipairs(matrix[i]) do
				matrix[i][j] = matrix[i][j] - i
				if matrix[i][j] < 0 then
					matrix[i][j] = matrix[i][j] + this:get_N()
				end
			end
		end]]
		setmetatable(matrix, Omatrix_mt)
		
		matrix.print_matrix = function(this)
			for i = 1, table.getn(this) do
				io.write(string.format("%2d ||%2d)\t", i, this.parallels[i] or 0))
				for k = 1, table.getn(this[i]) do
					io.write(string.format("%d\t", this[i][k]))
				end
				io.write("\n")
			end
			io.write("\n")
			return this
		end
		
		matrix.get_gens = function(this)
			local result = {}
			local p1 = {}
			local p2 = {}
			local a = {}
			local n = table.getn(this)
			local k = 0
			for i = 1, n do
				p1[i] = 1
				p2[i] = n - 1
				if this.parallels[i] ~= i then
					p2[i] = p2[i] - 1
					k = k + 1
				end
				a[i] = i
			end
			k = k / 2
			for i = 1, n*(n-1)/2-k do
				for gen = 1, n-1 do
					local left = a[gen]
					local right = a[gen+1]
					if (p1[left] <= p2[left] and p1[right] <= p2[right]
						and this[left][p1[left]] == right and o[right][p1[right]] == left)
					then
						p1[left] = p1[left] + 1
						p1[right] = p1[right] + 1
						a[gen] = right
						a[gen+1] = left
						result[i] = gen
						break
					end
				end
			end
			return result
		end
		
		matrix.remove_line = function(this, l)
			local n = table.getn(this)
			if l > n then
				print("remove_line error, l > n")
				return
			end
			for i = l, n-1 do
				this[i] = this[i+1]
			end
			this[n] = nil
			n = n-1
			for i = 1, n do
				local llen = table.getn(this[i])
				for j = 1, llen do
					if this[i][j] == l then
						for k = j, llen-1 do
							this[i][k] = this[i][k+1]
						end
						this[i][llen] = nil
						break
					end
				end
			end
			
			for i = 1, n do
				for j = 1, table.getn(this[i]) do
					if this[i][j] > l then
						this[i][j] = this[i][j] - 1
					end
				end
			end
			return this
		end
		
		Omatrices[this] = matrix
		
		return matrix
	end
	
	conf.get_polygon_num = function(this, recreate)
		if not recreate and polygon_nums[this] then
			return polygon_nums[this]
		end
		
		local s = {int = {max_n = 0}, ext = {max_n = 0}}
		
		local function put(t, n)
			t[n] = (t[n] or 0) + 1
			--print ("  ", t.max_n, t[n], n)
			--t.max_n = 8
			if (t.max_n < n) then
				t.max_n = n
			end
		end
		
		local n = {}
		local is_int = {}	
		for i = 0, this:get_N() do	
			n[i] = 0 
			is_int[i] = false
		end
		
		for i = 1, table.getn(this) do
			local gen = this[i]
			n[gen-1] = n[gen-1] + 1
			n[gen+1] = n[gen+1] + 1
			--print(n[gen-1], n[gen+1], n[gen]+1)
			if (is_int[gen] == true) then
				put(s.int, n[gen]+1)
			else
				put(s.ext, n[gen]+1)
			end
			n[gen] = 1
			is_int[gen] = true
		end
		for i = 0, this:get_N() do
			put(s.ext, n[i])
		end
		
		for i = 1, s.ext.max_n do
			s.ext[i] = s.ext[i] or 0
		end
		for i = 1, s.int.max_n do
			s.int[i] = s.int[i] or 0
		end
		
		setmetatable(s, polygon_num_mt)
		
		s.print = function(this)
			print("External areas:")
			for i = 1, this.ext.max_n do
				print(i, this.ext[i])
			end
			print("Internal areas:")
			for i = 1, this.int.max_n do
				print(i, this.int[i])
			end
			print("\n")
			return this	
		end
		
		polygon_nums[this] = s
		
		return s
	end
	
	conf.get_polygons = function(this)
		local is_int = {}
		local a = {}
		local p = {}
		local int_polygons = {}
		local ext_polygons = {}

		for i = 0, this:get_N() do
			a[i] = i
			p[i] = {l1 = i, l2 = i + 1, d1 = -1, d2 = -1, l = {}, r = {}, c = i}
			is_int[i] = false
		end
		
		for i = 1, table.getn(this) do 
			local gen = this[i]
			
			p[gen].e = i
			table.insert(p[gen].l, i)
			table.insert(p[gen-1].r, i)
			table.insert(p[gen+1].l, i)
			
			if is_int[gen] then
				table.insert(int_polygons, p[gen])
			else
				table.insert(ext_polygons, p[gen])
			end
			p[gen] = {b = i, l = {i}, r = {}}
			
			a[gen], a[gen + 1] = a[gen + 1], a[gen]
			is_int[gen] = true
		end

		for i = 1, this:get_N() - 1 do
			p[i].l2 = a[i]
			p[i].l1 = a[i + 1]
			p[i].d2 = 1
			p[i].d1 = 1
			table.insert(ext_polygons, p[i])
		end
		
		p[0].l1 = a[1]
		p[0].d1 = 1
		table.insert(ext_polygons, p[0])
		
		p[this:get_N()].l2 = a[this:get_N()]
		p[this:get_N()].d2 = 1
		table.insert(ext_polygons, p[this:get_N()])
		
		return {ext_polygons = ext_polygons, int_polygons = int_polygons}
	end

	setmetatable(conf, configuration_mt)
	
	--print(N, k)
	return conf
end

-- in:		string: generators
-- out:	configuration
function read_conf(gens)
	local line = gens
	local conf = {}
	local _, j = string.find(line, ")")
	if j then line = string.sub(line, j) end
	
	for w in string.gfind(line, "(%d+)%s*") do
		table.insert(conf, tonumber(w)+1)
	end
	return read_conf_from_table(conf)
end

configuration_mt = {
	__eq = function (a, b)
		return a:get_polygon_num() == b:get_polygon_num() and a:get_Omatrix() == b:get_Omatrix()
	end
}

print_matrix = 0

-- in:		Omatrix: o1, o2; bool: calc_diff
-- out:	bool: true if equal; int: difference
function compare_confs(o1, o2, calc_diff)
	local n1 = table.getn(o1)
	local n2 = table.getn(o2)
	if n1 ~= n2 then return false end
	local function norm(v)
		v = math.fmod(v+2*n1, n1)
		if v == 0 then v = n1 end
		return v
	end
	local function norm2(v)
		v = math.fmod(v+2*n1, 2*n1)
		if v == 0 then v = 2*n1 end
		return v
	end	
	local function get_val(offset, i, j)
		local val
		if norm2(i + offset) <= n1 then 
			val = o2[norm2(i + offset)][j]
		else 
			local line = norm(i+offset)
			if (o2.parallels[line]) then line = o2.parallels[line] end
			val = o2[line][table.getn(o2[line])-j+1]
		end
		if ((offset <= n1 and val > offset) or (offset > n1 and val <= norm(offset))) then return norm(val - offset) end
		if (o2.parallels[val]) then val = o2.parallels[val] end
		val = val - offset
		return norm(val)
	end
	local function get_line_len(l)
		return table.getn(o2[norm(l)])
	end
	
	--  Выводит Число различий между элементами Оматрицами в определенном повороте
	local function cmp_offset(offset)
		local diff = 0;

		for i = 1, n1 do
			if (table.getn(o1[i]) ~= get_line_len(offset+i)) then
				return 100000
			end
			
			for j = 1, table.getn(o1[i]) do
				if o1[i][j] ~= get_val(offset, i, j) then 
					diff = diff + 1
					if not calc_diff then 
						return 1
					end
				end
			end
		end

		return diff
	end

	local function convert_n2o(v, d)
		v = norm2(2+d-v)
		if v > n1 then return o1.parallels[v-n1] end
		return v
	end
	local function convert_o2n(v, d)
		if v > d + 1 then v = o1.parallels[v] + n1 end
		return norm(2+d-v)
	end
	
	--print_matrix(o1)
	--print_matrix(o2)

	-- Возвращает:
	-- минимальное число различий в поворотах.
	local function f()
		local diff, min_diff;
		min_diff = n1*n1*2
		for i = 0, 2*n1-1 do
			if o2.parallels[norm(i)] <= norm(i) then
				--[[print("i = ", i)
				for j = 1, n1 do
					for k = 1, get_line_len(j+i) do
						io.write(string.format("%2d ", get_val(i, j, k)))
					end
					print("")
				end --]]
				diff = cmp_offset(i)
--[[				if calc_diff then
				end--]]
				min_diff = (diff < min_diff) and diff or min_diff
				if diff == 0 then
					return 0
				end
			end
		end
		return min_diff
	end

	local diff = f()
--	if res then return res, diff end
	if diff == 0 then
		return true, 0
	end

	o = {}
	o[1] = {}
	local d = 0
	if (o1.parallels[1] ~= 1) then d = 1 o[2] = {} end
	for j = 1, table.getn(o1[convert_n2o(1, d)]) do
			o[1][j] = convert_o2n(o1[convert_n2o(1, d)][j], d)
			if d == 1 then
				o[2][j] = convert_o2n(o1[convert_n2o(2, d)][j], d)
			end
	end
	for i = 2+d, n1 do
		o[i] = {}
		local ind = convert_n2o(i, d)
		local len = table.getn(o1[ind])
		for j = 1, len do
			o[i][j] = convert_o2n(o1[ind][len - j + 1], d)
		end
	end
	o.parallels = o1.parallels
	o1 = o

	local diff2 = f()
	if diff > diff2 then
		diff = diff2
	end
	return diff == 0, diff
end

Omatrix_mt = {
	__eq = compare_confs
}

polygon_num_mt = {
	__eq = function (a, b)
		local function cmp(t1, t2)
			if (t1.max_n ~= t2.max_n) then return false end
			for i = 1, t1.max_n do
				if (t1[i] or 0) ~= (t2[i] or 0) then return false end
			end
			return true
		end
		return cmp(a.ext, b.ext) and cmp(a.int, b.int)
	end
}

function load_bgr(filename)
	local big_gr = {}
	local i1, i2
	io.input(filename)
	while true do
		local line = io.read()
		if line == nil then break end
		_, _, i1, i2 = string.find(line, "(%d+)%s*(%d+)")
		i1, i2 = tonumber(i1), tonumber(i2)
		--print(i1, i2)
		big_gr[i1] = big_gr[i1] or {}
		big_gr[i2] = big_gr[i2] or {}
		table.insert(big_gr[i1], i2)
		table.insert(big_gr[i2], i1)
	end
	return big_gr
end

function distance(bgr, i1, i2)
	local potencial = {}
	local input = {}
	local output = {}
	table.insert(input, i1)
	local l
	l = 0
	while table.getn(input) > 0 do
		l = l + 1
		for i, k in pairs(input) do
			for i1, k1 in pairs(bgr[k]) do
				if not potencial[k1] then
					table.insert(output, k1)
					potencial[k1] = l
					if k1 == i2 then return l end
				end
			end
		end
		input = output
		output = {}
	end
end

function parse_ini(filename)
	local res, m, mode, n, val
	local ini = {}
	io.input(filename)
	while true do
		local line = io.read()
		if line == nil then break end
		res, _, m = string.find(line, "^%[(.+)%]$")
		if res then
			mode = m
			ini[mode] = {}
		else
			res, _, n, val = string.find(line, "^(.+)=(.+)$")
			if res then
				ini[mode][n] = val
			end
		end
	end
	return ini
end

function init_svg()
	package.loadlib("lua_svg.dll", "init")()
end

function convert_arn_to_svg(filename, svg_filename, border_size, color)
	border_size = border_size or 0.1
	color = color or 0
	
	ini = parse_ini(filename)
	
	
	if tonumber(ini.version.version) == 1 then
		os.setlocale("rus")	
	else
		os.setlocale("eng")	
	end
	
	if not ini.parameters.number_of_generators then return nil end
	local n_gens = tonumber(ini.parameters.number_of_generators)

	local conf = {}
	local x = {}
	local y = {}
	local xmin, xmax, ymin, ymax
	xmin = tonumber(ini.points["x0"])
	xmax = xmin
	ymin = tonumber(ini.points["y0"])
	ymax = ymin
	for i = 1, n_gens do
		conf[i] = tonumber(ini.points[string.format("g%d", i - 1)]) + 1
		x[i] = tonumber(ini.points[string.format("x%d", i - 1)])
		y[i] = tonumber(ini.points[string.format("y%d", i - 1)])
		
		xmax = (x[i] > xmax) and x[i] or xmax
		ymax = (y[i] > ymax) and y[i] or ymax
		xmin = (x[i] < xmin) and x[i] or xmin
		ymin = (y[i] < ymin) and y[i] or ymin
	end
	
	local zoomx = 800 / (math.floor((xmax - xmin) * (1 + border_size) + 1))
	local zoomy = 600 / (math.floor((ymax - ymin) * (1 + border_size) + 1))
	local zoom = (zoomx > zoomy) and zoomy or zoomx
	
	conf = read_conf_from_table(conf)
	local polygons = conf:get_polygons()

	svg.set_name(svg_filename)
	local width = (math.floor((xmax - xmin) * (1 + border_size) + 1)) * zoom
	local height = (math.floor((ymax - ymin) * (1 + border_size) + 1)) * zoom
	svg.set_size(width, height)
	svg.begin_drawing()
	
	local offsetx = -xmin + (xmax - xmin) * border_size / 2.0
	local offsety = -ymin + (ymax - ymin) * border_size / 2.0
	
	for i = 1, n_gens do
		x[i] = (x[i] + offsetx) * zoom
		y[i] = (y[i] + offsety) * zoom
	end
	
	local Omat = conf:get_Omatrix_indexed()	
	print(os.date())
	
	
	for i, k in pairs(polygons.int_polygons) do
		local polygon = {}
		if (conf[k.b] % 2) == color then
			
			for i = 1, table.getn(k.l) do
				table.insert(polygon, x[k.l[i]])
				table.insert(polygon, y[k.l[i]])
			end
			
			for i = table.getn(k.r), 1, -1 do
				table.insert(polygon, x[k.r[i]])
				table.insert(polygon, y[k.r[i]])
			end
			
			svg.draw_polygon(polygon)
		end
	end
	
	local corner = {{x = 0, y = 0}, {x = 0, y = height}, {x = width, y = height}, {x = width, y = 0}}
		
	function get_tangent(l, d)
		local g1 = Omat[l][1]
		local g2 = Omat[l][table.getn(Omat[l])]
		return {x = (x[g2] - x[g1]) * d, y = (y[g2] - y[g1]) * d}
	end
	
	function cross(t1, t2)
		return t1.x*t2.y - t1.y*t2.x
	end	
	
	function sub(t1, t2)
		return {x = t1.x - t2.x, y = t1.y - t2.y}
	end	
	
	function get_lines_intersection(p1, t1, p2, t2)
		local beta = cross(sub(p1, p2), t1) / cross(t2, t1)
		return {x = p2.x + t2.x*beta, y = p2.y + t2.y*beta}
	end

	function get_border_intersection(l, d)
		local t = get_tangent(l, d)
		local ln
		for i = 1, 4 do
			local t1 = sub(corner[i], {x = x[Omat[l][1] ], y = y[Omat[l][1] ]})
			local t2 = sub(corner[i % 4 + 1], {x = x[Omat[l][1] ], y = y[Omat[l][1] ]})
			local a1 = cross(t2, t) / cross(t2, t1)
			local a2 = cross(t1, t) / cross(t1, t2)
			if a1 >= 0 and a2 >= 0 then
				ln = i
				break
			end
		end
		local res =  get_lines_intersection(
			{x = x[Omat[l][1] ], y = y[Omat[l][1] ]}, t,
			corner[ln],
			sub(corner[ln % 4 + 1], corner[ln])
		)
		return res, ln
	end

	for i, k in pairs(polygons.ext_polygons) do
		local polygon = {}
		if ((conf[k.b] or conf[k.e] or k.c) % 2) == color then
			local t, c1 = get_border_intersection(k.l2, k.d2)
			table.insert(polygon, t.x)
			table.insert(polygon, t.y)
			
			local t, c2 = get_border_intersection(k.l1, k.d1)
			
			while c1 ~= c2 do
				c2 = c2 % 4 + 1
				table.insert(polygon, corner[c2].x)
				table.insert(polygon, corner[c2].y)
			end
			
			table.insert(polygon, t.x)
			table.insert(polygon, t.y)
			
			if not k.b then
				for i = 1, table.getn(k.l) do
					table.insert(polygon, x[k.l[i] ])
					table.insert(polygon, y[k.l[i] ])
				end

				for i = table.getn(k.r), 1, -1 do
					table.insert(polygon, x[k.r[i] ])
					table.insert(polygon, y[k.r[i] ])
				end
			else
				for i = table.getn(k.r), 1, -1 do
					table.insert(polygon, x[k.r[i] ])
					table.insert(polygon, y[k.r[i] ])
				end
				
				for i = 1, table.getn(k.l) do
					table.insert(polygon, x[k.l[i] ])
					table.insert(polygon, y[k.l[i] ])
				end
			end

			svg.draw_polygon(polygon)
		end
	end
	
	svg.end_drawing()
end

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
	
	operator ==
}

poligon_num {
	out:		this
	print()
	
	operator ==
}

--]]

-- in:		string: filename; string: end generators
-- out:	array of the configurations
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

-- in:		string: generators
-- out:	configuration
function read_conf(gens)
	local line = gens
	--print(line)
	local conf = {}
	
	local _, j = string.find(line, ")")
	if j then line = string.sub(line, j) end
	
	for w in string.gfind(line, "(%d+)%s*") do
		table.insert(conf, tonumber(w)+1)
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

	setmetatable(conf, configuration_mt)	
	
	--print(N, k)
	return conf
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

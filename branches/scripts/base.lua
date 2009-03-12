-- Хранит N и k для всех конфигураций
local configurations = {__mode = "k"}

-- Герераторы отсчитываются от 1
-- Вывести список генераторов
-- Возвращает разность
function print_gens(conf)
	local s = - 1 + (configurations[conf].N % 2);
	for i, k in ipairs(conf) do
		io.write(string.format("%d ",k-1))
		s = s - ((k-1) % 2)*2 + 1;
	end
	io.write("\n")	
	if s < 0 then s = -s end
	return s
end

function print_conf(conf) 
	--print("\ngenerators:")
	local s = print_gens(conf)
	
	print("N = ", configurations[conf].N)
	print("k = ", configurations[conf].k)
	print("s = ", s)
end

function get_N(conf)
	return configurations[conf].N
end

function get_k(conf)
	return configurations[conf].k
end

-- Чтение набора конфигураций из файла
-- Возвращает массив конфигураций
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

-- Чтение конфигурации из строки генераторов
-- Возвращает конфигурацию (массив генераторов)
function read_conf(gens)
	local line = gens
	--print(line)
	local conf = {}
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
	--print(N, k)
	return conf
end

print_matrix = 0

Omatrix_mt = {
	__eq = function (o1, o2)
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
		
		local function cmp_offset(offset)
			for i = 1, n1 do
				if (table.getn(o1[i]) ~= get_line_len(offset+i)) then
					return false
				end
				for j = 1, table.getn(o1[i]) do
					if o1[i][j] ~= get_val(offset, i, j) then return false end
				end
			end
			return true
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
		local function f()
			for i = 0, 2*n1-1 do
				if o2.parallels[norm(i)] <= norm(i) then
					--[[print("i = ", i)
					for j = 1, n1 do
						for k = 1, get_line_len(j+i) do
							io.write(string.format("%2d ", get_val(i, j, k)))
						end
						print("")
					end --]]
					if cmp_offset(i) then return true end
				end
			end
		end
		if f() then return true end
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
		
		o1 = o
	
		if f() then return true end
		return false
	end
}

function get_Omatrix(conf)
	local matrix = {}
	local lines = {}
	local positions = {}
	matrix.parallels = {}
	for i = 1, get_N(conf) do
		matrix[i] = {}
		lines[i] = i
		positions[i] = 0
		matrix.parallels[i] = i
	end
	for i = 1, table.getn(conf) do
		local gen = conf[i]
		local first = lines[gen]
		local second = lines[gen+1]
		positions[first] = positions[first] + 1
		positions[second] = positions[second] + 1
		
		matrix[first][positions[first]] = second;
		lines[gen] = second;
		
		matrix[second][positions[second]] = first;
		lines[gen+1] = first;
	end
	for i = 1, get_N(conf)-1 do
		if lines[i] < lines[i+1] then
			matrix.parallels[lines[i]] = lines[i+1]
			matrix.parallels[lines[i+1]] = lines[i]
		end
	end
	--[[for i = 1, get_N(conf) do
		for j, v in ipairs(matrix[i]) do
			matrix[i][j] = matrix[i][j] - i
			if matrix[i][j] < 0 then
				matrix[i][j] = matrix[i][j] + get_N(conf)
			end
		end
	end]]
	setmetatable(matrix, Omatrix_mt)
	return matrix
end

function print_matrix(matrix)
	for i = 1, table.getn(matrix) do
		io.write(string.format("%2d ||%2d)\t", i, matrix.parallels[i] or 0))
		for k = 1, table.getn(matrix[i]) do
			io.write(string.format("%d\t", matrix[i][k]))
		end
		io.write("\n")
	end
	io.write("\n")
	return matrix
end

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

function get_polygon_num(conf)
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
	for i = 0, get_N(conf) do	
		n[i] = 0 
		is_int[i] = false
	end
	
	for i = 1, table.getn(conf) do
		local gen = conf[i]
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
	for i = 0, get_N(conf) do
		put(s.ext, n[i])
	end
	setmetatable(s, polygon_num_mt)
	return s
end

function print_polygon_num(s)
	print("External areas:")
	for i = 1, s.ext.max_n do
		print(i, s.ext[i])
	end
	print("Internal areas:")
	for i = 1, s.int.max_n do
		print(i, s.int[i])
	end
	print("\n")
	return s
end

function distance2(s1, s2)
	local sum = 0
	local function sum_it(t1, t2)
		local n = t1.max_n	
		if (t2.max_n > n) then n = t2.max_n end
		for i = 1, n do
			local v1 = t1[i] or 0
			local v2 = t2[i] or 0
			sum = sum + (v1-v2)*(v1-v2)
		end
	end
	--sum_it(s1.ext, s2.ext)
	sum_it(s1.int, s2.int)
	
	return sum
end

function distance(conf1, conf2)
	local s1 = get_polygon_num(conf1)
	local s2 = get_polygon_num(conf2)

	return distance2(s1, s2)
end

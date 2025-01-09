dofile"base.lua"

verbose = os.getenv("VERBOSE")
additional_gens = os.getenv("ADDITIONAL_GENS")
filename = os.getenv("CONF_FILE")
do_not_extract_tdc = os.getenv("DO_NOT_EXTRACT_TDC")

print("#FILE", filename)

if not do_not_extract_tdc then
	io.input(filename)
	confs = {}
	while true do
		local line = io.read()
		if line == nil then break end
		
		if additional_gens then
			line = line .. " " .. additional_gens
		end
		local conf = read_conf(line)
        
        conf:get_Omatrix():print();
        conf:get_Omatrix():rotate_sphere(1):print();
        conf:get_Omatrix():rotate_sphere(2):print();
        conf:get_Omatrix():rotate_sphere(3):print();
		

		local flag = false
		for i, k in ipairs(confs) do
			if k == conf then
				flag = true
				break
			end
		end
		
		if not flag then
			table.insert(confs, conf)
			if (verbose) then
				print("\nconf #", #confs)
				conf:print()
				conf:get_polygon_num():print()
			else
				conf:print_gens()
			end
		end
		
    end
	
else
	confs = load_confs(filename, additional_gens)
end

print("#", "s")

if verbose then
	local confs_num = {}
	
	local min_s, max_s
	min_s = confs[1]:get_s()
	max_s = min_s
	
	for i, k in ipairs(confs) do
		local s = k:get_s()
	
		print(i, s)
		
		confs_num[s] = (confs_num[s] or 0) + 1
		max_s = (max_s > s) and max_s or s
		min_s = (min_s < s) and min_s or s
	end
	
	for i = min_s, max_s do
		if confs_num[i] then
			print(string.format("s = %d, %d", i, confs_num[i]))
		end
	end
end

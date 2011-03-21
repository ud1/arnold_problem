dofile"base.lua"

filename = "11-0.txt"

confs = load_confs(filename, additional_gens)

uniq_confs = {}
groups = {}

for i, conf in ipairs(confs) do
	omat = conf:get_Omatrix()

	local flag = false
	for j, uniq_conf in ipairs(uniq_confs) do
		if conf == uniq_conf then
			flag = true
			break
		end
		
		for k = 1, conf:get_N() do
			local rt = omat:rotate_sphere(k)
			local rt_gens = rt:get_gens()
			local rt_conf = read_conf_from_table(rt_gens)
			
			if uniq_conf == rt_conf then
				flag = true
				break
			end
		end

		if flag then
			if not groups[j] then groups[j] = {} end
			table.insert(groups[j], conf)
			break
		end
	end
	
	--break
	
	if not flag then
		table.insert(uniq_confs, conf)
		conf:print()
		
		groups[#uniq_confs] = {}
		table.insert(groups[#uniq_confs], conf)
	end
end

--[[
for i = 1, #groups do
	print("Group ", i)
	for j, conf in ipairs(groups[i]) do
		conf:print()
		conf:get_polygon_num():print()
	end
end

--]]



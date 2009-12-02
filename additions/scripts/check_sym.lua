dofile"base.lua"

filename = os.getenv("CONF_FILE")

confs = load_confs(filename, additional_gens)

for i, k in ipairs(confs) do
	sym = k:get_Omatrix():check_sym()
	if sym then
		print("\nsym", sym)
		print("conf #", i)
		k:print()
		k:get_polygon_num():print()
	end
end


	
	
dofile"base.lua"

verbose = os.getenv("VERBOSE")
additional_gens = os.getenv("ADDITIONAL_GENS")
filename = os.getenv("CONF_FILE")
confs = load_confs(filename, additional_gens)

dists = {}

dist = {}
for i, k in ipairs(confs) do
	dists[i] = get_polygon_num(k)
end

dist2 = {}
dist2.num = {}

for i, k in ipairs(dists) do
	local flag = false
	for i1, k1 in ipairs(dist2) do
		if (k1 == k and get_Omatrix(confs[i]) == get_Omatrix(confs[dist2.num[i1]])
		) then
			flag = true
			break
		end
	end
	if not flag then
		table.insert(dist2, k)
		dist2.num[table.getn(dist2)] = i
		dist[table.getn(dist2)] = {}
		if (verbose) then
			print("\nconf #", table.getn(dist2))
			print_conf(confs[i])
			print_polygon_num(k)
			--print("Omatrix:\n")
			--print_matrix(get_Omatrix(confs[i]))
		else
			print_gens(confs[i])
		end
	end
end

if (verbose) then
	dists = dist2

	for i1, k1 in ipairs(dists) do
		for i2, k2 in ipairs(dists) do
		dist[i1][i2] = distance2(dists[i1], dists[i2])
		end
	end

	for i1, k1 in ipairs(dist) do
		for i2, k2 in ipairs(dist[i1]) do
			io.write(string.format("%3d ", k2))
		end
		io.write("\n");
	end

end

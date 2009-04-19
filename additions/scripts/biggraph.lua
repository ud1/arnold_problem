dofile"lines.lua"

for i = 1, table.getn(confs) do
	for i1 = i, table.getn(confs) do
		d, diff = compare_confs(confs[i]:get_Omatrix(), confs[i1]:get_Omatrix(), 1)
		if diff == 6 then
			print(i, i1)
		end	
	end
end


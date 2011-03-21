dofile"base.lua"

filename = "8.txt"

confs = load_confs(filename, additional_gens)

for i, k in ipairs(confs) do
	local mat = k:get_Omatrix()
	mat:print()
	
	print()
	
	local umat = {}
	for i = 1, #mat do
		umat[i] = {}
		for j = 1, #mat[i] do
			if mat[i][j] > i then
				umat[i][#umat[i]+1] = mat[i][j]
			end
		end
	end
	
	for i = 1, #umat do
		if #umat[i] > 1 then
			for j = 1, #umat[i]-1 do
				m = umat[i][j]
				n = umat[i][j+1]
				if m > n then
					for i_ = i, n-1 do
						for j_ = n, m-1 do
							io.write(string.format("+d%d*v%d ", i_, j_))
						end
					end
					for i_ = n, m-1 do
						for j_ = i, n-1 do
							io.write(string.format("-d%d*v%d ", i_, j_))
						end
					end
				else
					for i_ = m, n-1 do
						for j_ = i, m-1 do
							io.write(string.format("+d%d*v%d ", i_, j_))
						end
					end
					for i_ = i, m-1 do
						for j_ = m, n-1 do
							io.write(string.format("-d%d*v%d ", i_, j_))
						end
					end
				end
				io.write("> 0\n")
			end
		end
	end
end


	
	

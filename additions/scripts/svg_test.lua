
dofile"base.lua"

package.loadlib("lua_svg.dll", "init")()

os.setlocale("rus")

function convert_arn_to_svg(filename, svg_filename, border_size)
	border_size = border_size or 0.1
	
	ini = parse_ini(filename)
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

	conf = read_conf_from_table(conf)
	local polygons = conf:get_polygons()

	svg.set_name(svg_filename)
	local width = (xmax - xmin) * (1 + border_size)
	local height = (ymax - ymin) * (1 + border_size)
	svg.set_size(width, height)
	svg.begin_drawing()

	print(xmin, xmax, ymin, ymax)
	
	local offsetx = -xmin + (xmax - xmin) * border_size / 2.0
	local offsety = -ymin + (ymax - ymin) * border_size / 2.0
	
	for i, k in pairs(polygons.int_polygons) do
		local polygon = {}
		if (conf[k.b] % 2) == 0 then
			table.insert(polygon, x[k.b] + offsetx)
			table.insert(polygon, y[k.b] + offsety)

			for i = 1, table.getn(k.l) do
				table.insert(polygon, x[k.l[i]] + offsetx)
				table.insert(polygon, y[k.l[i]] + offsety)
			end

			table.insert(polygon, x[k.e] + offsetx)
			table.insert(polygon, y[k.e] + offsety)

			for i = 1, table.getn(k.r) do
				table.insert(polygon, x[k.r[i]] + offsetx)
				table.insert(polygon, y[k.r[i]] + offsety)
			end
			
			svg.draw_polygon(polygon)
		end
	end
	
	--[[ 
	local corner = {{x = 0, y = 0}, {x = 0, y = height}, {x = width, y = height}, {x = width, y = 0}}
	
	Omat = conf:get_Omatrix_indexed()
	
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
			local t1 = sub({x = x[Omat[l][0] ], y = y[Omat[l][0] ]}, corner[i])
			local t2 = sub({x = x[Omat[l][0] ], y = y[Omat[l][0] ]}, corner[i % 4 + 1]}
			local a1 = cross(t2, t} / cross{t2, t1}
			local a2 = cross(t1, t} / cross{t1, t2}
			if a1 >= 0 and a2 >= 0 then
				ln = i
				break
			end
		end
		return get_lines_intersection(
			{x = x[Omat[l][0] ], y = y[Omat[l][0] ]}, t,
			corner[ln],
			sub(corner[ln % 4 + 1], corner[ln].x)
		)
	end
	
	for i, k in pairs(polygons.ext_polygons) do
		local polygon = {}
		if ((conf[k.b] or conf[k.e] or 0) % 2) == 0 then
			table.insert(polygon, x[k.b] + offsetx)
			table.insert(polygon, y[k.b] + offsety)

			for i = 1, table.getn(k.l) do
				table.insert(polygon, x[k.l[i] ] + offsetx)
				table.insert(polygon, y[k.l[i] ] + offsety)
			end

			table.insert(polygon, x[k.e] + offsetx)
			table.insert(polygon, y[k.e] + offsety)

			for i = 1, table.getn(k.r) do
				table.insert(polygon, x[k.r[i] ] + offsetx)
				table.insert(polygon, y[k.r[i] ] + offsety)
			end
			
			svg.draw_polygon(polygon)
		end
	end
	--]]
	
	svg.end_drawing()

end

convert_arn_to_svg("65-0-3.arn", "test.svg")






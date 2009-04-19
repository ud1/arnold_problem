#include <QSvgGenerator>
#include <QPainter>

#include "luabind/luabind.hpp"
#include <vector>
#include <stdio.h>

QSvgGenerator generator;
QPainter painter;

extern "C" {
	__declspec(dllexport) int init (lua_State *L);
}

void set_name(const char *filename) {
	generator.setFileName(filename);
}

void set_size(int w, int h) {
	generator.setSize(QSize(w, h));
}

void begin_drawing() {
	painter.begin(&generator);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(Qt::black, Qt::SolidPattern));
}

void end_drawing() {
	painter.end();
}

void draw_polygon(luabind::object const& px) {
	std::vector<QPointF> points;
	if (type(px) == LUA_TTABLE) {
		for (luabind::iterator i(px), end; i != end;) {
			double x = luabind::object_cast<double> ( *(i++) );
			double y = luabind::object_cast<double> ( *(i++) );
			points.push_back(QPointF(x, y));
		}
	}
	painter.drawConvexPolygon(&points[0], points.size());
}

__declspec(dllexport) int init (lua_State *L) {
	luabind::open(L);

	luabind::module(L, "svg") [
		luabind::def("set_name", set_name),
			luabind::def("set_size", set_size),
			luabind::def("begin_drawing", begin_drawing),
			luabind::def("end_drawing", end_drawing),
			luabind::def("draw_polygon", draw_polygon)
	];

	return 0;
}



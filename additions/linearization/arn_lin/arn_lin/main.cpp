#include <stdio.h>
#include <stdlib.h>
#include "mmsvdsolveunit.h"

void R(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4, double &x, double &y) {
	double alpha = ((x3 - x1)*(y4 - y3) - (x4 - x3)*(y3 - y1)) / ((x2 - x1)*(y4 - y3) - (x4 - x3)*(y2 - y1));
	x = (x2 - x1)*alpha + x1;
	y = (y2 - y1)*alpha + y1;
}

void dR(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4, int n, double &x, double &y) {
	double temp = x2*y4 - x2*y3 - x1*y4 + x1*y3 - x4*y2 + x4*y1 + x3*y2 - x3*y1;
	temp *= temp;
	switch (n) {
		case 0: // dR/dx1
			x = (x3*y4*x4*y2-x3*y4*x4*y1+x4*y3*x3*y2-x4*y3*x3*y1+x4*y1*x2*y4
				-x4*y1*x2*y3+2*x4*y1*x3*y2-x3*y1*x2*y4+x3*y1*x2*y3-x2*y4*x4*y2
				+x2*y4*x3*y2+x2*y3*x4*y2-x2*y3*x3*y2+x3*x3*y4*y1-x3*x3*y4*y2
				-x4*x4*y3*y2+x4*x4*y3*y1-x4*x4*y1*y2-x3*x3*y1*y2-2*x4*y2*y2*x3
				+x4*x4*y2*y2+x3*x3*y2*y2)/temp;
			y = (-y2+y1)*(-y4+y3)*(-x2*y4+x2*y3+x4*y2-x3*y2+x3*y4-x4*y3)/temp;
			break;
		case 1: // dR/dy1
			x = -(-x2+x1)*(-x4+x3)*(-x2*y4+x2*y3+x4*y2-x3*y2+x3*y4-x4*y3)/temp;
			y = -(x2*y4*y4*x1+x2*y3*y3*x1+2*x2*x2*y4*y3-x2*x2*y4*y4-x2*x2*y3*y3-2*x2*y4*x1*y3
				+x2*y4*x4*y2-x2*y4*x3*y2-x2*y3*x4*y2+x2*y3*x3*y2-x1*y4*x4*y2
				+x1*y4*x3*y2+x1*y3*x4*y2-x1*y3*x3*y2-x3*y4*x2*y3+x3*y4*x1*y3
				-x4*y3*x2*y4+x4*y3*x1*y4+x3*y4*y4*x2-x4*y3*y3*x1-x3*y4*y4*x1+x4*y3*y3*x2)/temp;
			break;
		case 2: // dR/dx2
			x = -(x3*y4-x1*y4+x1*y3-x4*y3+x4*y1-x3*y1)*(x4*y2-x4*y1-x3*y2+x3*y1)/temp;
			y = -(-y2+y1)*(x3*y4-x1*y4+x1*y3-x4*y3+x4*y1-x3*y1)*(-y4+y3)/temp;
			break;
		case 3: // dR/dy2
			x = (-x2+x1)*(x3*y4-x1*y4+x1*y3-x4*y3+x4*y1-x3*y1)*(-x4+x3)/temp;
			y = (x3*y4-x1*y4+x1*y3-x4*y3+x4*y1-x3*y1)*(x2*y4-x2*y3-x1*y4+x1*y3)/temp;
			break;
		case 4: // dR/dx3
			x = (-x2+x1)*(-x1*y4*y2+x1*y3*y2+y1*x2*y4-y1*x2*y3-y4*x4*y1+y4*x2*y3
				+x4*y3*y1-y4*x1*y3+y4*x4*y2-x4*y3*y2+x1*y4*y4-x2*y4*y4)/temp;
			y = (-y2+y1)*(x1*y4*y4+y4*x2*y3-y1*x2*y3+y1*x4*y3-y4*x1*y3+y1*x2*y4
				-y2*x4*y3+y2*x1*y3-y4*x4*y1+y4*x4*y2-y2*x1*y4-x2*y4*y4)/temp;
			break;
		case 5: // dR/dy3
			x = -(-x2+x1)*(x4*x4*y2-x4*x4*y1+x2*x3*y4+x4*x3*y1-x4*x2*y4+x1*x3*y2
				-x1*x3*y4-x1*x4*y2+x2*x4*y1+x4*x1*y4-x4*x3*y2-x2*x3*y1)/temp;
			y = -(-y2+y1)*(-x4*y2*x3-x2*y4*x4-x1*y4*x3+x1*y4*x4+x1*x3*y2-x1*x4*y2
				-x3*y1*x2+x4*x3*y1+x4*y1*x2-x4*x4*y1+x4*x4*y2+x2*y4*x3)/temp;
			break;
		case 6: // dR/dx4
			x = -(-x2+x1)*(-x1*y4*y2+x1*y3*y2+y1*x2*y4-y1*x2*y3-y4*x2*y3+y4*x3*y2
				-y4*x3*y1+y4*x1*y3+y3*x3*y1-y3*x3*y2+x2*y3*y3-x1*y3*y3)/temp;
			y = -(-y2+y1)*(y3*x1*y4-y1*x2*y3+y2*x1*y3-y1*x3*y4-y3*x2*y4-y3*x3*y2
				+y1*x2*y4+y3*x3*y1+y2*x3*y4-y2*x1*y4+x2*y3*y3-x1*y3*y3)/temp;
			break;
		case 7: // dR/dy4
			x = (-x2+x1)*(-x3*x3*y2+x3*x3*y1+x4*x1*y3-x4*x3*y1+x1*x3*y2-x1*x4*y2
				-x4*x2*y3+x2*x4*y1+x4*x3*y2-x3*x1*y3+x3*x2*y3-x2*x3*y1)/temp;
			y = (-y2+y1)*(-x3*x3*y2+x3*x3*y1+x4*y2*x3+x3*x2*y3-x4*y3*x2+x4*y3*x1
				-x3*x1*y3+x1*x3*y2-x1*x4*y2-x3*y1*x2-x4*x3*y1+x4*y1*x2)/temp;
			break;
	}
}

struct pt {
	union {
		struct {
			int i1, i2, i3, i4;
		};
		int i[4];
	};

	double x, y;
};

void norm(pt * points, int n) {
	double x_min, y_min, x_max, y_max;
	x_min = x_max = points[0].x;
	y_min = y_max = points[0].y;
	for (int i = 0; i < n*(n-1)/2; ++i) {
		if (x_min > points[i].x)
			x_min = points[i].x;

		if (y_min > points[i].y)
			y_min = points[i].y;

		if (x_max < points[i].x)
			x_max = points[i].x;

		if (y_max < points[i].y)
			y_max = points[i].y;
	}
	for (int i = 0; i < n*(n-1)/2; ++i) {
		points[i].x = (points[i].x - x_min) / (x_max - x_min);
		points[i].y = (points[i].y - y_min) / (y_max - y_min);
	}
}

pt * init(int *gens, int n) {
	double l = 0.0;
	int *p = new int[n];
	int *a = new int[n];
	int *o = new int[n*(n-1)];
	int *o_ind = new int[n*(n-1)];
	pt *points = new pt[n*(n-1)/2];

	for (int i = 0; i < n; ++i) {
		p[i] = 0;
		a[i] = i;
	}
	for (int i = 0; i < n*(n-1)/2; ++i) {
		int l = a[gens[i]];
		int r = a[gens[i] + 1];
		o[l*(n-1)+p[l]] = r;
		o[r*(n-1)+p[r]] = l;
		o_ind[l*(n-1)+p[l]++] = i+1;
		o_ind[r*(n-1)+p[r]++] = -i-1;
		a[gens[i]] = r;
		a[gens[i] + 1] = l;
	}

	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n - 1; ++j) {
			int ind = o_ind[i*(n-1)+j];
			if (ind > 0) {
				points[ind-1].i1 = abs(o_ind[i*(n-1) + (j-1+n-1)%(n-1)])-1;
				points[ind-1].i2 = abs(o_ind[i*(n-1) + (j+1)%(n-1)])-1;
			} else {
				points[-ind-1].i3 = abs(o_ind[i*(n-1) + (j-1+n-1)%(n-1)])-1;
				points[-ind-1].i4 = abs(o_ind[i*(n-1) + (j+1)%(n-1)])-1;
			}
		}
	}

	for (int i = 0; i < n*(n-1)/2; ++i) {
		points[i].x = gens[i];
		points[i].y = l;
		if (i > 0 && (gens[i - 1] == gens[i] + 1 || gens[i - 1] == gens[i] - 1))
			++l;
	}

	norm(points, n);

	delete []p;
	delete []a;
	delete []o;
	delete []o_ind;

	return points;
}

int main() {
	int gens[] = {1, 3, 2, 1, 0, 1, 3, 2, 1, 3};
	int n = 5;
	pt *points = init(gens, n);

	ap::real_2d_array a;
	a.setbounds(1, n*(n-1), 1, n*(n-1)+1);

	while (1) {
		double err = 0.0;
		for (int i = 0; i < n*(n-1)/2; ++i) {
			double x, y;
			R(points[points[i].i1].x, points[points[i].i1].y,
				points[points[i].i2].x, points[points[i].i2].y,
				points[points[i].i3].x, points[points[i].i3].y,
				points[points[i].i4].x, points[points[i].i4].y, x, y);
			x -= points[i].x;
			y -= points[i].y;
			err += x*x+y*y;
			printf("%f %f\n", points[i].x, points[i].y);
		}
		printf("%f\n", err);

		for (int i = 1; i <= n*(n-1); ++i) {
			for (int j = 1; j <= n*(n-1); ++j) {
				a(i, j) = 0;
			}
		}
		for (int i = 0; i < n*(n-1)/2; ++i) {
			double x, y;
			for (int j = 0; j < 4; ++j) {
				dR(points[points[i].i1].x, points[points[i].i1].y,
					points[points[i].i2].x, points[points[i].i2].y,
					points[points[i].i3].x, points[points[i].i3].y,
					points[points[i].i4].x, points[points[i].i4].y, 2*j, x, y);
				a(2*i+1, 2*points[i].i[j]+1) = x;
				a(2*i+2, 2*points[i].i[j]+1) = y;
				dR(points[points[i].i1].x, points[points[i].i1].y,
					points[points[i].i2].x, points[points[i].i2].y,
					points[points[i].i3].x, points[points[i].i3].y,
					points[points[i].i4].x, points[points[i].i4].y, 2*j+1, x, y);
				a(2*i+1, 2*points[i].i[j] + 2) = x;
				a(2*i+2, 2*points[i].i[j] + 2) = y;
			}
		}

		for (int i = 0; i < n*(n-1)/2; ++i) {
			double x = 0.0;
			double y = 0.0;
			for (int j = 0; j < n*(n-1)/2; ++j) {
				x += a(2*i+1, 2*j+1)*points[j].x;
				x += a(2*i+1, 2*j+2)*points[j].y;
				y += a(2*i+2, 2*j+1)*points[j].x;
				y += a(2*i+2, 2*j+2)*points[j].y;
			}
			double x_, y_;
			R(points[points[i].i1].x, points[points[i].i1].y,
				points[points[i].i2].x, points[points[i].i2].y,
				points[points[i].i3].x, points[points[i].i3].y,
				points[points[i].i4].x, points[points[i].i4].y, x_, y_);
			x -= x_ - points[i].x;
			y -= y_ - points[i].y;
			a(2*i+1, n*(n-1)+1) = x;
			a(2*i+2, n*(n-1)+1) = y;
		}

		ap::real_1d_array sol;
		sol.setbounds(1, n*(n-1));
		bool res = svdsolve(a, n*(n-1), n*(n-1), sol);
		for (int i = 0; i < n*(n-1)/2; ++i) {
			points[i].x = sol(2*i+1);
			points[i].y = sol(2*i+2);
		}
		norm(points, n);
	}

	return 0;
}
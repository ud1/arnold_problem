#include <stdio.h>

struct val {
	int l, r;
};

int res;

//double F(int n, val *vals, double *params, double *grad) {
//	double f = 0.0;
//	if (grad) {
//		for (int i = 0 ; i < 2*n; ++i) {
//			grad[i] = 0;
//		}
//	}
//
//	res = 0;
//
//	for (int i = 0 ; i < n*(n-1)/2 - 1; ++i) {
//		double d1 = (params[2*vals[i].r + 1] - params[2*vals[i].l + 1]);
//		double d2 = (params[2*vals[i+1].r + 1] - params[2*vals[i+1].l + 1]);
//		double n1 = (params[2*vals[i].r] - params[2*vals[i].l]);
//		double n2 = (params[2*vals[i+1].r] - params[2*vals[i+1].l]);
//		double t1 = - n1 / d1;
//		double t2 = - n2 / d2;
//
//		if (t2 < t1)
//			++res;
//
//		t2 -= t1 + 0.1;
//
//		if (t2 < 0) {
//			f += t2*t2;
//			if (grad) {
//				grad[2*vals[i+1].r] += -2*t2/d2;
//				grad[2*vals[i+1].l] += 2*t2/d2;
//				grad[2*vals[i].r] += 2*t2/d1;
//				grad[2*vals[i].l] += -2*t2/d1;
//
//				grad[2*vals[i+1].r+1] += 2*t2*n2/(d2*d2);
//				grad[2*vals[i+1].l+1] += -2*t2*n2/(d2*d2);
//				grad[2*vals[i].r+1] += -2*t2*n1/(d1*d1);
//				grad[2*vals[i].l+1] += 2*t2*n1/(d1*d1);
//			}
//		}
//	}
//
//	return f;
//}

double eps = 0.01;

double F(int n, int *o, double *params, double *grad) {
	double f = 0.0;
	if (grad) {
		for (int i = 0 ; i < 2*n; ++i) {
			grad[i] = 0;
		}
	}

	res = 0;

	for (int i = 0 ; i < n; ++i) {
		for (int j = 0; j < n-1; ++j) {
			for (int k = j+1; k < n-1; ++k) {
				int l, r, l1, r1;
				l1 = l = i;
				r = o[l*(n-1)+j];
				r1 = o[l1*(n-1)+k];

				double d1 = (params[2*r + 1] - params[2*l + 1]);
				double d2 = (params[2*r1 + 1] - params[2*l1 + 1]);
				double n1 = (params[2*r] - params[2*l]);
				double n2 = (params[2*r1] - params[2*l1]);
				double t1 = - n1 / d1;
				double t2 = - n2 / d2;

				if (t2 < t1)
					++res;

				t2 -= t1 + eps;

				if (t2 < 0) {
					f += t2*t2;
					if (grad) {
						grad[2*r1] += -2*t2/d2;
						grad[2*l1] += 2*t2/d2;
						grad[2*r] += 2*t2/d1;
						grad[2*l] += -2*t2/d1;

						grad[2*r1+1] += 2*t2*n2/(d2*d2);
						grad[2*l1+1] += -2*t2*n2/(d2*d2);
						grad[2*r+1] += -2*t2*n1/(d1*d1);
						grad[2*l+1] += 2*t2*n1/(d1*d1);
					}
				}
			}
		}
	}

	return f;
}

int main() {
	int n = 15;
	double *params = new double[n*2];
	double *grad  = new double[n*2];
	//int gens[] = {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7};
	//int gens[] = {1, 3, 5, 7, 9, 11, 13, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 7, 9, 11, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 9, 11, 13, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 9, 8, 7, 6, 5, 7, 9, 11, 10, 9, 8, 7, 6, 5, 4, 3, 5, 7, 9, 11, 13, 12, 11, 10, 9, 11, 13, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 5, 7, 9, 8, 7, 6, 5, 7, 9, 11, 13, 15, 14, 13, 12, 11, 10, 9, 8, 7, 9, 11, 13, 12, 11, 10, 9, 11, 13, 15};
	int gens[] = {1, 3, 5, 7, 9, 11, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 7, 9, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7, 9, 8, 7, 6, 5, 7, 9, 11, 10, 9, 8, 7, 9, 11, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 3, 5, 4, 3, 5, 7, 6, 5, 7, 9, 11, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 5, 7, 9, 11, 10, 9, 8, 7, 9, 11, 13};
	int *a = new int[n];
	val *vals = new val[n*(n-1)/2];

	for (int i = 0; i < n; ++i) {
		a[i] = i;
		params[2*i] = (double) i / (double) (n-1); // position
		params[2*i+1] = -((double) i - (n - 1) / 2.0); // speed
	}
	//for (int i = 0; i < n*(n-1)/2; ++i) {
	//	vals[i].l = a[gens[i]];
	//	vals[i].r = a[gens[i]+1];
	//	a[gens[i]+1] = vals[i].l;
	//	a[gens[i]] = vals[i].r;
	//}

	int *p = new int[n];
	int *o = new int[n*(n-1)];

	for (int i = 0; i < n; ++i) {
		p[i] = 0;
		a[i] = i;
	}
	for (int i = 0; i < n*(n-1)/2; ++i) {
		int l = a[gens[i]];
		int r = a[gens[i] + 1];
		o[l*(n-1) + p[l]++] = r;
		o[r*(n-1) + p[r]++] = l;
		a[gens[i]] = r;
		a[gens[i] + 1] = l;
	}

	double *temp_params = new double[n*2];
	while(1) {
		double f;

		f = 0.0;
		f = F(n, o, params, grad);
		//for (int i = 0; i < 2*n; ++i) {
		//	for (int j = 0; j < 2*n; ++j)
		//		temp_params[j] = params[j];

		//	temp_params[i] += 0.001;
		//	double f1 = F(n, vals, temp_params, NULL);
		//	printf("%f %f\n", grad[i], f1 - f);
		//}
		if (f <= 0.0) {
			break;
		}

		//grad[0] = 0;
		//grad[2*(n-1)] = 0;

		double step = 1e20;
		for (int i = 0; i < n; ++i) {
			for (int j = i+1; j < n; ++j) {
				if (grad[2*j] - grad[2*i]) {
					double t = -(params[2*j] - params[2*i]) / (grad[2*j] - grad[2*i]);
					if (t >= 0 && step > t)
						step = t;
				}
				if (grad[2*j+1] - grad[2*i+1]) {
					double t = -(params[2*j+1] - params[2*i+1]) / (grad[2*j+1] - grad[2*i+1]);
					if (t >= 0 && step > t)
						step = t;
				}
			}
		}

		while(1) {
			double a = 0;
			double b = step*0.9;
			const double fi = 1.6180339887498948482045868343656;
			double x1 = b - (b - a)/fi;
			double x2 = a + (b - a)/fi;

			for (int i = 0; i < 2*n; ++i) {
				temp_params[i] = params[i] - x1*grad[i];
			}
			double y1 = F(n, o, temp_params, NULL);

			for (int i = 0; i < 2*n; ++i) {
				temp_params[i] = params[i] - x2*grad[i];
			}
			double y2 = F(n, o, temp_params, NULL);

			while (b - a >= 1e-10) {
				if (y1 <= y2) {
					b = x2;
					x2 = x1;
					x1 = b - (b - a)/fi;
					y2 = y1;
					for (int i = 0; i < 2*n; ++i) {
						temp_params[i] = params[i] - x1*grad[i];
					}
					y1 = F(n, o, temp_params, NULL);
				} else {
					a = x1;
					x1 = x2;
					x2 = a + (b - a)/fi;
					y1 = y2;
					for (int i = 0; i < 2*n; ++i) {
						temp_params[i] = params[i] - x2*grad[i];
					}
					y2 = F(n, o, temp_params, NULL);
				}
			}
			if (a && (y1 < f)) {
				for (int i = 0; i < 2*n; ++i) {
					params[i] -= a*grad[i];
				}
				printf("res %d %e %f %f\n", res, y1, step*0.9, a);

				static int fnum = 0;
				fnum = !fnum;

				FILE *file;
				char filename[256];
				sprintf(filename, "out%d.lines", fnum);
				file = fopen(filename, "wb");
				fwrite(&n, sizeof(n), 1, file);
				int temp = 0;
				fwrite(&temp, sizeof(temp), 1, file);
				int i;
				for (i=0; i<n; ++i) {
					double a, b;
					a = params[2*i] + 1;
					b = a / params[2*i+1];
					a = 1/a;
					b = 1/b;
					fwrite(&a, sizeof(a), 1, file);
					fwrite(&b, sizeof(b), 1, file);
				}
				fclose(file);

				break;
			} else {
				step /= 2;
			}
		}
	}

	return 0;
}
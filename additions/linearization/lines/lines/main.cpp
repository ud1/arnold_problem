#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#include "principalaxis.h"


const double infinity = 100000.0;
const double epsilon = 1.0/infinity;
const double Rad = 100.0;
double params[3];

int gN_vals;
double *g_vals;
double (*g_potencial)();

double f(ap::real_1d_array x) {
	int i;
	for (i = 0; i < gN_vals; ++i)
		g_vals[i] = x(i+1);

	return g_potencial();
}

void minimize( double (*potencial)(), double *vals, int N) {
	//int i;
	//ap::real_1d_array x;
	//gN_vals = N;
	//x.setlength(N+1);
	//for (i = 0; i < N; ++i)
	//	x(i+1) = vals[i];

	//g_vals = vals;
	//g_potencial = potencial;
	//principalaxisminimize(N, x, 0.001, 10);

	//for (i = 0; i < N; ++i)
	//	vals[i] = x(i+1);

	static int _N = 0;
	double * _grad = new double [N];
	//if (_N < N) {
	//	if (_grad)
	//		delete []_grad;
	//	_grad = new double [N];
	//	_N = N;
	//}
	int i;
	double pot = potencial();
	double oldPot = pot;
	int changes;
	do {
		changes = 0;
		double maxV = 0.0;
		for (i=0; i<N; ++i) {
			double old = vals[i];
			vals[i] += 0.01;
			_grad[i] = (potencial()-pot)/0.001;
			if (_grad[i] > maxV)
				maxV = _grad[i];
			if (-_grad[i] > maxV)
				maxV = -_grad[i];
			vals[i] = old;
		}
		if (maxV) {
			do {
				for (i=0; i<N; ++i) {
					vals[i] -= _grad[i]/maxV*0.001;
				}
				changes++;
				/*double rad = 10.0;
				int ch = 0;
				do {
					do {
						for (i=0; i<N; ++i) {
							vals[i] -= _grad[i]/maxV*rad;
						}
						oldPot = pot;
					} while ((pot = potencial()) < oldPot);
					pot = oldPot;
					do {
						for (i=0; i<N; ++i) {
							vals[i] += _grad[i]/maxV*rad;
						}
						oldPot = pot;
					} while ((pot = potencial()) < oldPot);
					pot = oldPot;
					for (i=0; i<N; ++i) {
						vals[i] -= _grad[i]/maxV*rad;
					}
					rad /= 2.0;
				} while (rad > 0.0001);*/


				oldPot = pot;
			} while ((pot = potencial()) < oldPot);
			for (i=0; i<N; ++i) {
				vals[i] += _grad[i]/maxV*0.001;
			}
			changes--;
		}
		oldPot = pot;
	} while (changes);
	delete []_grad;
}

void minimize2( double (*potencial)(), double *vals, int N) {
	int i;
	double rad = 10.0;
	double pot = potencial();
	double oldPot = pot;
	int changes;
	while (rad > 0.0001) {
		do {
			changes = 0;
			for (i=0; i<N; ++i) {
				do {
					vals[i] += rad;
					changes++;
					oldPot = pot;
				} while ((pot = potencial()) < oldPot);
				vals[i] -= rad;
				changes--;
				pot = oldPot;
				do {
					vals[i] -= rad;
					changes++;
					oldPot = pot;
				} while ((pot = potencial()) < oldPot);
				vals[i] += rad;
				changes--;
				pot = oldPot;
			}
			//pot = potencial();
		} while (changes);
		rad /= 2.0;
	}
}

double eps(double f) {
	if (f < epsilon && f >= 0)
		return epsilon;
	if (f > -epsilon && f <= 0)
		return -epsilon;
	return f;
}

struct configuration {
	configuration() {
		matrix = NULL;
		N = 0;
		k = 0;
		vals = NULL;
	}
	int **matrix;
	int N, k;
	double *vals;
	void getAB(double &a, double &b, int i) {
		if (i < 2*k) {
			if (!(i % 2)) {
				a = 1.0/eps(vals[i])*sin(vals[i+1]);
				b = 1.0/eps(vals[i])*cos(vals[i+1]);
			} else {
				double koef = vals[2*(N-k)+(i/2)];
				a = 1.0/eps(/*vals[i-1] **/ koef)*sin(vals[i]);
				b = 1.0/eps(/*vals[i-1] **/ koef)*cos(vals[i]);				
			}
			return;
		} else {
			a = 1.0/eps(vals[2*(i-k)])*sin(vals[2*(i-k)+1]);
			b = 1.0/eps(vals[2*(i-k)])*cos(vals[2*(i-k)+1]);
			return;
		}
	}
	void reset() {
		// vals[ 0..(N-k)*2-1 ] - a,b coeficients;
		// vals[ (N-k)*2..N-1 ] - multipliers a,b = a,b * mult
		// line : (a,b)*(x,y) = 1;
		int i;
		for (i=0; i<N-k; ++i) {
			vals[2*i] = 1.0;
			vals[2*i+1] = M_PI/(N-k)*i;
		}
		for (i=0; i<k; ++i) {
			vals[(N-k)*2+i] = -1;
		}
	}
} conf;



double position(int i, int j, double &x, double &y, double &cosAlpha2) {
	double ai, aj, bi, bj;

	conf.getAB(ai, bi, i);
	conf.getAB(aj, bj, j);

	double det = ai*bj-aj*bi;
	cosAlpha2 = 1;
	x = y = infinity;
	if (det < epsilon && det > -epsilon) {
		return infinity;
	}

	double leni = sqrt(ai*ai + bi*bi);
	if (leni < epsilon)
		return infinity;

	double lenj = sqrt(aj*aj + bj*bj);
	if (lenj < epsilon)
		return infinity;

	x = (bj - bi)/det;
	y = (ai - aj)/det;
	
	ai /= leni;
	aj /= lenj;
	bi /= leni;
	bj /= lenj;
	cosAlpha2 = (ai * aj + bi * bj);
	cosAlpha2 *= cosAlpha2;

	return ai * y - bi * x;
}

double pointsPotencial(double rad) {
	if (rad < 0)
		return exp(-rad*params[1]);

	return exp(rad/params[0]);

	if (rad < 0)
		return -rad;
	//return 0;
	return rad/100;
	double r = 10*(rad-1.0);
	double r2 = r * r;
	if (r < 0.5)
		return r2*r2*r2*0.05*params[0]*params[0];
	return /*r2*/pow(r, 2.5)/1.0;
	//if (r < 0)
	//	return params[0]*params[0]*r;
	//return r;
}

double linesPotencial(double cosAlpha2) {
	return 0;
	return /*params[1]*params[1]*/3000.0/(1.0+epsilon-cosAlpha2);
}

double radialPotencial(double _rad2) {
	return 0.0;
	return _rad2;
}

double potencial() {
	int i, j;
	double pot = 0.0;
	for (i=0; i<conf.N; ++i) {
		int lastN;
		if (i < 2*conf.k)
			lastN = conf.N-2;
		else lastN = conf.N-1;
		double xi, yi, cosAlpha2;
		double lastPoint = position(i, conf.matrix[i][lastN-1], xi, yi, cosAlpha2);
		double prevPos = position(i, conf.matrix[i][0], xi, yi, cosAlpha2);
		double sign = lastPoint - prevPos;
		if (sign > 0.0)
			sign = 1.0;
		else sign = -1.0;

		pot += linesPotencial(cosAlpha2);
		pot += radialPotencial(xi*xi + yi*yi);
		for (j=1; j<lastN; ++j) {
			double xj, yj;
			double pos = position(i, conf.matrix[i][j], xj, yj, cosAlpha2);
			pot += pointsPotencial((pos - prevPos)*sign);
			pot += linesPotencial(cosAlpha2);
			pot += radialPotencial(xj*xj + yj*yj);
			prevPos = pos;
			//printf("%f\n", pot);
		}

		//double ai, bi;
		//conf.getAB(ai, bi, i);
		//double len2 = (ai*ai+bi*bi);
		//if (len2 < epsilon)
		//	pot += infinity;
		//else pot += exp(-len2);
	}
	return pot;
}

void setConfiguration(int N, int k, const int *generators) {
	if (conf.vals)
		delete []conf.vals;
	if (conf.matrix) {
		int i;
		for (i=0; i<conf.N; ++i)
			delete [] conf.matrix[i];
		delete []conf.matrix;
	}
	conf.vals = new double[N*2-k];
	conf.N = N;
	conf.k = k;
	conf.matrix = new int*[N];
	int i;
	for (i=0; i<N; ++i) {
		conf.matrix[i] = new int[N-1];
	}

	int *lines = new int[N];
	int *positions = new int[N];
	for (i=0; i<N; ++i) {
		lines[i] = i;
		positions[i] = 0;
	}
	for (i=0; i<N*(N-1)/2-k; ++i) {
		int gen = generators[i];
		int first, second;
		first = lines[gen];
		second = lines[gen+1];
		conf.matrix[first][positions[first]++] = lines[gen] = second;
		conf.matrix[second][positions[second]++] = lines[gen+1] = first;
	}
	delete []lines;
	delete []positions;
	conf.reset();
}

void calc(double &maxV, double &minV, double &maxCosAlpha2, double &minCosAlpha2) {
	maxV = 0.0;
	minV = infinity;
	maxCosAlpha2 = 0.0;
	minCosAlpha2 = 1.0;

	int i, j;
	double x, y, cosAlpha2;
	for (i=0; i<conf.N; ++i) {
		int lastN;
		if (i < 2*conf.k)
			lastN = conf.N-2;
		else lastN = conf.N-1;
		double lastPoint = position(i, conf.matrix[i][lastN-1], x, y, cosAlpha2);
		double prevPos = position(i, conf.matrix[i][0], x, y, cosAlpha2);
		if (maxCosAlpha2 < cosAlpha2)
			maxCosAlpha2 = cosAlpha2;
		if (minCosAlpha2 > cosAlpha2)
			minCosAlpha2 = cosAlpha2;
		double sign = lastPoint - prevPos;
		if (sign > 0.0)
			sign = 1.0;
		else sign = -1.0;
		printf("----line %d -----\n", i);
		for (j=1; j<lastN; ++j) {
			double pos = position(i, conf.matrix[i][j], x, y, cosAlpha2);
			//pot += pairPotencial(pos - prevPos);
			double v = (pos - prevPos)*sign;
			printf("%f\n", v);
			if (maxV < v)
				maxV = v;
			if (minV > v)
				minV = v;
			prevPos = pos;
		}
	}
}

double potencial2() {
	conf.reset();
	minimize(potencial, conf.vals, conf.N*2-conf.k);
	int i = 6;

	//minimize(potencial, conf.vals, conf.N*2-conf.k - i*2);
	//minimize(potencial, conf.vals+i*2, i*2);
	//minimize(potencial, conf.vals, conf.N*2-conf.k);

	//for (i = 14; i <= conf.N; ++i) {
	//	minimize(potencial, conf.vals, (i - 1)*2);
	//	minimize(potencial, conf.vals + (i - 1) * 2, 2);
	//	minimize(potencial, conf.vals, i*2);
	//}
	double maxV;
	double minV;
	double maxCosAlpha2;
	double minCosAlpha2;
	calc(maxV, minV, maxCosAlpha2, minCosAlpha2);
	double pot = 0.0;
	double r = (minV - 1.0f)*10.0;
	double r2 = r*r;
	if (r < 0)
		pot += r2*r2;
	else pot += r2;
	const double koef = 100.0;
	//if (minV < epsilon)
	//	pot += koef*maxV/epsilon;
	//else pot += koef*maxV/minV;
	//double minAlpha = acos(sqrt(maxCosAlpha2))*180.0/M_PI;
	//double deltaAlpha = (minAlpha-7.0)*10;
	//if (deltaAlpha < 0.0)
	//	pot += deltaAlpha*deltaAlpha;
	printf("pot = %f\n", pot);
	return pot;
}



int main(int argc, char **argv) {
	params[0] = 10.0;
	params[1] = 1.0;
	params[2] = 1.0;
	int *generators;
	if (argc > 2) {
		sscanf(argv[1], "%d", &conf.N);
		sscanf(argv[2], "%d", &conf.k);
		int count = conf.N*(conf.N-1)/2-conf.k;
		generators = new int[count];
		int i;
		for (i=0; i<count; ++i) {
			sscanf(argv[i+3], "%d", &generators[i]);
		}
		setConfiguration(conf.N, conf.k, generators);
	} else return 0;
	//int generators[] = {1,3,2,1,3,5,4,3,5,7,6,5,7,8,7,6,5,4,3,2,1,3,5,7,9,10,9,8,7,6,
	//	5,4,3,2,1,0,1,3,5,7,9,8,7,6,5,4,3,2,1,3,5,4,3,5,7,6,5,7,9,8,7,9};
	//setConfiguration(12, 4, generators);
/*	int generators[] = {1,3,2,1,3,5,4,3,5,7,6,5,4,3,5,7,9,8,7,6,5,7,9,11,10,9,8,7,6,5,4,3,2,1,0,1,3,5,4,3,2,1,3,5,7,
		9,11,12,11,10,9,8,7,6,5,4,3,2,1,3,5,7,6,7,9,8,7,9,11,10,9,8,7,6,5,4,3,5,7,6,5,7,9,11};
	setConfiguration(14, 7, generators);*/	
	//int generators[] = {0,1,0,2,4,3,2,1,0,2,4,5,4,6,8,7,6,5,4,3,2,1,0,2,4,6,5,4,3,2,4,6,
	//	8,9,8,7,6,5,4,3,2,1,0,2,4,6,8,7,6,5,4,3,2,4,6};
	//setConfiguration(11, 0, generators);
	//int generators[] = {0,2,1,0,2,4,3,2,4,6,5,4,3,2,1,0,2,4,6,7,6,5,4,3,2,1,0,2,4,6,5,4,3,2,4,6};
	//setConfiguration(9, 0, generators);
	//int generators[] = {0,1,0,2,4,3,2,1,0,2,4,5,4,3,2,1,0,2,4,3,2};
	//setConfiguration(7, 0, generators);
	//int generators[] = {0,2,1,0,2,3,2,4,6,5,4,6,8,7,6,5,4,3,2,4,6,8,10,9,8,7,6,
	//	5,4,3,2,1,0,2,4,6,8,10,11,10,9,8,7,6,5,4,3,2,1,0,2,4,6,8,10,9,8,7,6,5,4,3,
	//	2,4,6,8,7,6,5,4,6,8,10,9,8,7,8,10};
	//setConfiguration(13, 0, generators);
	minimize2(potencial2, params, 2);
	double maxV;
	double minV;
	double maxCosAlpha2;
	double minCosAlpha2;
	calc(maxV, minV, maxCosAlpha2, minCosAlpha2);
	printf("maxV = %f, minV = %f\n", maxV, minV);
	printf ("maxV/minV = %f, maxCosAlpha2 = %f, minCosAlpha2 = %f\n", maxV/minV, maxCosAlpha2, minCosAlpha2);
	printf ("minAngle = %f, maxCosAlpha2/minCosAlpha2 = %f\n", acos(sqrt(maxCosAlpha2))*180.0/M_PI, maxCosAlpha2/minCosAlpha2);
	if (1/*minV > 0.0*/) {
		FILE *file;
		char filename[256];
		sprintf(filename, "%d-%d.lines", conf.N, conf.k);
		file = fopen(filename, "wb");
		fwrite(&conf.N, sizeof(conf.N), 1, file);
		fwrite(&conf.k, sizeof(conf.k), 1, file);
		int i;
		for (i=0; i<conf.N; ++i) {
			double a,b;
			conf.getAB(a, b, i);
			fwrite(&a, sizeof(a), 1, file);
			fwrite(&b, sizeof(b), 1, file);
		}
		int count = conf.N*(conf.N-1)/2-conf.k;
		fwrite(&generators[0], sizeof(generators[0]), count, file); 
		fclose(file);
	}
	system("PAUSE");
	return 0;
}
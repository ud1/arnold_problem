#include <stdio.h>
#include <stdlib.h>
#include <map>

#define MAXN 100

std::map<int, double> cash;

double v (int n, int l) {
	int i;
	double sum = 0.0;
	if (n == 1) {
		if (l == 1) {
			return 1.0;
		}
		return 0.0;
	}
	if (cash.count(n*MAXN + l))
		return cash[n*MAXN + l];
	for (i = 0; i < n; ++i)
		sum += v(n-1, l - i);

	cash[n*MAXN + l] = sum;
	return sum;
}

int main(int argc, char **argv) {
	int n, l;
	if (argc == 2) {
		n = atoi(argv[1]);
		l = n*(n-1)/4;
		printf("%f\n", v(n, l));
	} else if (argc == 3) {
		n = atoi(argv[1]);
		l = atoi(argv[2]);
		printf("%f\n", v(n, l));
	}

	return 0;
}
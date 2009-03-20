#include <stdio.h>
#include <stdlib.h>

double v (int n, int l) {
	int i;
	double sum = 0.0;
	if (n == 1) {
		if (l == 1) {
			return 1.0;
		}
		return 0.0;
	}
	for (i = 0; i < n; ++i)
		sum += v(n-1, l - i);
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
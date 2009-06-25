#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define N 9

#define NG ((N-1)*N/2)

int ar[NG][N];
int rear[NG];
int poly[N+1];

int main() {
	int i, j, res, lev, maxval, maxval_j;
	res = 0;
	lev = 0;
	for (i=0; i<N; ++i) {
		ar[0][i] = i;
	}
	for (i=0; i<=N; ++i) {
		poly[i] = 2;
	}

	for (i=1; i < NG && lev<NG;) {
		//for (j=0; j<N; ++j) {
		//	printf("%2d ", ar[i][j]);
		//}
		//printf("\n");
		memcpy(ar[i], ar[i-1], sizeof(ar[i]));
		for (j=0; j<N-1; j+=2) {
			if (ar[i-1][j] < ar[i-1][j+1]) {
				ar[i][j] = ar[i-1][j+1];
				ar[i][j+1] = ar[i-1][j];
				++res;
				++lev;
				poly[j+1] = 1;
				++poly[j];
				++poly[j+2];
				printf("%d\n", res);
			}
		}
		for (j=0; j<N; ++j) {
			printf("%2d ", ar[i][j]);
		}
		printf("\n");
		for (j=1; j<N-1; ++j) {
			printf("%2d ", poly[j]);
		}
		printf("\n");
		++i;
		memcpy(ar[i], ar[i-1], sizeof(ar[i]));
		maxval = -1;
		maxval_j = -1;
		for (j=1; j<N-1; j+=2) {
			if (ar[i-1][j] < ar[i-1][j+1]) {
				int val = poly[j+1];
				if (ar[j-1] < ar[j+1])
					++val;
				if (j+2 < N && ar[j] < ar[j+2])
					++val;
				if (val >= maxval) {
					maxval = val;
					maxval_j = j;
				}
			}
		}
		if (ar[i-1][maxval_j] < ar[i-1][maxval_j+1]) {
			ar[i][maxval_j] = ar[i-1][maxval_j+1];
			ar[i][maxval_j+1] = ar[i-1][maxval_j];
			--res;
			++lev;
			poly[maxval_j+1] = 1;
			++poly[maxval_j];
			++poly[maxval_j+2];
			printf("%d\n", res);
		}
		for (j=0; j<N; ++j) {
			printf("%2d ", ar[i][j]);
		}
		printf("\n");
		for (j=1; j<N-1; ++j) {
			printf("%2d ", poly[j]);
		}
		printf("\n");
		++i;
	}

	printf("%d\n", res);
	system("PAUSE");
	return 0;
}
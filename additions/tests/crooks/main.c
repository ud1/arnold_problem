#include <stdlib.h>
#include <stdio.h>
#include <time.h>


int N, trN;
typedef unsigned char byte;
typedef struct {
	byte l1, l2, l3;
} triplet;

triplet *triplets;
int *trInd;
byte *bmatrix;
typedef __int64 INT64;
INT64 *stat, statCnt;

byte addTr(byte a, byte b, byte c) {
	if (bmatrix[a*N+b] >= 2)
		return 0;
	if (bmatrix[a*N+c] >= 2)
		return 0;
	if (bmatrix[b*N+c] >= 2)
		return 0;
	bmatrix[a*N+b]++;
	bmatrix[a*N+c]++;
	bmatrix[b*N+c]++;
	return 1;
}

int step() {
	int i, j, res = 0;
	for (i=0; i<trN; ++i)
		trInd[i] = i;
	for (i=0; i<N; ++i) {
		for (j=0; j<N; ++j) {
			bmatrix[i*N+j] = 0;
			if (i+1 == j)
				bmatrix[i*N+j] = 1;
		}
	}
	bmatrix[0*N+(N-1)] = 1;

	for (i=trN; i>0; --i) {
		int tr;
		int rnd = rand() % i;
		tr = trInd[rnd];
		if (addTr(triplets[tr].l1, triplets[tr].l2, triplets[tr].l3)) ++res;
		trInd[rnd] = trInd[i-1];
	}
	//for (i=0; i<N; ++i) {
	//	for (j=0; j<N; ++j) {
	//		printf("%d ",bmatrix[i*N+j]);
	//	}
	//	printf("\n");
	//}
	return res;
}

int main() {
	int i, j, k, ind, res, maxres, maxposible;
	char tmpbuf[128];
	FILE *file;
	N = 19;

	trN = N*(N-1)*(N-2)/6;
	triplets = (triplet *) malloc(trN*sizeof(triplet));
	ind = 0;
	for (i=0; i<N; ++i) {
		for (j=i+1; j<N; ++j) {
			for (k=j+1; k<N; ++k) {
				triplets[ind].l1 = i;
				triplets[ind].l2 = j;
				triplets[ind].l3 = k;
				++ind;
			}
		}
	}
	trInd = (int *) malloc(trN*sizeof(int));
	bmatrix = (byte *) malloc(N*N*sizeof(byte));

	srand( (unsigned)time( NULL ) );
	maxres = 0;
	maxposible = N*(N-2)/3;
	stat = (INT64 *) malloc(maxposible * sizeof(INT64));
	for (i=0; i<maxposible; ++i)
		stat[i] = 0;
	file = fopen("out.csv", "w");
	if (!file)
		return 0;
	printf("maxposible = %d\n", maxposible);
	statCnt = 0;
	while (1) {
		if (maxres < (res = step())) {
			maxres = res;
			_strdate(tmpbuf);
			printf("%s ", tmpbuf);
			_strtime(tmpbuf);
			printf("%s) ", tmpbuf);
			printf("%d\n", res);
		}
		++stat[res];
		++statCnt;
		if (!(statCnt % 100000)) {
			for (i=0; i<maxposible; ++i)
				fprintf(file, "%d;", stat[i]);
			fprintf(file, "\n");
			fflush(file);
		}
		if (maxposible <= maxres)
			break;
	}
	system("PAUSE");
	return 0;
}
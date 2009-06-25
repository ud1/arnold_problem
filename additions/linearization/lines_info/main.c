#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int N_gen, N, k;
int shift;
int *generators;

int parseParam(char ** argv) {
	int i;
	if (!strcmp(argv[0], "-s")) {
		if (!sscanf(argv[1], "%d", &shift)) {
			shift = 1;
			return 0;
		}
		return 1;
	}
	if (!strcmp(argv[0], "-g")) {
		int maxGen = 0;
		i = 1;
		while (argv[i] && argv[i][0] != '-')
			++i;
		N_gen = i - 1;
		if (generators)
			free(generators);
		generators = malloc(sizeof(generators[0])*N_gen);
		for (i=1; i<=N_gen; ++i) {
			sscanf(argv[i], "%d", &generators[i-1]);
			generators[i-1] -= shift;
			if (maxGen < generators[i-1])
				maxGen = generators[i-1];
		}
		N = maxGen + 2;
		k = N*(N-1)/2 - N_gen;
		return N_gen;
	}
	if (!strcmp(argv[0], "-i")) {
		int res = 0;
		printf("%d lines, %d parallel pairs\n\n", N, k);
		printf("generators:\n  ");
		for (i=0; i<N_gen; ++i) {
			printf("%2d ", generators[i]);
			if (!((i+1) % 10))
				printf("\n  ");
			(generators[i] % 2) ? ++res: --res;
		}
		printf("\n\n");
		res -= ((N+1) % 2);
		res = (res > 0) ? res : -res;
		printf("result value %d\n\n", res);
		return 0;
	}
	if (!strcmp(argv[0], "-matrix")) {
		int ret;
		int **matrix;
		int *lines;
		int *positions;
		int j;
		matrix = malloc(sizeof(int *)*N);
		for (i=0; i<N; ++i) {
			matrix[i] = malloc(sizeof(int)*(N-1));
		}

		lines = malloc(sizeof(int)*N);
		positions = malloc(sizeof(int)*N);
		for (i=0; i<N; ++i) {
			lines[i] = i;
			positions[i] = 0;
		}
		for (i=0; i<N_gen; ++i) {
			int gen = generators[i];
			int first, second;
			first = lines[gen];
			second = lines[gen+1];
			matrix[first][positions[first]++] = lines[gen] = second;
			matrix[second][positions[second]++] = lines[gen+1] = first;
		}
		printf("matrix:\n");
		for (i=0; i<N; ++i){
			printf("  %2d)", i);
			for (j=0; j<positions[i]; ++j) {
				printf("%2d ", matrix[i][j]);
			}
			printf("\n");
		}
		printf("\n");
		ret = 0;
		if (!strcmp(argv[1], "sergey")) {
			ret = 1;
			printf("sergey matrix:\n");
			for (i=0; i<N; ++i){
				printf("  %2d)", i);
				for (j=0; j<positions[i]; ++j) {
					printf("%2d ", matrix[i][j] > i ? 0 : 1);
				}
				printf("\n");
			}
			printf("\n");
		}
		if (argv[1] && !strcmp(argv[1], "all")) {
			int k;
			int sflag = 0;
			if (argv[2] && !strcmp(argv[2], "sergey")) {
				sflag = 1;
				ret = 2;
			} else sflag = 0;
			for (k=1; k<N; ++k) {
				printf("rotation %d:\n", k);
				for (i=k; i<N; ++i){
					printf("  %2d)", i-k);
					for (j=0; j<positions[i]; ++j) {
						if (!sflag)
							printf("%2d ", (matrix[i][j] + N - k) % N);
						else printf("%2d ", (matrix[i][j] + N - k) % N > (i-k) ? 0 : 1);
					}
					printf("\n");
				}
				for (i=0; i<k; ++i){
					printf("  %2d)", N-k+i);
					for (j=0; j<positions[i]; ++j) {
						if (!sflag)
							printf("%2d ", (matrix[i][positions[i]-j-1] + N - k) % N);
						else printf("%2d ", (matrix[i][positions[i]-j-1] + N - k) % N > (N-k+i) ? 0 : 1);
					}
					printf("\n");
				}
				printf("\n");
			}
		}
		free(lines);
		free(positions);
		for (i=0; i<N; ++i) {
			free(matrix[i]);
		}
		free(matrix);
		return ret;
	}
	if (!strcmp(argv[0], "-metro")) {
		int j;
		int *l = malloc(sizeof(int)*N);
		for (i=0; i<N; ++i) {
			l[i] = i;
		}
		for (i=0; i<N_gen; ++i) {
			int gen = generators[i];
			int temp = l[gen];
			l[gen] = l[gen+1];
			l[gen+1] = temp;
		}

		printf("metro:\n");
		for (j=0; j<N; ++j) {
				printf("%2d", j);
		}
		printf("\n ");
		for (j=0; j<N; ++j) {
			printf("| ");
		}
		printf("\n ");
		for (i=0; i<N_gen;++i) {
			for (j=0; j<N; ++j) {
				if (generators[i] == j) {
					printf(" >");
				} else if (generators[i] == j-1) {
					printf("< ");
				} else if (i+1 < N_gen && generators[i] == j-2 && generators[i+1]-2 >= generators[i]) {
					++i;--j;
				} else {
					printf("| ");
				}
			}
			printf("\n ");
		}
		for (j=0; j<N; ++j) {
			printf("%2d", l[j]);
		}
		printf("\n\n");
	}
	return 0;
}

int main(int argc, char ** argv) {
	int i;
	if (argc == 1) {
		return 0;
	}
	shift = 0;
	N = k = 0;
	N_gen = 0;
	for (i=1; i<argc; ++i) {
		if (argv[i][0] == '-')
			i += parseParam(&argv[i]);
	}
	//system("PAUSE");
	return 0;
}
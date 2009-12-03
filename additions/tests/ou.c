#include <stdio.h>

#define max_n 9

int u[max_n][max_n-1], o[max_n][max_n-1], p[max_n][max_n];

int n = 6;

void print_m() {
	int i, j;
	for (i=0; i < n; ++i) {
		for (j=0; j < n-1; ++j)
			printf("%d ", o[i][j]);
		printf("\n");
	}
}

void generate_o () {
	int i, j, k, l;
	
	for (i = n; i--; )
		for (j = i; u[i][j] && j < n-1; ++j)
			p[i][j] = 1;
	
	for (i = n; i--; )
		for (j = 0; u[i][j] && j < i; j++)
			p[i][u[i][j] - 1] = j;

	for (i = n; i--; )
		for (j = 0; u[i][j] && j < i; j++)
			for (k = 0; k < j; k++)
				if (u[i][k] > u[i][j] )
					p[u[i][k] - 1][u[i][j] - 1]++;
	
	for (i = n; i--; )
		for (j = 0; u[i][j] && j < i; j++)
			o[i][ p[i][ u[i][j]-1 ] ] = u[i][j];
	
	for (l = n-1; l--; ) {
		for (i = n; i--; )
			for (j = n - 1; j > l; j--)
				if (i == j) {
					if (p[j][l] == -1)
						continue;
					for (k = 0; k < p[j][l]; k++ )
						if (o[j][k] < l + 1 || o[j][k] > i + 1)
							p[l][i]++;
				}
				else {
					if (j < i && p[j][l] < p[j][i])
						p[l][i]++;
				}
		
		for (j = l + 1; j < n; ++j)
			o[l][p[l][j]] = j + 1;
	}
	print_m();
}

int main () {
	int i, j;
	
	for (i = n; i--; )
		for (j = n-1; j--;)
			u[i][j] = 0;
	
	u[1][0] = 1;
	
	u[2][0] = 2;
	u[2][1] = 1;
	
	u[3][0] = 1;
	u[3][1] = 2;
	u[3][2] = 3;
	
	u[4][0] = 4;
	u[4][1] = 2;
	u[4][2] = 1;
	u[4][3] = 3;
	
	u[5][0] = 1;
	u[5][1] = 2;
	u[5][2] = 4;
	u[5][3] = 3;
	u[5][4] = 5;

	generate_o();
	
/*	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;
	u[][] = ;*/
	return 0;
}
#include <stdio.h>
#include <string.h>

// May vary
#define n1 39
#define n_step1 (n1*(n1-1) / 2)

#define plurality 3

// May vary
#define plurality_def 8

#define rearrangement_count 3

int n, k=-1, n_step;

int triplets_def[rearrangement_count][plurality_def];
int triplets[rearrangement_count][plurality]; //= {{1, 2, -1}, {2, -1, 1}, {-1, 2, 1}};

typedef struct {
	int defects, generator, stack;
	int rearrangement;
	int rearr_index;
} Stat;

Stat stat[n_step1 + 1];

int a[n1], d[n1 + 1];

int b_free;
int max_s = 0, max_defects = 0;

// User input
char filename[80] = "";
int max_defects_default = 0;
int full = 0;

void count_gen(int level) {
	int s, i, max_defects1;
	FILE *f;

	s = - 1 + (n & 1);
	for(i = 1; i <= level + 1; i++) {
		s -= (stat[i].generator & 1)*2 - 1;
	}
	if (s < 0)
		s = -s;
	if (s < max_s || !full && s == max_s)
		return;

	max_s = s;
	max_defects1 = ((n*(n+1)/2 - k - 3) - 3*max_s)/2;

	if (strcmp(filename, "") == 0) {
		printf(" %d %d %d)", s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			printf(" %d", stat[i].generator);
		}

		printf("\n");

		fflush(NULL);
		fflush(NULL);
		fflush(stdout);
		fflush(stdout);
	}
	else {
		f = fopen(filename, "a");

		fprintf(f, " %d %d %d)", s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			fprintf(f, " %d", stat[i].generator);
		}

		fprintf(f, "\n");
		fclose(f);
	}
	return;
}

void inline set (int curr_generator, int t) {
	int i;

	b_free += -2*t+1;
	i = a[curr_generator];
	a[curr_generator] = a[curr_generator + 1];
	a[curr_generator + 1] = i;
}

// Calculations for defectless configurations
void calc (int level) {
	int curr_generator, direct, i;

	max_s = 0;

	stat[level].rearr_index = -1;
	for (i = n + 1; i--; ) {
		d[i] = 2;
	}

	while (1) {
		if (stat[level].rearr_index < plurality - 1) {
			stat[level].rearr_index++;
			curr_generator = stat[level].generator + (direct = triplets[stat[level].rearrangement][stat[level].rearr_index]);
			if (curr_generator > n-2 || curr_generator < 0)
				continue;
			if (a[curr_generator] > a[curr_generator + 1])
				continue;
			stat[level + 1].generator = curr_generator;
			if (!b_free) {
				count_gen(level);
				continue;
			}
			// Optimization
			if (!(curr_generator % 2)) { // even, white
				// disallow black squares
				if (d[curr_generator] == 1)
					continue;
				if (d[curr_generator + 2] == 1)
					continue;

				// without defects, disallow white triangles and squares
				if (d[curr_generator + 1] < 3)
					continue;
			}
			stat[level+1].stack = d[curr_generator + 1];
			d[curr_generator + 1] = 0;
			d[curr_generator]++;
			d[curr_generator + 2]++;
			set(curr_generator, 1);
			level++;
//			stat[level].rearrangement = direct > 0 ? 1 : 2;
			//Euristics
			if (direct > 0) {
				if (curr_generator % 2)
					stat[level].rearrangement = 1;
				else
					stat[level].rearrangement = 0;
			}
			else
				stat[level].rearrangement = 2;
			stat[level].rearr_index = -1;
		}
		else {
			curr_generator = stat[level].generator;
			level--;
			set(curr_generator, 0);
			d[curr_generator]--;
			d[curr_generator + 2]--;
			d[curr_generator + 1] = stat[level+1].stack;
			if (!level)
				break;
		}
	}
}

void calc_def (int level) {
	int curr_generator, direct, i;

	max_s = 0;
	max_defects = max_defects_default;

	stat[level].rearr_index = -1;
	for (i = n + 1; i--; ) {
		d[i] = 2;
	}

	while (1) {
		if (stat[level].rearr_index < plurality_def - 1) {
			stat[level].rearr_index++;
			curr_generator = stat[level].generator + (direct = triplets_def[stat[level].rearrangement][stat[level].rearr_index]);
			if (curr_generator > n-2 || curr_generator < 0)
				continue;
			if (a[curr_generator] > a[curr_generator + 1])
				continue;
			stat[level + 1].generator = curr_generator;
			if (!b_free) {
				count_gen(level);
				continue;
			}
			stat[level + 1].defects = stat[level].defects;
			if (!(curr_generator % 2)) { // even, white
				if (d[curr_generator] == 1) {
					stat[level + 1].defects++;
				}
				if (d[curr_generator + 2] == 1) {
					stat[level + 1].defects++;
				}
				if (stat[level + 1].defects > max_defects) {
					continue;
				}

				// for defects, disallow white triangles and allow white squares
				if (d[curr_generator + 1] == 1)
					continue;
			}
			stat[level+1].stack = d[curr_generator + 1];
			d[curr_generator + 1] = 0;
			d[curr_generator]++;
			d[curr_generator + 2]++;
			set(curr_generator, 1);
			level++;
//			stat[level].rearrangement = direct > 0 ? 1 : 2;
			if (direct > 0) {
				if (curr_generator % 2)
					stat[level].rearrangement = 1;
				else
					stat[level].rearrangement = 0;
			}
			else
				stat[level].rearrangement = 2;
			stat[level].rearr_index = -1;
		}
		else {
			curr_generator = stat[level].generator;
			level--;
			set(curr_generator, 0);
			d[curr_generator]--;
			d[curr_generator + 2]--;
			d[curr_generator + 1] = stat[level+1].stack;
			if (!level)
				break;
		}
	}
}
int main(int argc, char **argv) {
	int i, k1;
	char s[80];

	for (i = 1; i < argc; i++) {
		strcpy(s, argv[i]);
		if (0 == strcmp("-n", s))
			sscanf(argv[++i], "%d", &n);
		else if (0 == strcmp("-k", s))
			sscanf(argv[++i], "%d", &k);
		else if (0 == strcmp("-max-def", s))
			sscanf(argv[++i], "%d", &max_defects_default);
		else if (0 == strcmp("-o", s))
			strcpy(filename, argv[++i]);
		else if (0 == strcmp("-full", s))
			full = 1;
		else {
			printf("Invalid parameter: \"%s\".\nUsage: %s -n N [-k K] [-max-def D] [-o filename]\n  -n\n	 line count;\n  -k\n	 parrallel pairs count;\n  -max-def\n	 the number of allowed defects;\n  -o\n	 output file.\n", s, argv[0]);
			return 0;
		}
	}
	//sscanf(argv[1], "%d/%d", &n, &k);

	b_free = (n_step = n*(n-1) / 2) - 1;

	for (i = 0; i < n; i++) {
		a[i] = i;
	}
	
	//{{1, 2, -1}, {2, -1, 1}, {-1, 2, 1}};

	for (i = 3; i < plurality; i++ ) {
		triplets[0][i] = triplets[1][i] = triplets[2][i] = i;
	}

	triplets[0][0] = 1;
	triplets[0][1] = 2;
	triplets[0][2] = -1;
	triplets[1][0] = 2;
	triplets[1][1] = -1;
	triplets[1][2] = 1;
	triplets[2][0] = -1;
	triplets[2][1] = 2;
	triplets[2][2] = 1;

	for (i = 3; i < plurality_def; i++ ) {
		triplets_def[0][i] = triplets_def[1][i] = triplets_def[2][i] = i;
	}

	triplets_def[0][0] = 1;
	triplets_def[0][1] = 2;
	triplets_def[0][2] = -1;
	triplets_def[1][0] = 2;
	triplets_def[1][1] = -1;
	triplets_def[1][2] = 1;
	triplets_def[2][0] = -1;
	triplets_def[2][1] = 2;
	triplets_def[2][2] = 1;

	stat[0].rearrangement = 0;
	stat[0].generator = 0;
	stat[0].defects = 0;

	if (k != -1) {
		printf("---------->>>k=%d\n",k);
		for (k1 = k; k1--; )
			set(2*k1, 1);
		if (max_defects_default == 0)
			calc(0);
		else
			calc_def(0);
		}
	else {
		if (n % 2) {
			for (k=0; k<=n/2; k++) {
				printf("---------->>>k=%d\n",k);
				if (max_defects_default == 0)
					calc(0);
				else
					calc_def(0);
				set(2*k, 1);
			}
		}
		else {
			for (k=0; k<n/2; k++)
				set(2*k, 1);
			printf("---------->>>k=%d\n",k);
			if (max_defects_default == 0)
				calc(0);
			else
				calc_def(0);
			for (; k--; ) {
				printf("---------->>>k=%d\n",k);
				set(2*k, 0);
				if (max_defects_default == 0)
					calc(0);
				else
					calc_def(0);
			}
		}
	}
	printf("---------->>>Process terminated.\n");
	return 0;
}

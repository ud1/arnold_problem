#include "assert.h"
#include "stdio.h"

#define MAX_N 35
#define MAX_DELTA MAX_N
#define MAX_LEV (MAX_N * (MAX_N - 1) / 2)

#define min(a, b) (a < b ? (a) : (b) )

typedef struct {
	int line[MAX_DELTA];
	int round[MAX_DELTA];
	int gen, len, color, last_gen;
} metro_row;

typedef struct {
	metro_row rows[MAX_LEV];
} metro_st;

metro_st conf;
int n, m, delta;

int line(int l, int i) {
	return (conf.rows[l].line[i] +  conf.rows[l].round[i] * delta + n) % n;
}

int line_r(int l, int i, int r) {
	return (conf.rows[l].line[i] + (conf.rows[l].round[i] + r) * delta + n) % n;
}

int line_r1(int l, int i, int r) {
	return line_r(l, i % conf.rows[l].len, r + i / conf.rows[l].len);
}

int b[MAX_N][MAX_N];

void do_stat(int max_level) {
	int left, right, i, j, s;
	static int max_s = -1;
	s = 0;
	// Все генераторы на 1 больше
	for (i = 0; i <= max_level; ++i) {
		s += ( ((conf.rows[i].gen - 1) % 2) ^ conf.rows[i].color) * 2 - 1;
	}
	if (s < 0)
		s = -s;

	if (s <= max_s)
		return;

	max_s = s;
	printf("%d)\n", max_s);
	for (i = 0; i <= max_level; ++i) {
		printf("%d] ", conf.rows[i].gen - 1);
		for (j = 0; j < conf.rows[i].len; ++j) {
			printf("%d ", conf.rows[i].line[j]);
		}
		printf("\n");
	}
	printf("\n");
}

void run() {
	int cur_generator, level, max_level, left, right, i, j, temp, flag, min_i;
	delta = 2*n / m;
	if (2*n % m) {
		printf("ERROR: 2*n % m != 0\n");
		return;
	}

	max_level = n*(n-1)/(2*m)-1;
	level = 1;

	for (i = 2; i < delta; ++i) {
		conf.rows[0].line[i] = i;
		conf.rows[level - 1].round[i] = 0;
	}
	conf.rows[0].line[0] = 1;
	conf.rows[0].line[1] = 0;

	conf.rows[0].gen = 1;
	conf.rows[0].color = 0;
	conf.rows[0].len = delta;

	conf.rows[1].gen = 0;
	conf.rows[1].last_gen = -1;

	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			b[i][j] = 0;
		}
	}
	// Обновляем b-матрицу
	for (i = 0; i < m; ++i) {
		left = line_r(0, 0, i);
		right = line_r1(0, 1, i);
		b[left][right] = 1;
		b[right][left] = 1;
	}

	while (level > 0) {
		if (conf.rows[level].last_gen >= 0) {
			// Удаляем изменения из b-матрицы
			for (i = 0; i < m; ++i) {
				left = line_r(level - 1, conf.rows[level].last_gen, i);
				right = line_r1(level - 1, conf.rows[level].last_gen + 1, i);
				assert(b[left][right]);
				assert(b[right][left]);
				b[left][right] = 0;
				b[right][left] = 0;
			}
			conf.rows[level].last_gen = -1;
		}

		if (level == 1) {
			temp = 0;
		}
		cur_generator = conf.rows[level].gen++;
		if (cur_generator < 0)
			continue;

		if (cur_generator == conf.rows[level - 1].len) {
			--level;
			continue;
		}

		// Проверка b-матрицы
		left = line(level - 1, cur_generator);
		right = line_r1(level - 1, cur_generator + 1, 0);
		if (!b[left][right]) {
			if (cur_generator == conf.rows[level - 1].gen - 1) {
				temp = 0;
			}
			// Копируем строку перед применением генератора
			memcpy(conf.rows[level].line, conf.rows[level - 1].line, sizeof(conf.rows[level - 1].line[0])*conf.rows[level - 1].len);
			memcpy(conf.rows[level].round, conf.rows[level - 1].round, sizeof(conf.rows[level - 1].round[0])*conf.rows[level - 1].len);
			conf.rows[level].len = conf.rows[level - 1].len;
			conf.rows[level].last_gen = cur_generator;

			// Обновляем b-матрицу
			for (i = 0; i < m; ++i) {
				left = line_r(level - 1, cur_generator, i);
				right = line_r1(level - 1, cur_generator + 1, i);
				assert(!b[left][right]);
				assert(!b[right][left]);
				b[left][right] = 1;
				b[right][left] = 1;
			}

			// применяем генератор
			right = (cur_generator + 1) % conf.rows[level].len;
			temp = conf.rows[level].line[cur_generator];
			conf.rows[level].line[cur_generator] = conf.rows[level].line[right];
			conf.rows[level].line[right] = temp;

			temp = conf.rows[level].round[cur_generator];
			conf.rows[level].round[cur_generator] = conf.rows[level].round[right];
			conf.rows[level].round[right] = temp;

			conf.rows[level].color = conf.rows[level - 1].color;

			// Крайние
			if (cur_generator == conf.rows[level].len - 1) {
				--conf.rows[level].round[0];
				++conf.rows[level].round[cur_generator];
			}

			// вычеркивание
			do {
				flag = 0;

				// вычеркиваем промежуточные
				for (i = 0; i < conf.rows[level].len - 1; ++i) {
					left = line(level, i);
					right = line(level, i + 1);
					if (left == right) {
						flag = 1;
						min_i = i;
						for (j = i + 2; j < conf.rows[level].len; ++j) {
							conf.rows[level].line[j - 2] = conf.rows[level].line[j];
							conf.rows[level].round[j - 2] = conf.rows[level].round[j];
						}
						conf.rows[level].len -= 2;
						continue;
					}
				}

				// вычеркиваем крайние
				if (conf.rows[level].len) {
					left = line(level, conf.rows[level].len - 1);
					right = line_r(level, 0, 1);
					if (left == right) {
						flag = 1;
						min_i = 0;
						for (j = 1; j < conf.rows[level].len - 1; ++j) {
							conf.rows[level].line[j - 1] = conf.rows[level].line[j];
							conf.rows[level].round[j - 1] = conf.rows[level].round[j];
						}
						conf.rows[level].len -= 2;
						conf.rows[level].color = !conf.rows[level - 1].color;
						continue;
					}
				}
			} while (flag);

			if (level == max_level) {
				do_stat(max_level);
				continue;
			}

			// Устанавливаем начальные генератор следущего уровня
			if (conf.rows[level].len == conf.rows[level - 1].len) {
				conf.rows[level + 1].gen = cur_generator - 1;
				++level;
			} else {
				// Было вычеркивание
				//conf.rows[level + 1].gen = min(min_i - 1, cur_generator - 1);
				conf.rows[level + 1].gen = 0;
				++level;
			}
			conf.rows[level].last_gen = -1;
		}
	}
}

int main() {
	n = 15;
	m = 5;
	run();
	return 0;
}

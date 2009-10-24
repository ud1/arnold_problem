#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_N 45
#define MAX_DELTA MAX_N
#define MAX_LEV (MAX_N * (MAX_N - 1) / 2)

#define min(a, b) (a < b ? (a) : (b) )

typedef struct {
	int line[MAX_DELTA];
	int round[MAX_DELTA];
	int d[MAX_DELTA];
	int gen, len, last_gen, defects;
} metro_row;

typedef struct {
	metro_row rows[MAX_LEV];
} metro_st;

metro_st conf;
int n, m, k, delta;

int b[MAX_N][MAX_N];
int parallels[MAX_N];
int old_gens[MAX_LEV];

int get_line(int pos) {
	if (pos / n) {
		return parallels[pos % n];
	} else {
		return pos % n;
	}
}

int get_pos(int lv, int i, int r) {
	int rnd = (i + conf.rows[lv].len) / conf.rows[lv].len + r - 1;
	int p = (i + conf.rows[lv].len) % conf.rows[lv].len;
	return (conf.rows[lv].line[p] + (conf.rows[lv].round[p] + rnd) * delta + 2*n) % (2*n);
}

int line(int l, int i, int r) {
	return get_line(get_pos(l, i, r));
}

int line_rh1(int l, int i, int r) {
	int val = get_pos(l, i, r);
	if (val >=0 && val < n)
		return 0;
	return 1;
}

void do_stat(int max_level) {
	int o[MAX_N][MAX_N], p1[MAX_N], p2[MAX_N], a[MAX_N];
	int left, right, i, j, s, gen, h1, h2, diffs;
	static int max_s = -1;
	s = 0;

	// Восстанавливаем Оматрицу
	for (i = 0; i < n; ++i) {
		p1[i] = 0;
		p2[i] = n - 2;
		if (parallels[i] != i) {
			p2[i]--;
		}
	}

	for (i = 0; i < max_level; ++i) {
		// Все генераторы на 1 больше
		gen = conf.rows[i+1].gen - 1;
		for (j = 0; j < m; ++j) {
			left = line(i, gen, j);
			right = line(i, gen + 1, j);
			if (!(h1 = line_rh1(i, gen, j))) {
				o[left][p1[left]++] = right;
			} else {
				o[left][p2[left]--] = right;
			}
			if (!(h2 = line_rh1(i, gen + 1, j))) {
				o[right][p1[right]++] = left;
			} else {
				o[right][p2[right]--] = left;
			}
		}
	}

	// Восстанавливаем генераторы по Оматрице
	for (i = 0; i < n; ++i) {
		p1[i] = 0; // Положение скобок в Оматрице
		p2[i] = n - 2;
		if (parallels[i] != i) {
			p2[i]--;
		}
		a[i] = i;
	}

	s = 0;
	diffs = 0;
	for (i = 0; i < n*(n-1)/2 - k; ++i) {
		for (gen = 0; gen < n - 1; ++gen) {
			left = a[gen];
			right = a[gen + 1];
			if (p1[left] <= p2[left] && p1[right] <= p2[right] && 
				o[left][p1[left]] == right && o[right][p1[right]] == left) 
			{
				++p1[left];
				++p1[right];
				s += (gen % 2)*2 - 1;
				if (old_gens[i] != gen)
					++diffs;
				a[gen] = right;
				a[gen + 1] = left;
				break;
			}
		}
		assert(gen != n - 1);
	}

	if (!diffs) // совпадение генератором с последней найденной конф
		return;

	if (s < 0)
		s = -s;

	if (s < max_s)
		return;

	max_s = s;

	for (i = 0; i < n; ++i) {
		p1[i] = 0; // Положение скобок в Оматрице
		p2[i] = i; // Массив а
	}
	//printf("generators:\n");
	printf("%02d] ", s);
	for (i = 0; i < n*(n-1)/2 - k; ++i) {
		for (gen = 0; gen < n - 1; ++gen) {
			left = p2[gen];
			right = p2[gen + 1];
			if (o[left][p1[left]] == right && o[right][p1[right]] == left) {
				++p1[left];
				++p1[right];
				old_gens[i] = gen;
				printf("%d ", gen);
				p2[gen] = right;
				p2[gen + 1] = left;
				break;
			}
		}
		assert(gen != n - 1);
	}
	printf("\n");
}

void init() {
	int i, j;
	delta = 2*n / m;
	if (2*n % m || k % m) {
		printf("ERROR: invalid m, n, k\n");
		return;
	}

	for (i = 0; i < n; ++i) {
		parallels[i] = i;
	}

	for (i = 0; i < delta; ++i) {
		conf.rows[0].line[i] = i;
		conf.rows[1].line[i] = i;
		conf.rows[0].round[i] = 0;
		conf.rows[1].round[i] = 0;
		conf.rows[1].d[i] = 2;
	}

	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			b[i][j] = 0;
		}
	}

	conf.rows[0].len = delta;
	conf.rows[1].len = delta;
	conf.rows[2].last_gen = -1;
	conf.rows[2].defects = 0;

	for (i = 0; i < n*(n-1)/2-k; ++i) {
		old_gens[i] = 0;
	}
}

void run() {
	int cur_generator, level, max_level, left, right, i, j, temp, flag, min_i, white;
	max_level = (n*(n-1)/2 - k) / m;

	// Выбираем начальный генератор
	for (cur_generator = 0; cur_generator < delta; ++cur_generator) {
		for (i = 0; i < m; ++i) {
			left = line(1, cur_generator, i);
			right = line(1, cur_generator + 1, i);
			if (b[left][right] || b[right][left])
				break;
		}
		if (i == m)
			break;
	}

	assert(cur_generator < delta);

	// устанавливаем четность
	white = (cur_generator + 1) % 2;
	// применяем генератор
	level = 1;
	right = (cur_generator + 1) % conf.rows[level].len;
	temp = conf.rows[level].line[cur_generator];
	conf.rows[level].line[cur_generator] = conf.rows[level].line[right];
	conf.rows[level].line[right] = temp;

	temp = conf.rows[level].round[cur_generator];
	conf.rows[level].round[cur_generator] = conf.rows[level].round[right];
	conf.rows[level].round[right] = temp;

	// Крайние
	if (cur_generator == conf.rows[level].len - 1) {
		--conf.rows[level].round[0];
		++conf.rows[level].round[cur_generator];
	}

	conf.rows[1].gen = cur_generator + 1; // В массиве генераторы на 1 больше
	conf.rows[2].gen = cur_generator - 1; // Отсюда стартанет поиск
	
	// Обновляем b-матрицу
	for (i = 0; i < m; ++i) {
		left = line(1, cur_generator, i);
		right = line(1, cur_generator + 1, i);
		b[left][right] = 1;
		b[right][left] = 1;
	}

	// Обновляем массив d
	conf.rows[level].d[cur_generator] = 1;
	conf.rows[level].d[(cur_generator - 1 + conf.rows[level].len) % conf.rows[level].len]++;
	conf.rows[level].d[(cur_generator + 1) % conf.rows[level].len]++;

	// начинаем поиск
	level = 2;
	while (level > 1) {
		if ((cur_generator = conf.rows[level].last_gen) >= 0) {
			conf.rows[level].last_gen = -1;
			// Удаляем изменения из b-матрицы
			for (i = 0; i < m; ++i) {
				left = line(level - 1, cur_generator, i);
				right = line(level - 1, cur_generator + 1, i);
				assert(b[left][right]);
				assert(b[right][left]);
				b[left][right] = 0;
				b[right][left] = 0;
			}
		}

		cur_generator = conf.rows[level].gen++;
		if (cur_generator < 0)
			continue;

		if (cur_generator == conf.rows[level - 1].len) {
			--level;
			continue;
		}

		// Проверка b-матрицы
		for (i = 0; i < m; ++i) {
			left = line(level - 1, cur_generator, 0);
			right = line(level - 1, cur_generator + 1, 0);
			if (b[left][right])
				break;
		}
		if (i == m) {
			// Оптимизация
			//conf.rows[level].defects = conf.rows[level - 1].defects;
			if (cur_generator % 2 == white) { //Нечетный белый генератор
				if (conf.rows[level-1].d[cur_generator] < 4)
					continue;
			}

			// Копируем строку перед применением генератора
			memcpy(conf.rows[level].line, conf.rows[level - 1].line, sizeof(conf.rows[level - 1].line[0])*conf.rows[level - 1].len);
			memcpy(conf.rows[level].round, conf.rows[level - 1].round, sizeof(conf.rows[level - 1].round[0])*conf.rows[level - 1].len);
			memcpy(conf.rows[level].d, conf.rows[level - 1].d, sizeof(conf.rows[level - 1].d[0])*conf.rows[level - 1].len);
			conf.rows[level].len = conf.rows[level - 1].len;
			conf.rows[level].last_gen = cur_generator;

			conf.rows[level].d[cur_generator] = 1;
			conf.rows[level].d[(cur_generator - 1 + conf.rows[level].len) % conf.rows[level].len]++;
			conf.rows[level].d[(cur_generator + 1) % conf.rows[level].len]++;

			// Обновляем b-матрицу
			for (i = 0; i < m; ++i) {
				left = line(level - 1, cur_generator, i);
				right = line(level - 1, cur_generator + 1, i);
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
					left = line(level, i, 0);
					right = line(level, i + 1, 0);
					if (left == right) {
						flag = 1;
						min_i = i;
						conf.rows[level].d[(i-1 + conf.rows[level].len) % conf.rows[level].len] += conf.rows[level].d[(i+1) % conf.rows[level].len];
						for (j = i + 2; j < conf.rows[level].len; ++j) {
							conf.rows[level].line[j - 2] = conf.rows[level].line[j];
							conf.rows[level].round[j - 2] = conf.rows[level].round[j];
							conf.rows[level].d[j - 2] = conf.rows[level].d[j];
						}

						conf.rows[level].len -= 2;
						continue;
					}
				}

				// вычеркиваем крайние
				if (conf.rows[level].len) {
					left = line(level, conf.rows[level].len - 1, 0);
					right = line(level, 0, 1);
					if (left == right) {
						flag = 1;
						min_i = 0;
						conf.rows[level].d[0] += conf.rows[level].d[conf.rows[level].len-2];
						conf.rows[level].line[0] = conf.rows[level].line[conf.rows[level].len - 2];
						conf.rows[level].round[0] = conf.rows[level].round[conf.rows[level].len - 2] - 1;
						conf.rows[level].len -= 2;
						continue;
					}
				}
			} while (flag);

			if (level == max_level) {
				do_stat(max_level);
				continue;
			}

			if ((cur_generator % 2 == white) && conf.rows[level].len == conf.rows[level - 1].len) { //Нечетный белый генератор
				if (conf.rows[level].d[(cur_generator - 1 + conf.rows[level - 1].len) % conf.rows[level - 1].len] >= 3) {
					continue;
				}
				if (conf.rows[level].d[(cur_generator + 1) % conf.rows[level - 1].len] >= 3) {
					continue;
				}
			}

			// Устанавливаем начальный генератор следущего уровня
			if (conf.rows[level].len == conf.rows[level - 1].len) {
				conf.rows[level + 1].gen = cur_generator - 1;
				++level;
			} else {
				// Было вычеркивание
				conf.rows[level + 1].gen = min(min_i - 1, cur_generator - 1);
				//conf.rows[level + 1].gen = 0;
				++level;
			}
			conf.rows[level].last_gen = -1;
		}
	}
}

void set_parallels(int j) {
	int i, left, right;
	for (i = 0; i < m; ++i) {
		left = line(1, j, i);
		right = line(1, j + 1, i);
		assert(!b[left][right]);
		assert(!b[right][left]);
		parallels[left] = right;
		parallels[right] = left;
		b[left][right] = 1;
		b[right][left] = 1;
	}
}

int main() {
	int i;
	n = 24;
	m = 3;
	k = 6;

	init();
	set_parallels(0);
	set_parallels(4);

	run();
	return 0;
}

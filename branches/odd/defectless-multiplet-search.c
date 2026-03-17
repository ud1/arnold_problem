/**
 * Программа осуществляет подбор мультиплета, описывающего обход дерева, и выполняет ограниченный обход дерева.
 * Отбираются те мультиплеты, для которых удалось спуститься на самый глубокий уровень.
 * 
 * Применяется оптимизация для нечетного количества прямых, зафиксирована раскраска, начальные генераторы.
 * Черные области могут быть только треугольниками, так как черные генераторы применяются сразу после белых
 * и тем самым не перебираются.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <limits.h>

// May vary
#define n1 49
#define n_step1 (n1*(n1-1) / 2)

#define plurality (n1-1)/2 // Количество узлов дерева на каждом уровне
#define rearrangement_count 1 // Количество вариантов перестановок для обхода этих узлов

// #define DEBUG 1

unsigned int n;
unsigned int level_limit = 0;

typedef uint_fast32_t line_num;

int level = 0; ///?

/**
 * Функция перемешивает массив случайным образом
 */
void shuffle(int arr[], int size) {
    for (int i = size - 1; i > 0; i--) {
        // Генерируем случайный индекс от 0 до i
        int j = rand() % (i + 1);

        // Меняем местами arr[i] и arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void print_multiplet(int triplets[][plurality]) {
	printf("[");
    // Выводим перемешанный массив
    for (int i = 0; i <= (n-3)/2; i++) {
        printf("%d, ", triplets[0][i]);
    }
	printf("]");
}

void move_in_multiplet(int multiplet[][plurality], int from, int to) {
    int a = multiplet[0][from];

    if (to > from) {
        for (int i = from; i < to; i++) {
            multiplet[0][i] = multiplet[0][i + 1];
        }
    }

    if (to < from) {
        for (int i = from; i > to; i--) {
            multiplet[0][i] = multiplet[0][i-1];
        }
    }

    multiplet[0][to] = a;
}
void init_multiplets(int triplets[][plurality]) {
	int i;
	// -2, n-3, n-5 ...
	// for (i = 1; i <= (n-3)/2; i++ ) {
	// 	triplets[0][i] = n-1 - 2*i;
	// }
    // move_in_multiplet(triplets, 1, 2);
    // move_in_multiplet(triplets, 2, 3);

	// -2, +2, n-3, n-5 ...
	// for (i = 1; i <= (n-3)/2; i++ ) {
	// 	triplets[0][i] = n+1 - 2*i;
	// }
	// triplets[0][1] = 2;

	// -2, 2, 4, ...
    // не очень работает для 33 ??
	for (i = 1; i <= (n-3)/2; i++ ) {
	 	triplets[0][i] = 2*i;
	}
    // псевдообертка 8
    /// move_in_multiplet(triplets, 1, 2);
    move_in_multiplet(triplets, (n-3)/2-3, 1);
    move_in_multiplet(triplets, 4, 2);

    // move_in_multiplet(triplets, 1, 2);
    // move_in_multiplet(triplets, (n-3)/2-1, 1);
    // move_in_multiplet(triplets, (n-3)/2-1, 3);

	triplets[0][0] = -2;

	// shuffle(triplets[0], (n-1)/2);

	// int ttt[] = {6, 16, 20, 22, 2, 4, -2, 14, 8, 10, 24, 12, 18, 26};
	// int ttt[] = {-2, 20, 6, 12, 16, 14, 10, 22, 4, 8, 2, 24, 26, 18};
	// int ttt[] = {-2, 20, 2, 12,  4, 6, 8, 10,  14, 16, 18,   22, 24, 26}; // 29
	// int ttt[] = {-2, 24, 2, 20,  4, 6, 8, 10, 12, 14, 16, 18,   22,   26, 28, 30}; // 33

//	int ttt[] = {-2, 26, 2, 18, 4, 6, 8, 10, 12, 14, 16, 20, 22, 24, 28, 30, 32}; // 35
    // int ttt[] = {-2, 30, 2, 28, 4, 24, 20, 8, 14, 10, 12, 16, 18, 26, 22, 6 }; // 35

	// int ttt[] = {-2, 30, 2, 22, 4, 6, 8, 10, 12, 14, 16, 18, 20, 24, 26, 28, 32, 34, 36}; // 39 ??

    // int ttt[] = {-2, 40, 38, 20, 32, 28, 4, 2, 10, 14, 24, 22, 18, 8, 6, 12, 16, 26, 36, 34, 42, 30}; // 45
	// for (i = 0; i <= (n-3)/2; i++ ) {
	// 	triplets[0][i] = ttt[i];
	// }

	printf("multiplet = ");
    print_multiplet(triplets);
	printf("\n");

	for (i = 0; i <= (n-3)/2; i++ ) {
		int shift = 2*i;
		if (shift == 0) {
			shift = -2;
		}
		int hasShift = 0;
		for (int j = 0; j <= (n-3)/2; j++) {
			if (shift == triplets[0][j]) {
				hasShift = 1;
				break;
			}
		}
		if (!hasShift) {
			printf("Shift=%d not found in multiplets.\n", shift);
			exit(0);
		}
	}

}


typedef struct {
    line_num generator;
    int rearr_index;
} Stat;

Stat stat[n_step1 + 1];

line_num a[n1]; // Текущая перестановка 

int max_level = 0;
unsigned long int min_iterations = ULONG_MAX;

// User input
char filename[80] = "";
int full = 0;

struct timeval start;

double last_signal = -10.0;

void handle_signal(int sig) {
    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

    printf("\n [%f] level = %d/%d", time_taken, level, max_level);
    printf("\n%d\n", (int)stat[n/2].generator);

    exit(0);


    // for (int i = 0; i < level; i++) {
    //     if (stat[i].generator % 2) {
    //         continue;
    //     }
    //     printf("%.*s", stat[i].generator, "                            ");
    //     printf(" %d \n", stat[i].generator);
    // }

    // printf("\n");

    // if (time_taken - last_signal < 1) {
    //     exit(0);
    // }

    // last_signal = time_taken;
}

void count_gen(int level, unsigned long int iterations) {
    if (full) {
        if (level > max_level) {
            return;
        }
    } else if (min_iterations <= iterations) {
        // printf("count_gen skip %lu %lu\n", min_iterations, iterations);
        return;
    }

    struct timeval end;
    int i;
    FILE *f;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6; // in seconds

    max_level = level;
    min_iterations = iterations;

    /**
     * В конфигурации имеется level четных генераторов. Каждый четный генератор порождает
     * белую область. С ним также ассоциированы два нечетных генератора, которые порождают
     * две черные области. Но с генератором 0 ассоциирован только один черный генератор.
     *
     * Кроме того, есть (n-1)/2 начальных черных генераторов, которые порождают
     * неучтенные черные области.
     *
     * Внешние области чередующегося цвета в количестве n + 1, которые пересекаются
     * в начальном положении сканирующей прямой и которые тоже не учтены,
     * вклада в разность не дают.
     */

    int s = level_limit - 1 + (n - 1) / 2;

    printf(" [%fs] A=%d", time_taken, s);
    printf(" i=%e)", (double)iterations);

    for (i = n / 2; i--; ) {
        printf(" %d", i * 2 + 1);
    }

    for (i = level_limit; i--;) {
        // Белый генератор
        printf(" %d", (int)stat[i].generator);
        // Ассоциированные черные генераторы
        if (stat[i].generator == 0) {
            printf(" 1");
        }
        else if (stat[i].generator % 2 == 0) {
            printf(" %d %d", (int)stat[i].generator - 1, (int)stat[i].generator + 1);
        }
    }

    printf("\n");

    // for (int i = 0; i < level; i++) {
    //     // if (stat[i].generator % 2) {
    //     //     continue;
    //     // }
    //     printf("%.*s", stat[i].generator, "||||||||||||||||||||||||||||||||||||||||||||||||||||");
    //     printf("><");
    //     printf("%.*s\n", n - 2 - stat[i].generator, "||||||||||||||||||||||||||||||||||||||||||||||||||||");
    // }

    // printf("\n");


    fflush(NULL);
    fflush(NULL);
    fflush(stdout);
    fflush(stdout);
    return;
}

/**
 * Пересекает прямые по генератору
 *
 * @param   int   generator       номер генератора
 * @param   int   cross_direction 1 если косичка переплетается, 0 если распутывается
 *
 * @return  void
 */
void set(unsigned int generator, unsigned int cross_direction) {
    line_num i;

    i = a[generator];
    a[generator] = a[generator + 1];
    a[generator + 1] = i;
}

/**
 * Применение белого генератора и двух ассоциированных (соседних) черных.
 */
void do_cross_with_assoc(unsigned int generator) {
    line_num i;

    i = a[generator];
    if (generator > 0) {
        a[generator] = a[generator - 1];
        a[generator - 1] = a[generator + 1];
    }
    else {
        a[generator] = a[generator + 1];
    }
    a[generator + 1] = a[generator + 2];
    a[generator + 2] = i;
}

/**
 * Отмена применения белого генератора и двух ассоциированных (соседних) черных.
 */
void do_uncross_with_assoc(unsigned int generator) {
    line_num i;

    i = a[generator + 2];
    a[generator + 2] = a[generator + 1];

    if (generator > 0) {
        a[generator + 1] = a[generator - 1];
        a[generator - 1] = a[generator];
    }
    else {
        a[generator + 1] = a[generator];
    }
    a[generator] = i;
}

/**
 * Метод обеспечивает перебор генераторов.
 *
 * @param   int 
 * @param   int 
 *
 * @return  
 */
double calc_deep(unsigned long int iterations_limit, int triplets[][plurality]) {
    int curr_generator;
    int level = level_limit - 1;
    unsigned long int iterations = 0;
    double deepest_level = level;

    max_level = level;

    for (int i = 0; i < n; i++) {
        a[i] = i;
    }

    // Оптимизация 0. Первые генераторы должны образовать (n-1)/2 внешних черных двуугольников.
    for (int i = n / 2; i--; ) {
        set(i * 2 + 1, 1);
    }

    stat[level].rearr_index = -1;
    stat[level + 1].generator = 0;
    
//    int i = iterations_limit;
    while (iterations < iterations_limit) {
        iterations++; // Раскомментировать при добавлении новых условий оптимизации для "профилировки".
#ifdef DEBUG 
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %lu)  lev=%d ri=%d maxi=%d, cond=%d\n", iterations, level, stat[level].rearr_index, (n - 3)/2, stat[level].rearr_index < (int)(n - 3)/2);
        // printf("-->> start lev = %d gen = %d\n", level, stat[level].generator);
#endif

		if (stat[level].rearr_index < (int) (n - 3)/2) {
			stat[level].rearr_index++;
			curr_generator = stat[level + 1].generator + triplets[0/*stat[i].rearrangement*/][stat[level].rearr_index];
#ifdef DEBUG 
			printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> next, curr_generator = %d, prev_gen=%d, delta=%d\n", curr_generator, (int) stat[level + 1].generator , triplets[0][stat[level].rearr_index]);
#endif

			if (curr_generator > n-3 || curr_generator < 0) {
				continue;
			}

#ifdef DEBUG
            for (int j = 0; j < n; j++) {
                printf("%d ", a[j]);
            }
            printf("\n");
#endif

            /**
             * Оптимизации.
             * 
             * 1. Генератор должен пересекать только прямые, которые еще не пересекались.
             * Прямые пересекались, если левая прямая имеет больший номер, чем правая.
             * 
             * 2. Последние генераторы должны образовать (n-1)/2 внешних черных двуугольников.
             * Стороны двуугольников - прямые с убывающими номерами.
             * Эта оптимизация - продолжение фиксации начальных генераторов. Применима только для черных генераторов.
             * Например, прямые 0 и 1 могут пересекаться только последним генератором n - 2 в самом конце.
             * В середине их нельзя пересекать. Тут проверяем хотя бы совпадение генератора и прямых.
             * Можно проверить, что это именно последний генератор, но на практике это не дает заметного ускорения.
             * 
             * 3. Черные генераторы исключаются из перебора.
             * Сразу после применения любого белого генератора применяются два соседних черных генератора
             * (для белого генератора 0 только один соседний правый генератор 1).
             * 
             * 4. Генератор 0 применяется только один раз для первой и последней прямой.
             * Он образует оставшийся внешний черный двуугольник, образованный прямыми 0 и n-1.
*
             * 5. Если прямая n-1 начала идти справа налево, остальные прямые пересекать смысла нет.
             * Соответствующие генераторы должны уменьшаться подряд и заканчиваться генератором 0.
             * Остальные генераторы применять в этом интервале не нужно. Для отслеживания начала движения
             * последней прямой используется a[n-2], а не a[n-1] в силу оптимизации 0.
             */

            if (curr_generator != 0) {
                // Оптимизация 1 и 3.
                // На самом деле если ее убрать, в перебор пойдут недопустимые наборы генераторов
                if (a[curr_generator - 1] > a[curr_generator + 1]) {
                    continue;
                }

                // Оптимизация 1 и 3.
                // На самом деле если ее убрать, в перебор пойдут недопустимые наборы генераторов
                if (a[curr_generator] > a[curr_generator + 2]) {
                    continue;
                }

                // Оптимизация 2.
                if (a[curr_generator] + 1 == a[curr_generator + 2] && curr_generator + 1 + a[curr_generator + 2] != n - 1) {
                    continue;
                }

                // Оптимизация 1.
                // Как показывает профилировка, после остальных оптимизаций эта не дает ускорения
                // if (a[curr_generator] > a[curr_generator + 1]) {
                    //     continue;
                // }

                // Оптимизация 2.
                if (a[curr_generator - 1] + 1 == a[curr_generator + 1] && curr_generator - 1 + a[curr_generator + 1] != n - 1) {
                    continue;
                }

                // Оптимизация 2. Закомментировано, потому что профилировка показывает, что количество итераций слегка сокращается, а время выполнения увеличивается.
                // if (a[curr_generator - 1] - 1 == a[curr_generator] && a[curr_generator] + curr_generator == n-1) {
                //     continue;
                // }
                // if (a[curr_generator + 1] - 1 == a[curr_generator + 2] && a[curr_generator + 1] - 1 + curr_generator == n-1) {
                //     continue;
                // }
            }
            else {
                // Оптимизация 4.
                if (a[curr_generator + 1] != n - 1) {
                    continue;
                }
                if (a[curr_generator] != 0) {
                    continue;
                }
                // Здесь нет смысла проверять оптимизацию 2, так как прямая с номером 1 уже пересекла прямую n-1 и прямую 2.
                // Поэтому a[2] != 1, и пересечение a[1] == 0 и a[2] всегда возможно.
            }

            // // Оптимизация 5. Закомментирована, так как не дает прироста в эвристике -2, n-3, n-5...
            // if (a[0] != n-1 && a[n-2] != n-1 && a[curr_generator + 1] != n-1) {
            //     continue;
            // }
            
            // Ограничение на поиск: максимальный генератор можно применить только два раза.
            // Слишком сильное, долгий перебор, но может пригодится в каких-нибудь эвристиках.
            // if (curr_generator == n - 3 && a[curr_generator + 1] != n-1 && a[curr_generator] != 0) {
            //     continue;
            // }


			stat[level].generator = curr_generator;

#ifdef DEBUG 
            printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> down\n");
#endif				
            // Применяем белый генератор curr_generator и ассоциированные соседние черные генераторы
            do_cross_with_assoc(curr_generator);

            level--;

            if (level == -1) {
#ifdef DEBUG 
                printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> count_gen!!! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
#endif				
                count_gen(level, iterations);

                goto up; // эквивалентно строке ниже, при изменении кода другой вариант может быть быстрее
            }
            else {
                // stat[level].rearrangement = 0;
                stat[level].rearr_index = -1;
            }
        }
        else {
        up:
#ifdef DEBUG 
            printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> up\n");
#endif				
            if (level == 0) {
                deepest_level = -1.0e8/iterations;
                // printf("up: %f %lu\n", deepest_level, iterations);
                return deepest_level;
            } else if (deepest_level > level + 1e-12 * iterations ) {
                deepest_level = level + 1e-12 * iterations;
            } /* else if (deepest_level > level - 1) {
                deepest_level -= 0.0001;
            }*/

            // Для корректной работы нужно это условие. Без него программа зациклится или вызовет seg fault.
            // Но его пропуск ускоряет программу. При этом она всё равно распечатает конфигурации.
            // Раскомментировать, например, при полном поиске.
            if (level == level_limit - 1) {
                // printf("search finished\n");
                break;
            }

            level++;

			curr_generator = stat[level].generator;
            // Отменяем генераторы в обратном порядке, так как они не коммутируют
			do_uncross_with_assoc(curr_generator);
        }
        // i--;
    }

    return deepest_level;
}

int main(int argc, char** argv) {
    int i;
    char s[80];

    if (argc <= 1) {
        printf("Usage: %s -n N [-o filename] [-full]\n  -n\n	 line count;\n  -o\n	 output file.\n", argv[0]);

        return 0;
    }

    signal(SIGINT, handle_signal);

    for (i = 1; i < argc; i++) {
        strcpy(s, argv[i]);
        if (0 == strcmp("-n", s))
            sscanf(argv[++i], "%d", &n);
        else if (0 == strcmp("-o", s))
            strcpy(filename, argv[++i]);
        else if (0 == strcmp("-full", s))
            full = 1;
        else {
            printf("Invalid parameter: \"%s\".\nUsage: %s -n N [-o filename] [-full]\n  -n\n	 line count;\n  -o\n	 output file.\n", s, argv[0]);

            return 0;
        }
    }

    if (n % 2 == 0) {
        printf("This program is designed for fast search of odd defectless configurations only.\n");

        return 0;
    }

    if (n > n1) {
        printf("This program is compiled to support up to %d lines, %d given.\n", n1, n);

        return 0;
    }

    level_limit = (n * (n - 2) + 3) / 6;

	int triplets[rearrangement_count][plurality];
	int cur_multiplet[rearrangement_count][plurality];

    gettimeofday(&start, NULL);

    // Инициализация генератора случайных чисел
    srand(time(NULL));


    init_multiplets(triplets);
    for (int k1 = 0; k1 < (n-1)/2; k1++) {
        cur_multiplet[0][k1] = triplets[0][k1];
    }

    unsigned long int budget = 1e9;
    double prev_level = calc_deep(budget, triplets);
    printf("start on l = %f\n", prev_level);

    for (unsigned long int h=1 ; h < 50; h++) {
        printf("\ntry = %lu\n", h);

        for (int k1 = 0; k1 < (n-3)/2; k1++) {
            if (k1 == 0 && cur_multiplet[0][0] == -2) {
                // на первом месте почти всегда должно быть -2
                continue;
            }
            int found = 0;
            int found_k1, found_k2;
            int max_level = 0;
            for (int k2 = k1 + 1; k2 < (n-1)/2; k2++) {
                move_in_multiplet(cur_multiplet, k2, k1);
//                print_multiplet(cur_multiplet);

                double cur_level = calc_deep(min_iterations < ULONG_MAX ? min_iterations :  h*budget, cur_multiplet);
                printf("pair %d %d   stopped on l = %f                                   \r", k1, k2, cur_level);
                fflush(stdout);
                if (cur_level > max_level) {
                    max_level = cur_level;
                }
                if (cur_level < prev_level) {
                    prev_level = cur_level;
                    found = 1;
                    found_k1 = k1;
                    found_k2 = k2;
                    printf("save %d %d   prev_level= %f\n", found_k1, found_k2, prev_level);
                }

                move_in_multiplet(cur_multiplet, k1, k2);
            }
            if (found) {
                move_in_multiplet(cur_multiplet, found_k2, found_k1);
                printf("work on multiplet = ");
                print_multiplet(cur_multiplet);
                printf("\n");
            } else if (prev_level == max_level) {
                printf("not found \n");
                break;
            }
        }

    }

    printf("---------->>> Process terminated.\n");

    return 0;
}

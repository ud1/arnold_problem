/**
 * Программа осуществляет обход дерева для поиска бездефектных конфигураций.
 *
 * Применяется оптимизация для нечетного количества прямых, зафиксирована раскраска, начальные генераторы.
 * Черные области могут быть только треугольниками, так как черные генераторы применяются сразу после белых
 * и тем самым не перебираются.
 *
 * Вычисляется какая-то статистика по многоугольникам для поиска эвристики
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <signal.h>

// May vary
#define n1 53
#define n_step1 (n1*(n1-1) / 2)

// #define DEBUG 1

unsigned int n;
unsigned int level_limit = 0;

typedef uint_fast32_t line_num;

typedef struct {
    line_num generator;
    line_num prev_polygon_cnt;
    unsigned int sequence_num; // Порядковый номер убывающей на 2 последовательности генераторов. Инкрементируется, когда текущий генератор больше предыдущего.
    unsigned long int iterations;
} Stat;

Stat stat[n_step1 + 1];

line_num a[n1]; // Текущая перестановка 
line_num d[n1 + 1]; // Количество боковых вершин в текущих многоугольниках вокруг прямых

unsigned int polygon_stat[100];

unsigned int b_free; // Количество оставшихся генераторов для подбора
int max_level = 0;

// User input
char filename[80] = "";
int full = 0;

struct timeval start;

void dump_generators() {
    int a2[n1];

    void set2(unsigned int generator) {
        line_num i;

        i = a2[generator];
        a2[generator] = a2[generator + 1];
        a2[generator + 1] = i;
    }

    for (int i = 0; i < n; i++) {
        a2[i] = i;
    }
    
    for (int i = n / 2; i--; ) {
        set2(i * 2 + 1);
    }

    for (int i = level_limit; i--;) {
        if (i < level_limit - 2 && (stat[i].iterations == 0 || n-3 < stat[i].generator)) {
            break;
        }


        if (stat[i-1].sequence_num != stat[i].sequence_num) {
            printf("\n [sn=%02d, i=%.7e] ", stat[i].sequence_num, (double)stat[i-1].iterations);

            for (int j = 0; j < n; j++) {
                printf(" %2d", a2[j]);
            }

            int j = 1 + (n-3 - stat[i].generator)/2;
            if (j > 200) {
                j = 200;
            }
            if (j < 1) {
                j = 1;
            }
            for (; j-- ;) {
                printf("   ");
            }
        }
        printf(" %2d", (int)stat[i].generator);

        set2(stat[i].generator);
        if (stat[i].generator > 0) {
            set2(stat[i].generator - 1);
        }
        set2(stat[i].generator + 1);
    }
}

void handle_signal(int sig) {
    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

    printf("\n [%fs] dumping:\n", time_taken);

    dump_generators();

    printf("\n [%fs] bye\n", time_taken);

    exit(0);
}

void count_gen(unsigned long int iterations) {

    if (level_limit < max_level || !full && level_limit == max_level) {
        return;
    }

    struct timeval end;
    int i;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6; // in seconds

    max_level = level_limit;

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
    printf(" i=%lu)", iterations);

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

    dump_generators();

    printf("\n");

    for (i = 1; i < n+1; i+=2) {
        // Нижние черные генераторы + большая окружность
        polygon_stat[i == 1 ? d[i]: (i == n ? d[i] - 2 : d[i] + 1)]++;
//        printf("poly[%d] = %d\n", i, i == 1 ? d[i]: (i == n ? d[i] - 2 : d[i] + 1));
    }

    for (i = 11; i--;) {
        printf("polygon[%d] = %d\n", i + 4, polygon_stat[i]);
    }

    for (i = 1; i < n+1; i+=2) {
        // откатываем назад
        polygon_stat[i == 1 ? d[i]: (i == n ? d[i] - 2 : d[i] + 1)]--;
    }

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

    // b_free += -2 * cross_direction + 1; // Корректируем кол-во оставшихся генераторов

    i = a[generator];
    a[generator] = a[generator + 1];
    a[generator + 1] = i;
}

/**
 * Применение белого генератора и двух ассоциированных (соседних) черных.
 */
void do_cross_with_assoc(int level, unsigned int generator) {
    line_num i;

    stat[level - 1].prev_polygon_cnt = d[generator + 1];
    polygon_stat[d[generator + 1]]++;

    d[generator + 1] = 0;
    d[generator + 3]++;

    i = a[generator];
    if (generator > 0) {
        d[generator - 1]++;

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
void do_uncross_with_assoc(int level, unsigned int generator) {
    line_num i;

    i = a[generator + 2];
    a[generator + 2] = a[generator + 1];

    if (generator > 0) {
        d[generator - 1]--;

        a[generator + 1] = a[generator - 1];
        a[generator - 1] = a[generator];
    }
    else {
        a[generator + 1] = a[generator];
    }
    a[generator] = i;

    d[generator + 3]--;
    d[generator + 1] = stat[level - 1].prev_polygon_cnt;
    polygon_stat[d[generator + 1]]--;

}

// // Стратегия перебора -2, +2, +4 ...

// unsigned int inline should_process(line_num *cur_gen, line_num prev_gen) {

//     if (*cur_gen == INT_FAST8_MAX) {
//         // С помощью условия пропускаем значение -2.
//         *cur_gen = (prev_gen > 0) ? prev_gen - 2 : prev_gen + 2;

//         return 1;
//     }

//     *cur_gen += 2;
//     if (*cur_gen == prev_gen) {
//         *cur_gen += 2;
//     }

//     if (*cur_gen <= n - 3) {
//         return 1;
//     }

//     return 0;
// }

// line_num inline init_working_gen() {
//     return INT_FAST8_MAX;
// }

// line_num inline init_skip_gen(line_num curr_generator) {
//     return n - 3; // todo
// }

// Стратегия перебора -2, n-3, n-5 ...
unsigned int inline should_process(line_num* cur_gen, line_num prev_gen, unsigned int sequence_num) {
    // Пример: если предыдущий генератор 2, заканчивать надо на 4

    int max_gen () {
//         if (sequence_num < 12 && sequence_num % 2 == 1) {
// //            printf("sn=%d, %d\n", sequence_num, n-3 - (sequence_num + 1) / 2);
//             return n-3 - (sequence_num + 1);
//         }

//         if (sequence_num == 6) {
//             return n - 5;
//         }

        // if (sequence_num == 10) {
        //     return 30;
        // }

        // if (sequence_num == 11) {
        //     return 24;
        // }

        // if (sequence_num == 12) {
        //     return 26;
        // }

        // if (sequence_num == 13) {
        //     return 28;
        // }
    
        // if (sequence_num == 14) {
        //     return 30;
        // }

        // if (sequence_num == 12) {
        //     return 34;
        // }

        // if (sequence_num == 13) {
        //     return 30;
        // }

        // if (sequence_num == 14) {
        //     return 32;
        // }

        // if (sequence_num == 15) {
        //     return 34;
        // }

        // if (sequence_num == 16) {
        //     return 36;
        // }

        // if (sequence_num == 17) {
        //     return 38;
        // }

        // if (sequence_num == 18) {
        //     return 40;
        // }

        // if (sequence_num == 19) {
        //     return 42;
        // }

        // if (sequence_num == 20) {
        //     return 44;
        // }


        return n - 3;
    }

    if (*cur_gen == INT_FAST8_MAX) {
        // С помощью условия пропускаем значение -2.
        if (prev_gen > 0) {

            // if (sequence_num == 12 && prev_gen == 12) {
            //     return 0;
            // }

            *cur_gen = prev_gen - 2;
        } else {
            *cur_gen = max_gen();
        }

        return 1;
    }

    if (*cur_gen + 2 == prev_gen) {
        if (n - 3 <= prev_gen) {
            // Предотвращаем зацикливание. Когда prev_gen на максимуме и равен n-3, переключать на n-3 нельзя
            return 0;
        }
        *cur_gen = max_gen();
        return 1;
    }

    if (*cur_gen == prev_gen + 2) {
        return 0;
    }

    *cur_gen -= 2;
    return 1;
}

line_num inline init_working_gen() {
    return INT_FAST8_MAX;
}

line_num inline init_skip_gen(line_num curr_generator) {
    return curr_generator + 2;
}

// //Стратегия перебора от большего к меньшему
// int inline should_process(int cur_gen, int prev_gen) {
//     return cur_gen > prev_gen - 2 && cur_gen > 0 ;
// }

// void inline modify_generator(int* cur_gen, int prev_gen) {
//     *cur_gen -= 2;
//     if (*cur_gen == prev_gen) {
//         *cur_gen -= 2;
//     }
// }

// int inline init_working_gen(int generator) {
//     return n - 1;
// }

// int inline init_skip_gen(int curr_generator) {
//     return 0;
// }

// Calculations for defectless configurations
void calc() {
    int level = level_limit - 1;
    unsigned long int iterations = 1;
    max_level = 0;

    for (int i = n + 1; i--; ) {
        d[i] = i == 1 ? 0 : 1;
    }
    
    // Оптимизация 0. Первые генераторы должны образовать (n-1)/2 внешних черных двуугольников.
    for (int i = n / 2; i--; ) {
        set(i * 2 + 1, 1);
    }


    stat[level + 1].generator = 0; // Начальное значение для эвристики. TODO может меняться вместе с ней.
    stat[level + 1].sequence_num = 0;
    stat[level + 1].iterations = 0;

    stat[level].generator = init_working_gen(); // завышенное несуществующее значение, будет уменьшаться

    while (1) {
        iterations++; // Раскомментировать при добавлении новых условий оптимизации для "профилировки".
#ifdef DEBUG 
        printf("-->> start lev = %d gen = %d\n", level, stat[level].generator);
#endif

        if (should_process(&stat[level].generator, stat[level + 1].generator, stat[level].sequence_num)) {

            unsigned int curr_generator = stat[level].generator;

#ifdef DEBUG 
            printf("test lev = %d gen = %d prev = %d b_free = %d\n", level, curr_generator, stat[level - 1].generator, b_free);
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

            // if (polygon_stat[12-4] > 0) {
            //     continue;
            // }
            // if (polygon_stat[11-4] > 0) {
            //     continue;
            // }
            // if (polygon_stat[10-4] > 0) {
            //     continue;
            // }
            // if (polygon_stat[9-4] > 1) {
            //     continue;
            // }
            // if (polygon_stat[8-4] > 2) {
            //     continue;
            // }
            // if (polygon_stat[7-4] > 17) {
            //     continue;
            // }


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

            // Оптимизация 5. Закомментирована, так как не дает прироста в эвристике -2, n-3, n-5...
            // if (a[0] != n-1 && a[n-2] != n-1 && a[curr_generator + 1] != n-1) {
            //     continue;
            // }

            // if (d[curr_generator + 1] + 4 > 8) {
            //     continue;
            // }

            // Ограничение на поиск: максимальный генератор можно применить только два раза.
            // Слишком сильное, долгий перебор, но может пригодится в каких-нибудь эвристиках.
            // if (curr_generator == n - 3 && a[curr_generator + 1] != n-1 && a[curr_generator] != 0) {
            //     continue;
            // }

            // Применяем белый генератор curr_generator и ассоциированные соседние черные генераторы
            do_cross_with_assoc(level, curr_generator);

            level--;  // Уменьшается при продвижении вглубь

            if (level == -1) {
                count_gen(iterations);

#ifdef DEBUG 
                printf("count-gen\n");
#endif
                goto up; // эквивалентно строке ниже, при изменении кода другой вариант может быть быстрее
                // stat[level].generator = init_skip_gen(curr_generator); // перебора не будет, чтобы вернуться
            }
            else {
                stat[level].generator = init_working_gen(); // запускаем перебор заново на другом уровне
                stat[level].iterations = iterations;
                // Запоминаем номер подпоследовательности убывающих на 1 генераторов
                stat[level].sequence_num = stat[level + 1].sequence_num + (curr_generator > stat[level + 2].generator ? 1 : 0);
            }
        }
        else {
        up:
            // Для корректной работы нужно это условие. Без него программа зациклится или вызовет seg fault.
            // Но его пропуск ускоряет программу. При этом она всё равно распечатает конфигурации.
            // Раскомментировать, например, при полном поиске.
            // if (level == level_limit - 1) {
            //     printf("search finished\n");
            //     break;
            // }

            stat[level].generator = 0;
            stat[level].iterations = 0;
            // Запоминаем номер подпоследовательности убывающих на 1 генераторов
            stat[level].sequence_num = 0;

            level++;

            // Отменяем генераторы в обратном порядке, так как они не коммутируют
            do_uncross_with_assoc(level, stat[level].generator);
        }
    }
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

    b_free = n * (n - 1) / 2;
    level_limit = (n * (n - 2) + 3) / 6;

    // Подготовка начальной перестановки
    for (i = 0; i < n; i++) {
        a[i] = i;
    }

    gettimeofday(&start, NULL);
    calc();

    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

    printf("---------->>> [%fs] Process terminated.\n", time_taken);

    return 0;
}

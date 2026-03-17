/**
 * Программа осуществляет обход дерева для поиска бездефектных конфигураций.
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
#include <signal.h>
#include <malloc.h>
#include "hash_table.h"

// May vary
#define n1 47
#define n_step1 (n1*(n1-1) / 2)

// #define DEBUG 1

unsigned int n;
unsigned int level_limit = 0;

// typedef uint_fast32_t line_num;

typedef struct {
    line_num generator;
} Stat;

Stat stat[n_step1 + 1];

line_num a[n1]; // Текущая перестановка

HashTable *table;

unsigned int b_free; // Количество оставшихся генераторов для подбора
int max_level = 0;

// User input
char filename[80] = "";
int full = 0;

struct timeval start;

void handle_signal(int sig) {
    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

    printf("\n [%fs] bye\n", time_taken);

    hash_table_print_stats(table);

    exit(0);
}

void count_gen(unsigned long int iterations, unsigned long int hashed) {

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
     * Кроме того, есть начальные черные (n-1)/2 генераторов, которые порождают
     * неучтенные черные области.
     *
     * Внешние области чередующегося цвета в количестве n + 1, которые пересекаются
     * в начальном положении сканирующей прямой и которые тоже не учтены,
     * вклада в разность не дают.
     */

    int s = level_limit - 1 + (n - 1) / 2;

    printf(" [%fs] A=%d", time_taken, s);
    printf("\t%lu\t%lu)", iterations, hashed);

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

    malloc_stats();
    printf("\n");
    hash_table_print_stats(table);

    printf("\n");

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
unsigned int inline should_process(line_num* cur_gen, line_num prev_gen) {
    // Пример: если предыдущий генератор 2, заканчивать надо на 4

    if (*cur_gen == INT_FAST8_MAX) {
        // С помощью условия пропускаем значение -2.
        *cur_gen = (prev_gen > 0) ? prev_gen - 2 : n - 3;

        return 1;
    }

    if (*cur_gen + 2 == prev_gen) {
        if (n - 3 <= prev_gen) {
            // Предотвращаем зацикливание. Когда prev_gen на максимуме и равен n-3, переключать на n-3 нельзя
            return 0;
        }
        *cur_gen = n - 3;
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
    unsigned long int iterations = 0;
    unsigned long int h = 0;
    max_level = 0;

    // Оптимизация 0. Первые генераторы должны образовать (n-1)/2 внешних черных двуугольников.
    for (int i = n / 2; i--; ) {
        set(i * 2 + 1, 1);
    }

    stat[level + 1].generator = 0; // Начальное значение для эвристики. TODO может меняться вместе с ней.
    stat[level].generator = init_working_gen(); // завышенное несуществующее значение, будет уменьшаться

    while (1) {
        iterations++; // Раскомментировать при добавлении новых условий оптимизации для "профилировки".
#ifdef DEBUG
        printf("-->> start lev = %d gen = %d\n", level, stat[level].generator);
#endif

        if (should_process(&stat[level].generator, stat[level + 1].generator)) {

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

            // Применяем белый генератор curr_generator и ассоциированные соседние черные генераторы
            do_cross_with_assoc(curr_generator);

            if (hash_table_contains(table, a, n)) {
                // printf("level = %d\n", level);
                // for (int r = 0; r < n; r++) {
                //     printf(" %d", a[r]);
                // }
                // printf("\n");

                // for (int r = level_limit-1; r >= level; r--) {
                //     // Белый генератор
                //     printf(" %d", (int)stat[r].generator);
                //     // Ассоциированные черные генераторы
                //     if (stat[r].generator == 0) {
                //         printf(" 1");
                //     }
                //     else if (stat[r].generator % 2 == 0) {
                //         printf(" %d %d", (int)stat[r].generator - 1, (int)stat[r].generator + 1);
                //     }
                // }
                // printf("\n");


                do_uncross_with_assoc(curr_generator);
                continue;
            }

            level--;

            if (level == -1) {
                count_gen(iterations, h);

#ifdef DEBUG
                printf("count-gen\n");
#endif
                goto up; // эквивалентно строке ниже, при изменении кода другой вариант может быть быстрее
                // stat[level].generator = init_skip_gen(curr_generator); // перебора не будет, чтобы вернуться
            }
            else {
                stat[level].generator = init_working_gen(); // запускаем перебор заново на другом уровне
            }
        }
        else {
        up:
            // Для корректной работы нужно это условие. Без него программа зациклится или вызовет seg fault.
            // Но его пропуск ускоряет программу. При этом она всё равно распечатает конфигурации.
            // Раскомментировать, например, при полном поиске.
            if (level == level_limit - 1) {
                printf("search finished\n");
                break;
            }

//            if (1.0 * level / level_limit < 0.9) {
                hash_table_insert(table, a, n);
                h++;
//            }

            level++;

            // Отменяем генераторы в обратном порядке, так как они не коммутируют
            do_uncross_with_assoc(stat[level].generator);
        }
    }
}

int main(int argc, char** argv) {
    int i;
    char s[80];

    // 53, 97, 193, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
    size_t bucket_count = 50331653;
    // size_t bucket_count = 100663319;
    // size_t bucket_count = 201326611;

    table = hash_table_create(bucket_count);

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
/*
    line_num ddd[n1] = { 20, 19, 18, 17, 16, 12, 10, 14, 8, 6, 4, 5, 2, 15, 3, 13, 9, 11, 7, 1, 0};
    line_num ttt[n1] = { 20, 19, 18, 17, 16, 12, 10, 14, 8, 6, 4, 5, 2, 15, 3, 13, 9, 11, 7, 0, 1};

    if (hash_table_contains(table, ddd, n)) {
        printf("Found!\n");
    } else {
        printf("Not found \n");
    }

    hash_table_insert(table, ddd, n);

    if (hash_table_contains(table, ttt, n)) {
        printf("Found!\n");
    } else {
        printf("Not found \n");
    }

return 0;*/

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

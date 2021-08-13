#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include<signal.h>

// May vary
#define n1 39
#define n_step1 (n1*(n1-1) / 2)

// #define DEBUG 1

int n;

int level = 0;

typedef struct {
    int generator;
    // int prev_polygon_cnt;
    int var;
} Stat;

Stat stat[n_step1 + 1];

int a[n1]; // Текущая перестановка 
int d[n1 + 1]; // Количество боковых вершин в текущих многоугольниках вокруг прямых

int b_free; // Количество оставшихся генераторов для подбора
int max_s = 0, max_defects = 0;

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

    printf("\n [%f] level = %d/%d", time_taken, level, b_free);
    printf("\n");
    for (int i = 0; i < level; i++) {
        if (stat[i].generator % 2) {
            continue;
        }
        printf("%.*s", stat[i].generator, "                            ");
        printf(" %d \n", stat[i].generator);
    }

    printf("\n");

    if (time_taken - last_signal < 1) {
        exit(0);
    }

    last_signal = time_taken;
}

void count_gen(int level) {
    struct timeval end;

    int s, i, max_defects1;
    FILE* f;

    s = -1 + (n & 1); // Поправка от избытка во внешней области

    for (i = 0; i < level; i++) {
        s -= (stat[i].generator & 1) * 2 - 1;
    }
    if (s < 0) {
        s = -s;
    }
    if (s < max_s || !full && s == max_s) {
        return;
    }

    max_s = s;
    max_defects1 = ((n * (n + 1) / 2 - 3) - 3 * max_s) / 2;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds


    if (strcmp(filename, "") == 0) {
        printf(" %f A=%d %d %d)", time_taken, s, max_defects, max_defects1);
        for (i = 0; i < level; i++) {
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
        for (i = 0; i < level; i++) {
            fprintf(f, " %d", stat[i].generator);
        }

        fprintf(f, "\n");
        fclose(f);
    }
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
void inline set(int generator, int cross_direction) {
    int i;

    //    b_free += -2 * cross_direction + 1; // Корректируем кол-во оставшихся генераторов

    i = a[generator];
    a[generator] = a[generator + 1];
    a[generator + 1] = i;
}

void inline do_cross(int level, int generator) {

#ifdef DEBUG
    printf("cross lev = %d gen = %d\n", level, generator);
#endif

    // stat[level + 1].prev_polygon_cnt = d[generator + 1];

    // d[generator + 1] = 0;
    // d[generator]++;
    // d[generator + 2]++;

    set(generator, 1);
}

void inline do_uncross(int level, int generator) {

#ifdef DEBUG
    printf("un-cross lev = %d gen = %d\n", level, generator);
#endif

    set(generator, 0);

    // d[generator]--;
    // d[generator + 2]--;
    // d[generator + 1] = stat[level + 1].prev_polygon_cnt;
}

// Стратегия перебора -2, +2, +4 ...

int inline should_process(int cur_gen, int prev_gen) {

    int k = cur_gen < n - 3;

    return k;
}

void inline modify_generator(int* cur_gen, int prev_gen) {
    // Пример: предыдущий генератор 2. Инициализируем 2, сразу вычитаем, получаем 0
    // Следующим шагом инкрементим до максимума и вычитаем.
    *cur_gen += 2;

    if (*cur_gen < 0) {
        *cur_gen += 2;
    }

    if (*cur_gen == prev_gen) {
        *cur_gen += 2;
    }
}

int inline init_working_gen(int generator) {
    return generator - 4;
}

int inline init_skip_gen(int curr_generator) {
    return curr_generator + 2; // todo
}

// Стратегия перебора -2, n-3, n-5 ...
// int inline should_process(int cur_gen, int prev_gen) {
//     // Пример: если предыдущий генератор 2, заканчивать надо на 4
//     int k = cur_gen != prev_gen + 2 && !(cur_gen == prev_gen && prev_gen + 3 == n);
//     // printf("->>>>>>>>>>>>>>> desicion = %d, cur_gen = %d prev_gen = %d\n", k, cur_gen, prev_gen);
//
//     return k;
// }
//
// void inline modify_generator(int* cur_gen, int prev_gen) {
//     // Пример: предыдущий генератор 2. Инициализируем 2, сразу вычитаем, получаем 0
//     // Следующим шагом инкрементим до максимума и вычитаем.
//     if (*cur_gen == -100) {
//         *cur_gen =  (prev_gen > 0) ? prev_gen - 2 : n - 3;
//         return ;
//     }
//
//     if (*cur_gen + 2 == prev_gen) {
//         *cur_gen =  n - 3;
//         return;
//     }
//
//     *cur_gen -= 2;
// }
//
// int inline init_working_gen(int generator) {
//     return -100;
// }
//
// int inline init_skip_gen(int curr_generator) {
//     return curr_generator + 2;
// }

// Стратегия перебора от большего к меньшему
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
    int curr_generator, start_level;
//    int level = 0;

    max_s = 0;


    for (int i = n + 1; i--; ) {
        d[i] = 2;
    }

    for (int i = n / 2; i--; ) {
        stat[level].generator = i * 2 + 1;
        do_cross(level, i * 2 + 1);
        level++;
    }

    start_level = level;

    stat[level].generator = init_working_gen(n-3); // завышенное несуществующее значение, будет уменьшаться
    stat[level].var = 0;

    while (1) {
#ifdef DEBUG 
        printf("-->> start lev = %d gen = %d\n", level, stat[level].generator);
#endif

        if (should_process(stat[level].generator, stat[level].var)) {
            modify_generator(&stat[level].generator, stat[level].var);

            curr_generator = stat[level].generator;

#ifdef DEBUG 
            printf("test lev = %d gen = %d prev = %d b_free = %d\n", level, curr_generator, stat[level].var, b_free);
#endif


            if (curr_generator != 0) {
                if (a[curr_generator - 1] > a[curr_generator + 1]) {
                    continue;
                }

                if (a[curr_generator] > a[curr_generator + 2]) {
                    continue;
                }
            }
            else {
                // Нулевой генератор может пересечь только первую и последнюю прямые
                if (a[curr_generator + 1] != n - 1) {
                    continue;
                }
                if (a[curr_generator] != 0) {
                    continue;
                }
            }

            if (a[curr_generator] > a[curr_generator + 1]) {
                continue;
            }

            // Optimization
            // Запрещаем белые треугольники и квадраты (хотя треугольники запрещены автопостроением черных треугольников)
            // if (d[curr_generator + 1] < 3) {
            //     // printf("no-white-squares\n");
            //     continue;
            // }

            do_cross(level, curr_generator);
            level++;
            // stat[level].var = curr_generator;

            if (curr_generator != 0) {
                stat[level].generator = curr_generator - 1;
                do_cross(level, curr_generator - 1);
                level++;
                // stat[level].var = curr_generator;
            }

            stat[level].generator = curr_generator + 1;
            do_cross(level, curr_generator + 1);
            level++;
            stat[level].var = curr_generator;

            if (level == b_free) {
                count_gen(level);

#ifdef DEBUG 
                printf("count-gen\n");
#endif

                stat[level].generator = init_skip_gen(curr_generator); // перебора не будет, чтобы вернуться
            }
            else {
                stat[level].generator = init_working_gen(curr_generator); // запускаем перебор заново на другом уровне
            }
        }
        else {
            if (level <= start_level) {
                printf("level <= start_level\n");
                break;
            }

            level--;
            // #ifdef DEBUG 
            // printf("uncross-1\n");
            // #endif
            do_uncross(level, stat[level].generator);

            if (stat[level].generator != 1) {
                level--;
                // #ifdef DEBUG 
                // printf("uncross-2\n");
                // #endif
                do_uncross(level, stat[level].generator);
            }

            level--;
            // #ifdef DEBUG 
            // printf("uncross-3\n");
            // #endif
            do_uncross(level, stat[level].generator);
        }
    }
}

int main(int argc, char** argv) {
    int i;
    char s[80];

    if (argc <= 1) {
        printf("Usage: %s -n N [-o filename]\n  -n\n	 line count;\n  -o\n	 output file.\n", s, argv[0]);

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
            printf("Invalid parameter: \"%s\".\nUsage: %s -n N [-o filename]\n  -n\n	 line count;\n  -o\n	 output file.\n", s, argv[0]);

            return 0;
        }
    }

    if (n % 2 == 0) {
        printf("This program is designed for fast search of odd defectless configurations only.");

        return 0;
    }

    b_free = n * (n - 1) / 2;

    for (i = 0; i < n; i++) {
        a[i] = i;
    }

    gettimeofday(&start, NULL);
    calc();

    printf("---------->>> Process terminated.\n");

    return 0;
}

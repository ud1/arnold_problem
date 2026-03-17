/**
 * Программа осуществляет обход дерева для поиска бездефектных конфигураций.
 *
 * Вариант ищет зеркально-симметричные конфигурации.
 * Поиск идет от середины к краям.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>

// May vary
#define n1 69
#define n_step1 (n1*(n1-1) / 2)

// #define DEBUG 1

int n;

int level = 0;

typedef struct {
    int generator;
    // int prev_polygon_cnt; // Кеш для количества текущих вершин
    int prev_generator;
} Stat;

Stat stat[n_step1 + 1];

int a[n1]; // Текущая перестановка
int a2[n1]; // Текущая перестановка вверх
int b[n1][n1]; // B-матрица

int b_free; // Количество оставшихся генераторов для подбора
int max_s;

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
        printf("%.*s", stat[i].generator, "                                              ");
        printf(" %d \n", stat[i].generator);
    }

    printf("\n");

    if (time_taken - last_signal < 1) {
        exit(0);
    }

    last_signal = time_taken;
}

void dump_rearrangement() {
    int i;

    printf("a = ");

    for (i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }

    printf("\nb =\n");

    for (i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf(" %d", b[i][j]);
        }
        printf("\n");
    }

    printf("\n");
}

void count_gen(int level, unsigned long int iterations) {
    struct timeval end;

    int s, i;
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

    s = s * 2 + n / 2; // Поправка от дублирующих генераторов

    max_s = s;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6; // in seconds

    // dump_rearrangement();

    if (strcmp(filename, "") == 0) {
        printf(" %f A=%d i=%u)", time_taken, s, iterations);

        for (i = level; i--; ) {
            printf(" %d", stat[i].generator);
        }

        printf(" 1 0 1");
        for (int i = 3; i < n; i += 2) {
            printf(" %d", i);
        }

        for (i = 0; i < level; i++) {
            printf(" %d", stat[i].generator);
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
    }
    else {
        f = fopen(filename, "a");

        fprintf(f, " A = %d)", s);
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

    b[a[generator]][a[generator + 1]] = cross_direction;
    b[a[generator + 1]][a[generator]] = cross_direction;
}

void inline set2(int generator, int cross_direction) {
    int i;

    //    b_free += -2 * cross_direction + 1; // Корректируем кол-во оставшихся генераторов

    i = a2[generator];
    a2[generator] = a2[generator + 1];
    a2[generator + 1] = i;

    b[a2[generator]][a2[generator + 1]] = cross_direction;
    b[a2[generator + 1]][a2[generator]] = cross_direction;
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
    set2(generator, 1);
}

void inline do_uncross(int level, int generator) {

#ifdef DEBUG
    printf("un-cross lev = %d gen = %d\n", level, generator);
#endif

    set(generator, 0);
    set2(generator, 0);

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

    if (*cur_gen <= 0) {
        *cur_gen += 2;
    }

    if (*cur_gen <= 0) {
        *cur_gen += 2;
    }

    if (*cur_gen == prev_gen) {
        *cur_gen += 2;
    }
}

int inline init_working_gen(int generator) {
    return generator - 4;
}

// Стратегия перебора -2, n-3, n-5 ...

// int inline should_process(int cur_gen, int prev_gen) {
//     // Пример: если предыдущий генератор 2, заканчивать надо на 4
//     int k = cur_gen != prev_gen + 2 && !(cur_gen == prev_gen && prev_gen + 3 == n);
//     // printf("->>>>>>>>>>>>>>> desicion = %d, cur_gen = %d prev_gen = %d\n", k, cur_gen, prev_gen);

//     return k;
// }

// void inline modify_generator(int* cur_gen, int prev_gen) {
//     // Пример: предыдущий генератор 2. Инициализируем 2, сразу вычитаем, получаем 0
//     // Следующим шагом инкрементим до максимума и вычитаем.
//     if (*cur_gen == -100) {
//         *cur_gen = (prev_gen > 2) ? prev_gen - 2 : n - 3;
//         return;
//     }

//     if (*cur_gen + 2 == prev_gen) {
//         *cur_gen = n - 3;
//         return;
//     }

//     *cur_gen -= 2;
// }

// int inline init_working_gen(int generator) {
//     return -100;
// }

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

// Calculations for defectless configurations
void calc() {
    int curr_generator, start_level;
    unsigned long int iterations = 0;
    //    int level = 0;

    max_s = 0;

    // Инициализируем генераторы по центру
    set(0, 1);

    set(1, 1); // черный генератор, ассоциированный с предыдущим белым
    set2(1, 1); // черный генератор, ассоциированный с предыдущим белым

    for (int i = 3; i < n; i += 2) {
        set(i, 1);
    }

    start_level = level;

    stat[level].generator = init_working_gen(2);
    stat[level].prev_generator = 0;

    while (1) {
        iterations++;
#ifdef DEBUG 
        printf("-->> start lev = %d gen = %d prev_gen = %d\n", level, stat[level].generator, stat[level].prev_generator);
#endif

        if (should_process(stat[level].generator, stat[level].prev_generator)) {
            modify_generator(&stat[level].generator, stat[level].prev_generator);

            curr_generator = stat[level].generator;

#ifdef DEBUG 
            printf("test lev = %d gen = %d prev = %d b_free = %d\n", level, curr_generator, stat[level].prev_generator, b_free);
#endif

            if (b[a[curr_generator - 1]][a[curr_generator + 1]]) {
                continue;
            }

            if (b[a[curr_generator]][a[curr_generator + 2]]) {
                continue;
            }

            if (b[a[curr_generator]][a[curr_generator + 1]]) {
                continue;
            }

            do_cross(level, curr_generator);
            level++;
            // stat[level].prev_generator = curr_generator;

            stat[level].generator = curr_generator - 1;
            do_cross(level, curr_generator - 1);
            level++;
            // stat[level].prev_generator = curr_generator;

            stat[level].generator = curr_generator + 1;
            do_cross(level, curr_generator + 1);
            level++;
            stat[level].prev_generator = curr_generator;

#ifdef DEBUG
            dump_rearrangement();
#endif

            if (level == b_free) {
                count_gen(level, iterations);

#ifdef DEBUG 
                printf("count-gen\n");
#endif

                goto up;
            }
            else {
                stat[level].generator = init_working_gen(curr_generator); // запускаем перебор заново на другом уровне
            }
        }
        else {
        up:
            if (level <= start_level) {
                printf("level <= start_level %u\n", iterations);
                break;
            }

            level--;
            do_uncross(level, stat[level].generator);

            // if (stat[level].generator != 1) {
            level--;
            do_uncross(level, stat[level].generator);
            // }

            level--;
            do_uncross(level, stat[level].generator);
        }
    }
}

int main(int argc, char** argv) {
    int i;
    char s[80];

    if (argc <= 1) {
        printf("Usage: %s -n N [-o filename]\n  -n\n	 line count;\n  -o\n	 output file.\n", argv[0]);

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

    b_free = (n * (n - 1) / 2 - n / 2) / 2 - 1; // с поправкой от генераторов по центру, выше диагонали и без генератора 1

    for (i = 0; i < n; i++) {
        a[i] = i;
        a2[i] = i;
        for (int j = 0; j < n; j++) {
            b[i][j] = 0;
        }
    }

    gettimeofday(&start, NULL);
    calc();

    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

    printf("---------->>> Process terminated at %f.\n", time_taken);

    return 0;
}

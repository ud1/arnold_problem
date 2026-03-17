#include <math.h>
#include <mpi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "lib.h"
#include "messaging.h"

static int max_s;

static volatile int alarm_fired = 0;

static void handle_alarm(int s) {
    alarm_fired = 1;
}

static int current_worker_id;

static void handle_interrupt(const int s) {
    printf(">>>>>>>>>>>>> Worker %d: Received %d signal\n", current_worker_id, s);
}

static struct {
    // Worker's timings
    unsigned long int run_time;
    unsigned long int idle_time;

    // Network's timings
    unsigned long int network_time;
} timings;

void send_message_to_dispatcher(Message *msg) {
    struct timeval sending_start;

    msg->w_idle_time = timings.idle_time;
    msg->w_run_time = timings.run_time;
    msg->network_time = timings.network_time;

    gettimeofday(&sending_start, NULL);
    MPI_Send(msg, sizeof(Message) / sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD);

    const unsigned long int send_time = time_after_us(sending_start);
    timings.idle_time += send_time;
    timings.network_time += send_time;
}

static void load_queue(const char *filename) {
    int res;
    char *str = (char *) malloc(65536), *ptr;
    int state = 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("File %s not found, exit\n", filename);
        exit(0);
    }
    while (res = fscanf(f, "%[^\n]\n", str), res != -1 && res != 0) {
        if (str[0] == '/') {
            continue;
        }
        // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
        switch (state) { // NOLINT(*-multiway-paths-covered)
            case 0:
                ptr = str;
                n = (int) strtol(ptr, &ptr, 10);
                state = 1;
                break;
            case 1:
                ptr = str;
                max_s = (int) strtol(ptr, &ptr, 10);
                state = 2;
                break;
        }
    }

    fclose(f);
    free(str);
}

// Функция для вычисления коэффициентов параболы y = a*x² + b*x + c
static void fitParabola(const int *x_values, const double *y_values, int n, double *a, double *b, double *c) {
    double sum_x = 0, sum_x2 = 0, sum_x3 = 0, sum_x4 = 0;
    double sum_y = 0, sum_xy = 0, sum_x2y = 0;

    for (int i = 0; i < n; i++) {
        double xi = x_values[i];
        double yi = y_values[i];

        sum_x += xi;
        sum_x2 += xi * xi;
        sum_x3 += xi * xi * xi;
        sum_x4 += xi * xi * xi * xi;
        sum_y += yi;
        sum_xy += xi * yi;
        sum_x2y += xi * xi * yi;
    }

    // Решаем систему уравнений методом Крамера
    double det = sum_x4 * (sum_x2 * n - sum_x * sum_x) -
                 sum_x3 * (sum_x3 * n - sum_x * sum_x2) +
                 sum_x2 * (sum_x3 * sum_x - sum_x2 * sum_x2);

    if (fabs(det) < 1e-10) {
        // Матрица вырождена, нет решения
        *a = *b = *c = 0;
        return;
    }

    double det_a = sum_x2y * (sum_x2 * n - sum_x * sum_x) -
                   sum_xy * (sum_x3 * n - sum_x * sum_x2) +
                   sum_y * (sum_x3 * sum_x - sum_x2 * sum_x2);

    double det_b = sum_x4 * (sum_xy * n - sum_x * sum_y) -
                   sum_x3 * (sum_x2y * n - sum_x2 * sum_y) +
                   sum_x2 * (sum_x2y * sum_x - sum_x2 * sum_xy);

    double det_c = sum_x4 * (sum_x2 * sum_y - sum_x * sum_xy) -
                   sum_x3 * (sum_x3 * sum_y - sum_x * sum_x2y) +
                   sum_x2 * (sum_x3 * sum_xy - sum_x2 * sum_x2y);

    *a = det_a / det;
    *b = det_b / det;
    *c = det_c / det;
}

// Функция для нахождения корней параболы
int findParabolaRoots(double a, double b, double c, double *root1, double *root2) {
    double discriminant = b * b - 4 * a * c;

    if (discriminant < 0 || fabs(a) < 1e-10) {
        return 0; // Нет действительных корней или это не парабола
    }

    *root1 = (-b - sqrt(discriminant)) / (2 * a);
    *root2 = (-b + sqrt(discriminant)) / (2 * a);

    return 1;
}

static double calculateRSquared(const int *x_values, const double *y_values, int n, double a, double b, double c) {
    double y_mean = 0;
    for (int i = 0; i < n; i++) {
        y_mean += y_values[i];
    }
    y_mean /= n;

    double ss_total = 0;
    double ss_res = 0;

    for (int i = 0; i < n; i++) {
        double predicted = a * x_values[i] * x_values[i] + b * x_values[i] + c;
        ss_total += (y_values[i] - y_mean) * (y_values[i] - y_mean);
        ss_res += (y_values[i] - predicted) * (y_values[i] - predicted);
    }

    return 1 - (ss_res / ss_total);
}

static double calc_evaluation_log_square() {
    // Сначала собираем данные для аппроксимации
    int x_values[n_step_limit + 1];
    double y_values[n_step_limit + 1];
    int point_count = 0;

    // Заполняем массивы значениями, где x > 1 и level_count > 0
    unsigned long int maxVal = 1;
    for (int i = n_step + 1; i--;) {
        if (stat[i].processed > maxVal) {
            maxVal = stat[i].processed;
            x_values[point_count] = i;
            y_values[point_count] = log((double) stat[i].processed);
            point_count++;
            // printf("x = %d y = %lu\n", i, stat[i].processed);
            if (point_count >= 20) {
                break;
            }
        }
    }

    //    printf("point_count = %d\n", point_count);

    if (point_count < 3) {
        // printf("Недостаточно точек для аппроксимации параболы\n");
        return n * n;
    }

    // Аппроксимируем параболу
    double a, b, c;
    fitParabola(x_values, y_values, point_count, &a, &b, &c);

    // double r2 = calculateRSquared(x_values, y_values, point_count, a, b, c);
    // printf("r2 = %e\n", r2);

    // Находим корни параболы
    double root1, root2;
    if (!findParabolaRoots(a, b, c, &root1, &root2)) {
        // printf("Парабола не пересекает ось X\n");
        return n * n;
    }

    // printf("root1 = %e root2 = %e\n", root1, root2);
    // exit(0);
    // Возвращаем наименьший корень
    return (root1 > root2) ? root1 : root2;
}

// Функция для вычисления коэффициентов прямой y = k*x + b
static void fitLine(const int *x_values, const double *y_values, int n, double *k, double *b) {
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;

    for (int i = 0; i < n; i++) {
        const double xi = x_values[i];
        const double yi = y_values[i];

        sum_x += xi;
        sum_y += yi;
        sum_xy += xi * yi;
        sum_x2 += xi * xi;
    }

    const double denominator = n * sum_x2 - sum_x * sum_x;
    if (fabs(denominator) < 1e-10) {
        // Прямая вертикальная или точек недостаточно
        *k = 0;
        *b = 0;
        return;
    }

    *k = (n * sum_xy - sum_x * sum_y) / denominator;
    *b = (sum_y - (*k) * sum_x) / n;
}

static double calc_evaluation_log_linear() {
    // Собираем данные для аппроксимации
    int x_values[n_step_limit + 1];
    double y_values[n_step_limit + 1];
    int point_count = 0;

    // Заполняем массивы значениями, где x > 1 и level_count > 0
    unsigned long int maxVal = 1;
    for (int i = n_step + 1; i--;) {
        if (stat[i].processed > maxVal) {
            maxVal = stat[i].processed;
            x_values[point_count] = i;
            y_values[point_count] = log((double) stat[i].processed);
            point_count++;
            if (point_count >= 5) {
                break;
            }
        }
    }

    if (point_count < 2) {
        return n * n;
    }

    // Аппроксимируем прямую
    double k, b;
    fitLine(x_values, y_values, point_count, &k, &b);

    // Находим точку пересечения с осью X (y=0)
    if (fabs(k) < 1e-10) {
        return n * n;
    }

    const double root = -b / k;
    return root;
}

static double calc_evaluation_avg() {
    double sum = 0;
    double weight = 0;

    for (int i = n_step + 1; i--;) {
        if (stat[i].processed > 1) {
            const double w = ((double) stat[i].processed);
            sum += w * i;
            weight += w;
        }
    }

    if (weight == 0) {
        return n * n;
    }

    return sum / weight;
}

static double calc_evaluation_log_avg() {
    double sum = 0;
    double weight = 0;

    for (int i = n_step + 1; i--;) {
        if (stat[i].processed > 1) {
            const double w = log1p((double) stat[i].processed);
            sum += w * i;
            weight += w;
        }
    }

    if (weight == 0) {
        return n * n;
    }

    return sum / weight;
}

static double calc_evaluation () {
#if GENERATOR_SORTING == SORTING_ORDER
    return 0.0;
#elif GENERATOR_SORTING == SORTING_ITERATIONS
    return 0.0;
#elif GENERATOR_SORTING == SORTING_EVALUATION_LOG_LINEAR
    return calc_evaluation_log_linear();
#elif GENERATOR_SORTING == SORTING_EVALUATION_LOG_SQUARE
    return calc_evaluation_log_square();
#elif GENERATOR_SORTING == SORTING_EVALUATION_LOG_AVG
    return calc_evaluation_log_avg();
#elif GENERATOR_SORTING == SORTING_EVALUATION_AVG
    return calc_evaluation_avg();
#endif
}

void count_gen(const int level, const unsigned long int iterations, const struct timeval start, const char *filename,
               const char *node_name, int full) {
    struct timeval end;
    FILE *f;

    gettimeofday(&end, NULL);
    const double time_taken = (double) end.tv_sec + (double) end.tv_usec / 1e6 - (double) start.tv_sec - (double) start.
                              tv_usec / 1e6; // in seconds

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

    int s = n_step - 1 + (n - 1) / 2;

    if (s > max_s) {
        max_s = s;
    } else {
        if (!full) {
            return;
        }
    }

    if (strcmp(filename, "") == 0) {
        f = stdout;
        fprintf(f, "\n%s [%fs] A=%d", node_name, time_taken, s);
    } else {
        f = fopen(filename, "a");
        fprintf(f, " [%fs] A=%d", time_taken, s);
    }

    fprintf(f, " i=%e)", (double) iterations);

    for (unsigned int i = n / 2; i--;) {
        fprintf(f, " %d", i * 2 + 1);
    }

    for (int i = 1; i <= level + 1; i++) {
        // Белый генератор
        fprintf(f, " %d", (int) stat[i].generator);
        // Ассоциированные черные генераторы
        if (stat[i].generator == 0) {
            fprintf(f, " 1");
        } else if (stat[i].generator % 2 == 0) {
            fprintf(f, " %d %d", (int) stat[i].generator - 1, (int) stat[i].generator + 1);
        }
    }

    fprintf(f, "\n");

    if (strcmp(filename, "") == 0) {
        fprintf(f, "\n");
        fflush(NULL);
        fflush(NULL);
        fflush(stdout);
        fflush(stdout);
    } else {
        fclose(f);
    }
}

/**
 * Пересекает прямые по генератору
 *
 * @param   generator       номер генератора
 * @param   cross_direction 1 если косичка переплетается, 0 если распутывается
 *
 * @return  void
 */
void inline set(const unsigned int generator, unsigned int cross_direction) {
    const line_num i = a[generator];
    a[generator] = a[generator + 1];
    a[generator + 1] = i;
}

/**
 * Применение белого генератора и двух ассоциированных (соседних) черных.
 */
void inline do_cross_with_assoc(const unsigned int generator) {
    line_num i = a[generator];
    if (generator > 0) {
        a[generator] = a[generator - 1];
        a[generator - 1] = a[generator + 1];
    } else {
        a[generator] = a[generator + 1];
    }
    a[generator + 1] = a[generator + 2];
    a[generator + 2] = i;
}

/**
 * Отмена применения белого генератора и двух ассоциированных (соседних) черных.
 */
void inline do_uncross_with_assoc(const unsigned int generator) {
    line_num i = a[generator + 2];
    a[generator + 2] = a[generator + 1];

    if (generator > 0) {
        a[generator + 1] = a[generator - 1];
        a[generator - 1] = a[generator];
    } else {
        a[generator + 1] = a[generator];
    }
    a[generator] = i;
}

void my_alarm(const long int microseconds) {
    struct itimerval timer;
    timer.it_value.tv_sec = microseconds / 1000000;
    timer.it_value.tv_usec = microseconds % 1000000;
    timer.it_interval.tv_sec = 0; // Интервал повторения (0 = одноразовый)
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
}

int inline can_apply(const int curr_generator) {
    if (curr_generator > n - 3 || curr_generator < 0) {
        return 0;
    }
    if (a[curr_generator] > a[curr_generator + 1]) {
        return 0;
    }


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
            return 0;
        }

        // Оптимизация 1 и 3.
        // На самом деле если ее убрать, в перебор пойдут недопустимые наборы генераторов
        if (a[curr_generator] > a[curr_generator + 2]) {
            return 0;
        }

        // Оптимизация 2.
        if (a[curr_generator] + 1 == a[curr_generator + 2] && curr_generator + 1 + a[curr_generator + 2] != n -
            1) {
            return 0;
        }

        // Оптимизация 1.
        // Как показывает профилировка, после остальных оптимизаций эта не дает ускорения
        // if (a[curr_generator] > a[curr_generator + 1]) {
        //     return 0;
        // }

        // Оптимизация 2.
        if (a[curr_generator - 1] + 1 == a[curr_generator + 1] && curr_generator - 1 + a[curr_generator + 1] !=
            n - 1) {
            return 0;
        }

        // Оптимизация 2. Закомментировано, потому что профилировка показывает, что количество итераций слегка сокращается, а время выполнения увеличивается.
        // if (a[curr_generator - 1] - 1 == a[curr_generator] && a[curr_generator] + curr_generator == n-1) {
        //     return 0;
        // }
        // if (a[curr_generator + 1] - 1 == a[curr_generator + 2] && a[curr_generator + 1] - 1 + curr_generator == n-1) {
        //     return 0;
        // }
    } else {
        // Оптимизация 4.
        if (a[curr_generator + 1] != n - 1) {
            return 0;
        }
        if (a[curr_generator] != 0) {
            return 0;
        }
        // Здесь нет смысла проверять оптимизацию 2, так как прямая с номером 1 уже пересекла прямую n-1 и прямую 2.
        // Поэтому a[2] != 1, и пересечение a[1] == 0 и a[2] всегда возможно.
    }

    // Оптимизация 5. Закомментирована, так как не дает прироста в эвристике -2, n-3, n-5...
    // if (a[0] != n-1 && a[n-2] != n-1 && a[curr_generator + 1] != n-1) {
    //     continue;
    // }

    // Ограничение на поиск: максимальный генератор можно применить только два раза.
    // Слишком сильное, долгий перебор, но может пригодится в каких-нибудь эвристиках.
    // if (curr_generator == n - 3 && a[curr_generator + 1] != n-1 && a[curr_generator] != 0) {
    //     continue;
    // }

    return 1;
}

/**
 * Метод обеспечивает перебор генераторов. Если на вход установленые ненулевые уровни,
 * метод ожидает установленных полей stat[].rearr_index и stat[].rearrangement
 *
 * @param level     Уровень, начиная с которого идет перебор
 * @param min_level Уровень, до которого нужно подняться, не дальше от корня, чем level
 * @param iterations
 * @param evaluation
 * @param start
 * @param output_filename
 * @param node_name
 * @param full
 *
 * @return  void
 */
unsigned long int run(
    int level,
    int min_level,
    unsigned long int iterations,
    double evaluation,
    const struct timeval start,
    const char *output_filename,
    const char *node_name,
    const int full
) {
    int curr_generator, i;
    Message message;

    // Оптимизация 0. Первые генераторы должны образовать (n-1)/2 внешних черных двуугольников.
    for (i = n / 2; i--;) {
        set(i * 2 + 1, 1);
    }

    // Initializing
    for (i = 0; i <= n_step; ++i) {
        stat[i].processed = 0;
    }
    // printf(">>>>>>>>>>>>>>>>> %s Starting generator rearrangement\n", node_name);

    // Восстанавливаем генераторы на основе rearr_index, установленных перед запуском
    for (i = 0; i < level; ++i) {
        // printf(">>>>>>>>>>>>>>>>> %s i=%d, gen=%d, shift=%d, rearr_index=%d\n", node_name, i, stat[i].generator,
        // triplets[stat[i].rearrangement][stat[i].rearr_index], stat[i].rearr_index);
        curr_generator = stat[i].generator + triplets[stat[i].rearrangement][stat[i].rearr_index];

        if (!can_apply(curr_generator)) {
            return iterations;
        }

        stat[i + 1].generator = curr_generator;
        // printf(">>>>>>>>>>>>>>>>> %s new_gen=%d\n", node_name, stat[i + 1].generator);
        stat[i + 1].processed++;
        do_cross_with_assoc(curr_generator);
        // printf(">>>>>>>>>>>>>>>>> curr_generator = %d\n", curr_generator);
    }

    alarm_fired = 0;

    // Run
    while (1) {
        iterations++; // Раскомментировать при добавлении новых условий оптимизации для "профилировки".
        // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> it=%d  lev=%d ri=%d maxi=%d, cond=%d\n", iterations, level, stat[level].rearr_index, (n - 3)/2, stat[level].rearr_index < (int)(n - 3)/2);
        if (stat[level].rearr_index < (n - 3) / 2) {
            stat[level].rearr_index++;
            curr_generator = stat[level].generator + triplets[stat[level].rearrangement][stat[level].rearr_index];
            // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> next, curr_generator = %d, %d\n", curr_generator, triplets[stat[level].rearrangement][stat[level].rearr_index]);

            if (!can_apply(curr_generator)) {
                continue;
            }

            stat[level + 1].generator = curr_generator;
            stat[level + 1].processed++;

            if (n_step - 1 == level) {
                // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> count_gen!!! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
                count_gen(level, iterations, start, output_filename, node_name, full);
                if (!full) {
                    // Специальный признак, который говорит, что дальше работать не нужно
                    return 0;
                }
                continue;
            }

            // Применяем белый генератор curr_generator и ассоциированные соседние черные генераторы
            // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> down\n");
            do_cross_with_assoc(curr_generator);
            level++;
            stat[level].level_count++;
            stat[level].rearrangement = 0;
            stat[level].rearr_index = -1;

            if (alarm_fired) {
                trace("%s Alarm\n", node_name);

                alarm_fired = 0;
                my_alarm(TQ);

                for (i = 0; i <= level; ++i) {
                    message.rearr_index[i] = stat[i].rearr_index;
                    message.rearrangement[i] = stat[i].rearrangement;
                    message.level_count[i] = stat[i].level_count;
                }

                for (i = min_level; i < n_step; ++i) {
                    if (stat[i + 1].processed > 1)
                        break;
                }
                const int last_unprocessed_level = i;
#if FORK_JOB == FORK_CURRENT
                /**
                 * Мы отдаем диспетчеру работу по перебору на "текущих" уровнях
                 * last_unprocessed_level <= level, где поработали и знаем оценку.
                 *
                 * min_level <= target_level <= current_level
                 * min_level <= last_unprocessed_level <= level
                 */

                // Параметры для нового джоба
                message.target_level = last_unprocessed_level;
                message.current_level = level;

                /**
                 * Модифицируем текущую задачу, чтобы продолжить перебор на других уровнях.
                 */
                while (level > last_unprocessed_level) {
                    curr_generator = stat[level].generator;
                    level--;
                    do_uncross_with_assoc(curr_generator);
                }
#elif FORK_JOB == FORK_NEXT
                /**
                 * Мы отдаем диспетчеру работу по перебору на уровнях
                 * min_level <= last_unprocessed_level, где мы ничего не сделали.
                 *
                 * target_level <= current_level <= remaining_level
                 * min_level <= last_unprocessed_level <= level
                 */
                // Параметры для нового джоба
                message.target_level = min_level;
                message.current_level = last_unprocessed_level;
                // Сообщаем текущий уровень оставшегося джоба
                message.remaining_level = level;

                /**
                 * Модифицируем текущую задачу, чтобы закончить перебор на тех уровнях,
                 * где мы его уже начали.
                 */
                min_level = last_unprocessed_level;
#elif FORK_JOB == FORK_NONE
                message.target_level = min_level;
                message.current_level = level;
                message.remaining_level = level;
#endif

                message.evaluation = calc_evaluation();
                message.iterations = iterations;
                message.status = FORKED;

                trace("%s message.current_level = %d\n", node_name, message.current_level);
                message.max_s = max_s;

                send_message_to_dispatcher(&message);

#if FORK_JOB == FORK_NONE
                return iterations;
#endif

                for (i = 0; i <= n_step; ++i) {
                    stat[i].processed = 1;
                }
            }
        } else {
            // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> up\n");

            curr_generator = stat[level].generator;
            level--;
            do_uncross_with_assoc(curr_generator);

            if (level == min_level) {
                break;
            }
        }
    }

    return iterations;
}

void do_worker(
    const int id,
    const char *dump_filename,
    const char *output_filename,
    const char *node_name,
    const struct timeval start,
    const int full
) {
    Message message;
    MPI_Status status;
    int i;

    struct timeval work_start, waiting_start;

    current_worker_id = id;

    timings.run_time = 0;
    timings.idle_time = 0;
    timings.network_time = 0;

    // Timer initialization
    signal(SIGALRM, handle_alarm);
    signal(SIGINT, handle_interrupt);
    signal(SIGTERM, handle_interrupt);

#if MULTIPLET == MULTIPLET_DECR
    // -2, n-3, n-5, n-7, ..., 2
    for (i = 1; i <= (n - 3) / 2; i++) {
        triplets[0][i] = n - 1 - 2 * i;
    }
    triplets[0][0] = -2;
#elif  MULTIPLET == MULTIPLET_INCR
    // -2, 2, 4, 6, ..., n-3
    for (i = 1; i <= (n - 3) / 2; i++) {
        triplets[0][i] = 2 * i;
    }
    triplets[0][0] = -2;
#endif

    stat[0].generator = 0;

    if (dump_filename[0]) {
        load_queue(dump_filename);
    }

    gettimeofday(&waiting_start, NULL);

    for (;;) {
        // receive from dispatcher:
        trace("%s Waiting for a message from the dispatcher\n", node_name);
        MPI_Recv(&message, sizeof(Message) / sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD, &status);
        gettimeofday(&work_start, NULL);

        const unsigned long int waiting_for_msg_time = time_diff_us(work_start, waiting_start);
        timings.idle_time += waiting_for_msg_time;
        timings.network_time += waiting_for_msg_time - message.d_search_time;

        if (message.status == QUIT) {
            trace("%s Received 'quit' message\n", node_name);
            break;
        }

        trace("%s Received a message from the dispatcher\n", node_name);

        max_s = message.max_s;

        for (i = 0; i <= message.current_level; ++i) {
            //            trace("%d ", message.rearr_index[i]);
            stat[i].rearrangement = message.rearrangement[i];
            stat[i].rearr_index = message.rearr_index[i];
            stat[i].level_count = message.level_count[i];
        }
        //        trace(" [%s]\n", NODE_NAME);

        // Собираем статистику в этом запуске с нуля
        // TODO возможно, убрать из сообщений передачу level_count
        for (i = 0; i <= n_step; ++i) {
            stat[i].level_count = 0;
        }

        n_step = (n * (n - 2) + 3) / 6;
        for (i = 0; i < n; i++) {
            a[i] = i;
        }

        trace(
            "%s Run, current_level = %d, target_level = %d, iterations = %lu, evaluation = %f\n",
            node_name,
            message.current_level,
            message.target_level,
            message.iterations,
            message.evaluation
        );

        my_alarm(TQ);
        const unsigned long int result = run(
            message.current_level,
            message.target_level,
            message.iterations,
            message.evaluation,
            start,
            output_filename, node_name, full);

        gettimeofday(&waiting_start, NULL);
        timings.run_time += time_diff_us(waiting_start, work_start);

        if (message.iterations > 0 && result == 0) {
            message.status = HALT;
        } else {
            message.iterations = result;
            message.status = FINISHED;
        }
        trace("%s Finished\n", node_name);

        send_message_to_dispatcher(&message);
    }
}

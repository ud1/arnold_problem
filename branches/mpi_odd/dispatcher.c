#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "messaging.h"
#include "lib.h"
#include "dispatcher.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

// Worker's state
int *worker_statuses;
int worker_count;

void init_worker_statuses(const int cnt) {
    worker_count = cnt;
    worker_statuses = (int *) malloc(worker_count * sizeof(int));
    for (int i = 0; i < worker_count; ++i) {
        worker_statuses[i] = FINISHED;
    }
}

int get_worker_id_with_status(const int status) {
    for (int i = 0; i < worker_count; ++i) {
        if (worker_statuses[i] == status) {
            return i + 1;
        }
    }
    return -1;
}

void set_worker_state(const int worker_id, const int state) {
    worker_statuses[worker_id - 1] = state;
}

// Worker jobs
typedef struct {
    int rearrangement[n_step_limit];
    int rearr_index[n_step_limit];
    int level_count[n_step_limit];
    int target_level, current_level;
    double evaluation;
    unsigned long int iterations;
} job;

job *jobs;

typedef struct {
    unsigned long int idle_time, run_time, network_time;
    unsigned long int sum_iterations;
    struct timeval last_updated_at;
} worker_stat;

worker_stat *worker_stat_array;

// Глобальное состояние диспетчера
static int max_s;

// Задачи, которые ещё никому не отправлены
Message **message_queue = NULL;
int message_queue_reserved, message_queue_size;

static struct {
    // Dispatcher
    unsigned long int enqueue_time;
    unsigned long int enqueue_count;
    unsigned long int run_time;
    unsigned long int idle_time;
} dispatcher_timings;

int compare_messages(Message *ma, Message *mb) {
#if GENERATOR_SORTING == SORTING_ORDER
    int *p1 = ma->rearr_index;
    int *p2 = mb->rearr_index;
    // ReSharper disable once CppTooWideScopeInitStatement
    const int *end_pointer = p1 + min(ma->current_level, mb->current_level);

    for (; !(*p2 - *p1) && p1 <= end_pointer; ++p1, ++p2) {
        // noop;
    }
    return *p2 - *p1;
#elif GENERATOR_SORTING == SORTING_ORDER_REVERSE
    int *p1 = ma->rearr_index;
    int *p2 = mb->rearr_index;
    // ReSharper disable once CppTooWideScopeInitStatement
    const int *end_pointer = p1 + min(ma->current_level, mb->current_level);

    for (; !(*p2 - *p1) && p1 <= end_pointer; ++p1, ++p2) {
        // noop;
    }
    return - *p2 + *p1;
#elif GENERATOR_SORTING == SORTING_ITERATIONS
    return - ma->iterations + mb->iterations;
#else
    // Сортировка на основе оценки
    return ma->evaluation - mb->evaluation;
#endif
}

Message *dequeue_message() {
    if (message_queue_size > 0) {
        return message_queue[--message_queue_size];
    }
    return NULL;
}

void init_message_queue() {
    message_queue_reserved = 1000;
    message_queue_size = 0;
    dispatcher_timings.enqueue_count = 0;
    message_queue = (Message **) malloc(message_queue_reserved * sizeof(Message *));
}

void enqueue_message(Message *message, const struct timeval start) {
    ++dispatcher_timings.enqueue_count;
    if (message_queue_size >= message_queue_reserved) {
        // Удваиваем выделенную память под очередь задач
        Message **tmp = realloc(message_queue, (message_queue_reserved *= 2) * sizeof(Message *));
        if (tmp != NULL) {
            message_queue = tmp;
        }
    }

    unsigned int compare_counter = 0;
    int i = message_queue_size - 1;

    // Сдвигаем элементы больше message вправо
    while (i >= 0 && compare_messages(message, message_queue[i]) < 0) {
        message_queue[i + 1] = message_queue[i];
        compare_counter++;
        i--;
    }

    // Вставляем message на освободившееся место позицию
    message_queue[i + 1] = message;

    message_queue_size++;

    if (dispatcher_timings.enqueue_count % (10000000 / TQ) == 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        unsigned long int total_iterations = 0;
        for (int i = 0; i < worker_count; ++i) {
            total_iterations += worker_stat_array[i].sum_iterations;
        }

        printf(
            "-------> util=%.1f speed=%f qsize %d push=%lu pop=%lu eval=%.2f...%.2f i=%.2e...%.2e cmp=%d\n",
            100.0 * (double) dispatcher_timings.run_time / (double) (
                dispatcher_timings.run_time + dispatcher_timings.idle_time),
            (double) total_iterations / (double) time_diff_us(now, start),
            message_queue_size,
            dispatcher_timings.enqueue_count,
            dispatcher_timings.enqueue_count - message_queue_size,
            message_queue[0]->evaluation, message_queue[message_queue_size - 1]->evaluation,
            (double)message_queue[0]->iterations, (double)message_queue[message_queue_size - 1]->iterations,
            compare_counter
        );
    }
}


void print_timings(const int to_stdout) {
    FILE *f;
    if (!to_stdout) {
        char filename[64];
        sprintf(filename, "%d_timings.txt", n);
        f = fopen(filename, "a");
    } else {
        f = stdout;
    }

    unsigned long int workers_run_time = 0, workers_idle_time = 0, network_time = 0, total_iterations = 0;

    for (int i = 0; i < worker_count; ++i) {
        workers_run_time += worker_stat_array[i].run_time;
        workers_idle_time += worker_stat_array[i].idle_time;
        network_time += worker_stat_array[i].network_time;
        total_iterations += worker_stat_array[i].sum_iterations;
    }

    fprintf(f,
            "\n--- Total worker info (count = %d) ---\n"
            "Run time    %11.3f s\n"
            "Idle time   %11.3f s\n"
            "Utilization %11.3f %%\n"
            "Speed up = %f\n"
            "\n--- Dispatcher info ---\n"
            "Enqueue total time = %f s, count = %lu, avg = %lu us\n"
            "Run time    %11.3f s\n"
            "Idle time   %11.3f s\n"
            "Utilization %11.3f %%\n"
            "\n--- Other info ---\n"
            "Total iterations  %lu\n"
            "Time quantum      %d ms\n"
            "Interconnect loss %.3f s\n\n",
            worker_count,
            (double) workers_run_time / 1000000.0,
            (double) workers_idle_time / 1000000.0,
            100.0 * (double) workers_run_time / ((double) workers_run_time + (double) workers_idle_time),
            (double) workers_run_time / (double) (dispatcher_timings.run_time + dispatcher_timings.idle_time),
            (double) dispatcher_timings.enqueue_time / 1000000.0, dispatcher_timings.enqueue_count,
            dispatcher_timings.enqueue_time > 0
                ? dispatcher_timings.enqueue_time / dispatcher_timings.enqueue_count
                : 0,
            (double) dispatcher_timings.run_time / 1000000.0,
            (double) dispatcher_timings.idle_time / 1000000.0,
            100.0 / (1 + (double) dispatcher_timings.idle_time / (double) dispatcher_timings.run_time),
            total_iterations,
            TQ,
            (double) network_time / 1000000.0
    );
    if (!to_stdout) {
        fclose(f);
    }
}

void copy_generators_from_message(job *job, const Message *message) {
    memcpy(job->rearrangement, message->rearrangement, sizeof(message->rearrangement));
    memcpy(job->rearr_index, message->rearr_index, sizeof(message->rearr_index));
    memcpy(job->level_count, message->level_count, sizeof(message->level_count));
}

void send_message_to_worker(const int worker_id, const Message *message) {
    MPI_Send(message, sizeof(Message) / sizeof(int), MPI_INT, worker_id, TAG, MPI_COMM_WORLD);

    copy_generators_from_message(&jobs[worker_id - 1], message);
    jobs[worker_id - 1].current_level = message->current_level;
    jobs[worker_id - 1].target_level = message->target_level;
    jobs[worker_id - 1].evaluation = message->evaluation;
    jobs[worker_id - 1].iterations = message->iterations;
}

static void load_queue(const char *filename, const struct timeval start) {
    int i, res;
    Message *msg = NULL;
    char *str = malloc(65536), *ptr;
    int state = 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("File %s not found, exit\n", filename);
        exit(0);
    }
    while (res = fscanf(f, "%[^\n]\n", str), res != -1 && res != 0) {
        if (str[0] == '/' && str[1] == '/') {
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
            case 2:
                msg = (Message *) malloc(sizeof(Message));
                sscanf(str, "%d %d %lu %le", &msg->target_level, &msg->current_level, &msg->iterations, // NOLINT(*-err34-c)
                       &msg->evaluation);
                state = 3;
                break;
            case 3:
                ptr = str;
                if (msg == NULL) {
                    fprintf(stderr, "Error importing queue\n");
                    exit(1);
                }
                for (i = 0; i <= msg->current_level; i++) {
                    msg->rearrangement[i] = (int) strtol(ptr, &ptr, 10);
                }
                state = 4;
                break;
            case 4:
                ptr = str;
                if (msg == NULL) {
                    fprintf(stderr, "Error importing queue\n");
                    exit(1);
                }
                for (i = 0; i <= msg->current_level; i++) {
                    msg->rearr_index[i] = (int) strtol(ptr, &ptr, 10);
                }
                state = 5;
                break;
            case 5:
                if (msg == NULL) {
                    fprintf(stderr, "Error importing queue\n");
                    exit(1);
                }
                ptr = str;
                for (i = 0; i <= msg->current_level; i++) {
                    msg->level_count[i] = (int) strtol(ptr, &ptr, 10);
                }
                msg->max_s = max_s;
                msg->status = BUSY;
                enqueue_message(msg, start);
                state = 2;
                break;
        }
    }

    fclose(f);
    free(str);
}

void copy_timings_from_message(worker_stat *timings, const Message *msg) {
    timings->idle_time = msg->w_idle_time;
    timings->run_time = msg->w_run_time;
    timings->network_time = msg->network_time;
    gettimeofday(&timings->last_updated_at, NULL);
}

void dump_queue() {
    static int v = 1, i, j;
    char filename[256];
    v = 1 - v;

    sprintf(filename, "%d_dump_gens_%d", n, v);
    FILE *f = fopen(filename, "w+");
    fprintf(f, "// n\n%d\n", n);
    fprintf(f, "// max_s\n");
    fprintf(f, "%d\n", max_s);

    fprintf(
        f,
        "// target_level, current_level, iterations, evaluation, rearrangement[], rearr_index[], level_count[] ...\n");
    // ReSharper disable once CppDFALoopConditionNotUpdated
    for (i = 0; i < worker_count; ++i) {
        if (worker_statuses[i] == BUSY) {
            fprintf(f, "%d %d %lu %le\n", jobs[i].target_level, jobs[i].current_level,
                    jobs[i].iterations, jobs[i].evaluation);
            for (j = 0; j <= jobs[i].current_level; ++j)
                fprintf(f, "%d ", jobs[i].rearrangement[j]);
            fprintf(f, "\n");
            for (j = 0; j <= jobs[i].current_level; ++j)
                fprintf(f, "%d ", jobs[i].rearr_index[j]);
            fprintf(f, "\n");
            for (j = 0; j <= jobs[i].current_level; ++j)
                fprintf(f, "%d ", jobs[i].level_count[j]);
            fprintf(f, "\n");
        }
    }
    // ReSharper disable once CppDFALoopConditionNotUpdated
    for (i = 0; i < message_queue_size; ++i) {
        fprintf(f, "%d %d %lu %le\n", message_queue[i]->target_level, message_queue[i]->current_level,
                message_queue[i]->iterations,
                message_queue[i]->evaluation);
        for (j = 0; j <= message_queue[i]->current_level; ++j)
            fprintf(f, "%d ", message_queue[i]->rearrangement[j]);
        fprintf(f, "\n");
        for (j = 0; j <= message_queue[i]->current_level; ++j)
            fprintf(f, "%d ", message_queue[i]->rearr_index[j]);
        fprintf(f, "\n");
        for (j = 0; j <= message_queue[i]->current_level; ++j)
            fprintf(f, "%d ", message_queue[i]->level_count[j]);
        fprintf(f, "\n");
    }

    fclose(f);
}

static volatile int get_timings = 0;

static void timings_requested(int s) {
    get_timings = 1;
}

static volatile int alarm_fired = 0;

static void handle_alarm(int s) {
    alarm_fired = 1;
}

static void handle_interrupt(int s) {
    printf(">>>>>>>>>>>>> Dispatcher: Received %d signal\n", s);
    print_timings(1);
}

void do_dispatcher(
    const int process_num,
    const char *dump_filename,
    const char *node_name,
    const struct timeval start
) {
    Message message, *msg;
    const int message_size = sizeof(Message) / sizeof(int);
    MPI_Status status;

    struct timeval t1, work_start, waiting_start;

    dispatcher_timings.idle_time = dispatcher_timings.enqueue_time = dispatcher_timings.run_time = 0;

    signal(SIGALRM, handle_alarm);
    signal(SIGINT, handle_interrupt);
    signal(SIGTERM, handle_interrupt);
    signal(SIGUSR1, timings_requested);

    printf("%s We have %d processes\n", node_name, process_num);

    init_message_queue();
    init_worker_statuses(process_num - 1);

    jobs = (job *) malloc(worker_count * sizeof(job));
    worker_stat_array = (worker_stat *) malloc(worker_count * sizeof(worker_stat));

    for (int i = 0; i < worker_count; ++i) {
        memset(&jobs[i], 0, sizeof(job));
        jobs[i].evaluation = 0;

        worker_stat_array[i].idle_time = 0;
        worker_stat_array[i].run_time = 0;
        worker_stat_array[i].network_time = 0;
        worker_stat_array[i].sum_iterations = 0;
        gettimeofday(&worker_stat_array[i].last_updated_at, NULL);
    }

    message.max_s = max_s = 0;

    if (!dump_filename[0]) {
#if INITIALIZATION == 0
        // Эталон
        msg = (Message *) malloc(sizeof(Message));
        msg->current_level = 0;
        msg->target_level = -1;
        msg->evaluation = n * n;
        msg->status = BUSY;
        msg->iterations = 0;
        msg->rearrangement[0] = 0;
        msg->rearr_index[0] = -1;
        msg->max_s = 0;
        enqueue_message(msg, start);
#else
        const int max_index = (n - 1) / 2;  // Максимальное значение индекса на каждом уровне
        int indices[INITIALIZATION];  // Массив для хранения текущих индексов
        int level = 0;  // Текущий уровень (0..INITIALIZATION-1)

        // Инициализация массива индексов
        for (int i = 0; i < INITIALIZATION; i++) {
            indices[i] = -1;  // Начинаем с -1, чтобы первый инкремент дал 0
        }

        // Итеративный перебор всех комбинаций (аналог вложенных циклов)
        while (level >= 0) {
            // Увеличиваем индекс на текущем уровне
            indices[level]++;

            // Если индекс превысил максимум, откатываемся на уровень выше
            if (indices[level] >= max_index) {
                indices[level] = -1;
                level--;
                continue;
            }

            // Если дошли до последнего уровня — создаём сообщение
            if (level == INITIALIZATION - 1) {
                msg = (Message *)malloc(sizeof(Message));
                msg->current_level = INITIALIZATION;
                msg->target_level = INITIALIZATION - 1;
                msg->evaluation = n * n;
                msg->status = BUSY;
                msg->iterations = 0;

                // Заполняем rearrangement (всегда 0) и rearr_index
                for (int j = 0; j <= INITIALIZATION; j++) {
                    msg->rearrangement[j] = 0;
                    msg->rearr_index[j] = (j < INITIALIZATION) ? indices[j] : -1;
                }

                msg->max_s = 0;
                enqueue_message(msg, start);
            }
            // Иначе переходим на следующий уровень
            else {
                level++;
            }
        }
        // for (int i = (n - 1) / 2; i--;) {
        //     msg = (Message *) malloc(sizeof(Message));
        //     msg->current_level = 1;
        //     msg->target_level = 0;
        //     msg->evaluation = n * n;
        //     msg->status = BUSY;
        //     msg->iterations = 0;
        //     msg->rearrangement[0] = 0;
        //     msg->rearr_index[0] = i;
        //     msg->rearrangement[1] = 0;
        //     msg->rearr_index[1] = -1;
        //     msg->max_s = 0;
        //     enqueue_message(msg, start);
        // }
#endif
    } else {
        load_queue(dump_filename, start);
    }

    int worker_id;
    while ((worker_id = get_worker_id_with_status(FINISHED)) != -1) {
        if ((msg = dequeue_message())) {
            trace("%s Sending a piece of work to the Worker %d, pop from stack\n", node_name, worker_id);
            send_message_to_worker(worker_id, msg);
            free(msg);
            set_worker_state(worker_id, BUSY);
        } else {
            break;
        }
    }

    alarm(TDUMP);

    // Main loop
    gettimeofday(&work_start, NULL);
    for (;;) {
        if (get_timings) {
            get_timings = 0;
            print_timings(0);
            dump_queue();
        }

        if (alarm_fired) {
            alarm_fired = 0;
            alarm(TDUMP);
            //            dump_queue();
        }

        trace("%s Waiting for a message\n", node_name);
        memset(&message, 0, sizeof(Message));

        gettimeofday(&waiting_start, NULL);
        dispatcher_timings.run_time += time_diff_us(waiting_start, work_start); // От начала работы до начала ожидания

        MPI_Recv(&message, message_size, MPI_INT, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
        const int sender_id = status.MPI_SOURCE;

        gettimeofday(&work_start, NULL);
        dispatcher_timings.idle_time += time_diff_us(work_start, waiting_start);

        message.max_s = max_s = max(message.max_s, max_s);
        copy_timings_from_message(&worker_stat_array[sender_id - 1], &message);
        switch (message.status) {
            case FORKED:
                trace(
                    "%s Have a message from Worker %d, current_level = %d, eval = %f\n",
                    node_name,
                    sender_id,
                    message.current_level,
                    message.evaluation
                );

                // Обновляем информацию о работе текущего процесса (он берет себе задачу поменьше)
                copy_generators_from_message(&jobs[sender_id - 1], &message);
#if FORK_JOB == FORK_NEXT
                // Отдаем будущую работу диспетчеру, а сами добьем с текущего места
                jobs[sender_id - 1].current_level = message.remaining_level;
                jobs[sender_id - 1].target_level = message.current_level;
#elif FORK_JOB == FORK_CURRENT
                // Отдаем диспетчеру текущую работу, а сами прыгамем к будущей
                jobs[sender_id - 1].current_level = message.target_level;
#elif FORK_JOB == FORK_NONE
                // Ничего не делаем, потому что сразу же получим сообщение FINISHED
#endif
                worker_stat_array[sender_id - 1].sum_iterations += message.iterations - jobs[sender_id - 1].iterations;
                jobs[sender_id - 1].iterations = message.iterations;
                int worker_id = get_worker_id_with_status(FINISHED);
                if (worker_id != -1) {
                    set_worker_state(worker_id, BUSY);
                    message.status = BUSY;
                    trace("%s Sending a piece of work to the Worker %d\n", node_name, worker_id);
                    message.d_search_time = time_after_us(worker_stat_array[worker_id - 1].last_updated_at);
                    send_message_to_worker(worker_id, &message);
                } else {
                    trace("%s Push to queue\n", node_name);
                    gettimeofday(&t1, NULL);
                    msg = (Message *) malloc(sizeof(Message));
                    if (msg == NULL) {
                        printf("Panic! Not enough memory!\n");
                    }
                    memcpy(msg, &message, sizeof(Message));
                    enqueue_message(msg, start);
                    dispatcher_timings.enqueue_time += time_after_us(t1);
                }
                break;
            case FINISHED:
                trace("%s Have 'finished' message from Worker %d\n", node_name, sender_id);
                worker_stat_array[sender_id - 1].sum_iterations += message.iterations - jobs[sender_id - 1].iterations;
                if ((msg = dequeue_message())) {
                    trace("%s Sending a piece of work to the Worker %d, pop from queue\n", node_name, sender_id);
                    // Отправляем процессу новое задание в ответ на его же сообщение, которое пришло в момент work_start
                    msg->d_search_time = time_after_us(work_start);
                    send_message_to_worker(sender_id, msg);
                    free(msg);
                } else {
                    set_worker_state(sender_id, FINISHED);
                    if (get_worker_id_with_status(BUSY) == -1) {
                        // All workers finished, kill them
                        message.status = QUIT;
                        for (worker_id = 1; worker_id < process_num; ++worker_id) {
                            MPI_Send(&message, message_size, MPI_INT, worker_id, TAG, MPI_COMM_WORLD);
                        }
                        print_timings(1);
                        return;
                    }
                }
                break;
            case HALT:
                printf("%s Halt message received\n", node_name);
                print_timings(1);
                MPI_Abort(MPI_COMM_WORLD, 0);
                return;
            default:
                printf("%s Status error\n", node_name);
        }
    }
}

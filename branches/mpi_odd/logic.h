#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>

#define TQ 1800 // Квант времени воркеров в микросекундах
// #define TQ 700000 // Квант времени воркеров в микросекундах
#define TDUMP 20 // Время срабатывания будильника в диспетчере для автодампа состояния

/**
 * Определяем, как сортировать задачи в очереди
 */
#define SORTING_ORDER 1 // в естественном порядке, определяемом направлением обхода
#define SORTING_EVALUATION_LOG_LINEAR 2 // на основе оценки с линейной интерполяцией
#define SORTING_EVALUATION_LOG_SQUARE 3 // на основе оценки с квадратичной интерполяцией
#define SORTING_EVALUATION_LOG_AVG 4 // на основе оценки с усреднением
#define SORTING_EVALUATION_AVG 5 // на основе оценки с усреднением без логарифмирования (похоже на максимум)
#define SORTING_ITERATIONS 6 // равномерное распределение итераций, типа раунд робин
#define SORTING_ORDER_REVERSE 7 // дурацкая сортировка
#define GENERATOR_SORTING SORTING_ORDER

/**
 * Определяем, сколько задач изначально будет создано.
 * 0 - одно сообщение для корня
 * 1 - (n-1)/2 сообщений для первого уровня
 * 2 - ((n-1)/2) ^ 2 сообщений для второго уровня
 * ...
 * В общем случае ((n-1)/2) ^ INITIALIZATION
 */
#define INITIALIZATION 0

/**
 * Определяем направление обхода дерева - "эвристику".
 * Идея: попробовать сделать так, чтобы в каждой задаче
 * был свой мультиплет, например, сгенерированный случайно.
 * Тогда при сортировке по итерациям (раунд-робин) можно
 * было бы определять, какой мультиплет лучше.
 */
#define MULTIPLET_DECR 1 // x-2, n-3, n-5, ... x+2
#define MULTIPLET_INCR 2 // x-2, x+2, x+4, ... n-3
#define MULTIPLET MULTIPLET_DECR

/**
 * Определяем, что происходит с задачей при срабатывании
 * таймера в воркере.
 */
// Отдаем будущую работу диспетчеру, а сами добьем с текущего места (старая логика).
// Проблема в том, что текущую работу сложно оценить (в режиме сортировки evaluation).
#define FORK_NEXT 1
// Отдаем текущую работу диспетчеру, а сами прыгаем вперед (новая логика).
// Текущую отданную работу легко оценить, но воркер может быть долго занят прыжками и форками.
// Также существенно растет объем очереди.
#define FORK_CURRENT 2
// Задача возвращается диспетчеру без дробления, воркер освобождается.
// Есть смысл использовать при ненулевом INITIALIZATION
#define FORK_NONE 3
#define FORK_JOB FORK_NEXT

// #define trace(format, ...) printf (format, __VA_ARGS__)
#define trace(format, ...)

#define n_limit 59
#define n_step_limit (n_limit*(n_limit-1) / 2)

#define plurality ((n_limit-1)/2) // Количество узлов дерева на каждом уровне
#define rearrangement_count 1 // Количество вариантов перестановок для обхода этих узлов

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

extern int n, n_step;

extern int triplets[rearrangement_count][plurality]; //= {{-2, n-3, n-5, ... 2}};

typedef uint_fast32_t line_num;

typedef struct {
    line_num generator;
    unsigned long int processed; // количество проходов по уровню с момента предыдущего срабатывания будильника
    int rearrangement;
    int rearr_index;
    int level_count;
} Stat;

extern Stat stat[n_step_limit + 1];

extern line_num a[n_limit]; // Текущая перестановка

#endif // LOGIC_H

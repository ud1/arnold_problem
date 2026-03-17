#ifndef MESSAGING_H
#define MESSAGING_H
#include "logic.h"

#define TAG 0 // Для MPI

enum { BUSY, FORKED, FINISHED, QUIT, HALT };

typedef struct {
    int current_level; // Текущий уровень воркера
    int target_level;
    // Уровень, до которого нужно подняться в этом джобе. Этот уровень ближе к корню, чем текущий уровень.
    int remaining_level; // Текущий уровень в урезанном джобе, оставшемся в воркере, в котором случился форк.
    double evaluation;
    int status;
    int rearrangement[n_step_limit];
    int rearr_index[n_step_limit];
    int level_count[n_step_limit];
    int max_s;
    unsigned long int iterations; // Количество предшествующих итераций главного цикла

    unsigned long int d_search_time;
    unsigned long int w_run_time;
    unsigned long int w_idle_time;
    unsigned long int network_time;
} Message;

#endif // MESSAGING_H

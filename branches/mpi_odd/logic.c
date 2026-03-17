#include "logic.h"

int n, n_step;

int triplets[rearrangement_count][plurality]; //= {{-2, n-3, n-5, ... 2}};

Stat stat[n_step_limit + 1];

line_num a[n_limit]; // Текущая перестановка

#include <stddef.h>
#include <sys/time.h>

unsigned long int time_diff_ms(const struct timeval t2, const struct timeval t1) {
    const unsigned long int run_time = (t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec) / 1000;

    return run_time;
}

unsigned long int time_after_ms(const struct timeval t1) {
    struct timeval t2;
    gettimeofday(&t2, NULL);

    return time_diff_ms(t2, t1);
}

unsigned long int time_diff_us(const struct timeval t2, const struct timeval t1) {
    const unsigned long int run_time = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);

    return run_time;
}

unsigned long int time_after_us(const struct timeval t1) {
    struct timeval t2;
    gettimeofday(&t2, NULL);

    return time_diff_us(t2, t1);
}

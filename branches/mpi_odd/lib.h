#ifndef LIB_H
#define LIB_H

unsigned long int time_diff_ms(struct timeval t2, struct timeval t1);
unsigned long int time_after_ms(struct timeval t1);
unsigned long int time_diff_us(struct timeval t2, struct timeval t1);
unsigned long int time_after_us(struct timeval t1);

#endif // LIB_H

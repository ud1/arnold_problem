#ifndef WORKER_H
#define WORKER_H

void do_worker(int id, const char *dump_filename, const char *output_filename, const char *node_name, struct timeval start, int full);

#endif // WORKER_H

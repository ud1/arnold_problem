#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include "logic.h"
#include "dispatcher.h"
#include "worker.h"

struct timeval start;

void show_help(char *filename) {
    printf(
        "Usage: %s -n N [-o filename] [-full] [-d dump_filename]\n"
        "-n             line count;\n"
        "-full          output all found generator sets;\n"
        "-d             dump file name\n"
        "-o             output file.\n", filename);
}

int main(int argc, char **argv) {
    char node_name[64];
    char output_filename[80] = "";
    char s[80], dump_filename[80];
    int full = 0;
    int process_num;
    int myid;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &process_num);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    if (argc < 2) {
        if (myid == 0) {
            show_help(argv[0]);
        }
        MPI_Finalize();
        return 0;
    }

    dump_filename[0] = '\0';

    for (int i = 1; i < argc; i++) {
        strcpy(s, argv[i]);
        if (!strcmp("-n", s))
            sscanf(argv[++i], "%d", &n); // NOLINT(*-err34-c)
        else if (!strcmp("-o", s))
            strcpy(output_filename, argv[++i]);
        else if (!strcmp("-full", s))
            full = 1;
        else if (!strcmp("-d", s))
            strcpy(dump_filename, argv[++i]);
        else {
            show_help(argv[0]);

            MPI_Finalize();
            return 0;
        }
    }

    if (n % 2 == 0) {
        printf("This program is designed for fast search of odd defectless configurations only.\n");
        MPI_Finalize();
        return 0;
    }

    if (n > n_limit) {
        printf("This program is compiled to support up to %d lines, %d given.\n", n_limit, n);
        MPI_Finalize();
        return -1;
    }

    gettimeofday(&start, NULL);

    if (myid == 0) {
        strcpy(node_name, "Dispatcher:");
        do_dispatcher(process_num, dump_filename, node_name, start);
    } else {
        sprintf(node_name, "Worker %d:", myid);
        do_worker(myid, dump_filename, output_filename, node_name, start, full);
    }

    struct timeval end;
    gettimeofday(&end, NULL);

    const double time_taken = (double) end.tv_sec + (double) end.tv_usec / 1e6 -
                              (double) start.tv_sec - (double) start.tv_usec / 1e6; // in seconds

    printf("---------->>> [%fs] %s terminated.\n", time_taken, node_name);
    MPI_Finalize();

    return 0;
}

#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define n1 41
#define n_step1 (n1*(n1-1) / 2)

#define plurality 3
#define rearrangement_count 3

#define TQ 5
#define TAG 0

int n, n_step;

int triplets[rearrangement_count][n1]; //= {{1, 2, -1}, {2, -1, 1}, {-1, 2, 1}};

char NODE[64];

typedef struct {
	int defects, generator, stack, processed;
	int rearrangement;
	int rearr_index;
} Stats;

Stats stats[n_step1 + 1];

int a[n1], d[n1 + 1];

int max_s[n1/2 + 1];

int b_free;
int max_defects;

// User input
char filename[80] = "";
int max_defects_default = 0;
int full = 0;
int show_stat = 0;

// MPI
int was_alarm = 0;

enum {BUSY, FORKED, FINISHED, QUIT};

typedef struct {
	int level;
	int min_level;
	int status;
	int rearrangement[n_step1];
	int rearr_index[n_step1];
} Message;

// Message stack
Message **messages = NULL;
int msg_reserve, msg_count;

void msg_init() {
	msg_reserve = 1000;
	msg_count = 0;
	messages = (Message**) malloc(msg_reserve*sizeof(Message*));
}

void push_msg_back(Message *msg) {
	printf("Dispatcher: Stack size is %d\n", msg_count);
	if (++msg_count > msg_reserve)
		messages = (Message**) realloc(messages, (msg_reserve*=2)*sizeof(Message*));
	messages[msg_count-1] = msg;
	printf("Dispatcher: Stack size is %d\n", msg_count);
}

Message *pop_msg() {
	if (msg_count)
		return messages[--msg_count];
	return NULL;
}

// Workers state
int* wrk_state;
int wrk_count;

void wrk_init(int cnt) {
	int i;
	wrk_count = cnt;
	wrk_state = (int *) malloc(wrk_count*sizeof(int));
	for (i = 0; i < wrk_count; ++i) {
		wrk_state[i] = FINISHED;
	}
}

int get_worker(int status) {
	int i;
	for (i = 0; i < wrk_count; ++i) {
		if (wrk_state[i] == status) {
			return i+1;
		}
	}
	return -1;
}

int set_wrk_state(int i, int state) {
	wrk_state[i-1] = state;
}

void tmr (int s) {
//	if (!worker_is_busy)
//	    return;
//	alarm(TQ);
	was_alarm = 1;
}

void find_parallel_config (int k, int level) {
	int s, i, max_defects1;
	FILE *f;

	for (i = 0; i < n; i++)
		if (a[i] + i < n-2 || a[i] + i > n)
			return;

	s = - 1 + (n & 1);
	for (i = 1; i <= level + 1; i++) {
		s -= (stats[i].generator & 1)*2 - 1;
	}
	if (s < 0)
		s = -s;
	if (s < max_s[k] || !full && s == max_s[k])
		return;

	max_s[k] = s;
	max_defects1 = ((n*(n+1)/2 - k - 3) - 3*max_s[k])/2;

	if (strcmp(filename, "") == 0) {
		printf("%s A(%d, %d) = %d; %d %d)", NODE, n, k, s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			printf(" %d", stats[i].generator);
		}

		printf("\n");
		if (show_stat)
			for (i = 0; i < n/2 + 1; i++ )
				printf("%s A(%d, %d) = %d;\n", NODE, n, i, max_s[i]);

		fflush(NULL);
		fflush(NULL);
		fflush(stdout);
		fflush(stdout);
	}
	else {
		f = fopen(filename, "a");

		fprintf(f, "A(%d, %d) = %d; %d %d)", n, k, s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			fprintf(f, " %d", stats[i].generator);
		}

		fprintf(f, "\n");
		if (show_stat)
			for (i = 0; i < n/2 + 1; i++ )
				fprintf(f, "A(%d, %d) = %d;\n", n, i, max_s[i]);
		fclose(f);
	}
}

//inline 
void set (int curr_generator, int t) {
	int i;

	b_free += -2*t+1;
	i = a[curr_generator];
	a[curr_generator] = a[curr_generator + 1];
	a[curr_generator + 1] = i;
}

void run(int level, int min_level) {
	int curr_generator, direct, i;
	Message message;

	for (i = n/2; i--; )
		max_s[i] = 0;
	max_defects = max_defects_default;

	for (i = n + 1; i--; ) {
		d[i] = 2;
	}

	// Initializing
	for (i = 0; i <= n_step; ++i) {
		stats[i].processed = 0;
	}

	for (i=0; i<level; ++i) {
		stats[i + 1].generator = curr_generator = stats[i].generator + triplets[stats[i].rearrangement][stats[i].rearr_index];
		printf("{%d %d}", i+1, curr_generator);
		printf("\n");
		stats[i + 1].defects = stats[i].defects;
		stats[i + 1].processed++;
		if (!(curr_generator % 2)/* && (b_free*2 > n)*/) { // even, white
			if (d[curr_generator] == 1) {
				stats[i + 1].defects++;
			}
			if (d[curr_generator + 2] == 1) {
				stats[i + 1].defects++;
			}
		}
		stats[i+1].stack = d[curr_generator + 1];
		d[curr_generator + 1] = 0;
		d[curr_generator]++;
		d[curr_generator + 2]++;
		set(curr_generator, 1);
		
	}
	
	was_alarm = 0;

	// Run
	while (1) {
		if (stats[level].rearr_index < plurality - 1) {
			stats[level].rearr_index++;
			curr_generator = stats[level].generator + (direct = triplets[stats[level].rearrangement][stats[level].rearr_index]);
			if (curr_generator > n-2 || curr_generator < 0)
				continue;
			if (a[curr_generator] > a[curr_generator + 1])
				continue;
			stats[level + 1].generator = curr_generator;
			stats[level + 1].processed++;
			if (b_free <= n/2) {
				//				printf("%d\n", b_free);
				set(curr_generator, 1);
				find_parallel_config(b_free + 1, level);
				set(curr_generator, 0);
			}
			if (!b_free) {
				//				count_gen(level);
				continue;
			}
			stats[level + 1].defects = stats[level].defects;
			if (!(curr_generator % 2)/* && (b_free*2 > n)*/) { // even, white
				if (d[curr_generator + 1] < 3) // '<3' for 0 defects, '==1' for full search
					continue;
				if (d[curr_generator] == 1) {
					stats[level + 1].defects++;
				}
				if (d[curr_generator + 2] == 1) {
					stats[level + 1].defects++;
				}
				if (stats[level + 1].defects > max_defects) {
					continue;
				}
			}
			stats[level+1].stack = d[curr_generator + 1];
			d[curr_generator + 1] = 0;
			d[curr_generator]++;
			d[curr_generator + 2]++;
			set(curr_generator, 1);
			level++;
			//			stats[level].rearrangement = direct > 0 ? 1 : 2;
			if (direct > 0) {
				if (curr_generator % 2)
					stats[level].rearrangement = 1;
				else
					stats[level].rearrangement = 0;
			}
			else
				stats[level].rearrangement = 2;
			stats[level].rearr_index = -1;

			if (was_alarm) {
				printf("%s Alarm\n", NODE);
				was_alarm = 0;
				alarm(TQ);
				for (i = min_level; i < n_step; ++i) {\
					printf("<%d %d>", i, stats[i + 1].processed);
					if (stats[i+1].processed > 1)
						break;
				}
				printf("\n");
				message.level = i-1;
				message.min_level = min_level;
				min_level = i;
				message.status = FORKED;
				printf("%s message.level = %d\n", NODE, message.level);
				for (i = 0; i <= message.level; ++i) {
					message.rearr_index[i] = stats[i].rearr_index;
					message.rearrangement[i] = stats[i].rearrangement;
					printf ("%d[%d] ", stats[i].rearrangement, stats[i].rearr_index);
				}
				for (i = 0; i < message.level; ++i)
				    printf("{%d %d}", i+1, stats[i+1].generator);
				printf("\n");
				MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD);

				for (i = 0; i <= n_step; ++i) {
					stats[i].processed = 1;
				}
			}
		} else {
			curr_generator = stats[level].generator;
			level--;
			set(curr_generator, 0);
			d[curr_generator]--;
			d[curr_generator + 2]--;
			d[curr_generator + 1] = stats[level+1].stack;
			if (level == min_level)
				break;
		}
	}
}

void do_dispatcher(int numprocs) {
	Message message, *msg;
	MPI_Status status;
	int worker, i;

	printf("%s We have %d processors\n", NODE, numprocs);

	// Message stack initializing
	msg_init();

	// Workers state initializing
	wrk_init(numprocs-1);

	// Sending first peace of work (root) to the first worker
	message.level = 0;
	message.min_level = 0;
	message.status = BUSY;
	message.rearrangement[0] = 0;
	message.rearr_index[0] = -1;
	set_wrk_state(1, BUSY);
	MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 1, TAG, MPI_COMM_WORLD);

	// Main loop
	for (;;) {
		printf("%s Waiting for a message\n", NODE);
		memset((void *)&message, 0, sizeof(Message));
		MPI_Recv((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
		
		for (i=0; i <= message.level; ++i)
			printf("%d(%d) ", message.rearrangement[i], message.rearr_index[i]);
		printf("\n");
		switch (message.status) {
			case FORKED:
				printf("%s Have message, level = %d\n", NODE, message.level);
				worker = get_worker(FINISHED);
				if (worker != -1) {
					set_wrk_state(worker, BUSY);
					message.status = BUSY;
					printf("%s Sending peace of work to node %d\n", NODE, worker);
					MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, worker, TAG, MPI_COMM_WORLD);
				} else {
					printf("%s Push to stack\n", NODE);
					msg = (Message *) malloc(sizeof(Message));
					if (msg == NULL) {
					    printf ("Panic! Not enough memory!");
					}
					memcpy((void *) msg, (void *) &message, sizeof(Message));
					push_msg_back(msg);
				}
				break;
			case FINISHED:
				printf("%s Have finished message from %d\n", NODE, status.MPI_SOURCE);
				if (msg = pop_msg()) {
					printf("%s Sending peace of work to node %d, pop from stack\n", NODE, status.MPI_SOURCE);
					MPI_Send((void *)msg, sizeof(Message)/sizeof(int), MPI_INT, status.MPI_SOURCE, TAG, MPI_COMM_WORLD);
					free(msg);
				} else {
					set_wrk_state(status.MPI_SOURCE, FINISHED);
					worker = get_worker(BUSY);
					if (worker == -1) {
						// All workers finished, kill them
						message.status = QUIT;
						for (worker = 1; worker < numprocs; ++worker)
							MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, worker, TAG, MPI_COMM_WORLD);
						return;
					}
				}
				break;
			default:
				printf("%s Status error\n", NODE);
		}
	}
}

void do_worker(int id) {
	Message message;
	MPI_Status status;
	int i;

	// Timer initialization
	signal(SIGALRM, tmr);
	//alarm(TQ);

	for (i = 3; i < n1; i++ ) {
		triplets[0][i] = triplets[1][i] = triplets[2][i] = i;
	}

	triplets[0][0] = 1;
	triplets[0][1] = 2;
	triplets[0][2] = -1;
	triplets[1][0] = 2;
	triplets[1][1] = -1;
	triplets[1][2] = 1;
	triplets[2][0] = -1;
	triplets[2][1] = 2;
	triplets[2][2] = 1;

	stats[0].generator = 0;
	stats[0].defects = 0;

	for(;;) {
		// receive from dispatcher:
		printf("%s Waiting for a message from dispatcher\n", NODE);
		MPI_Recv((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD, &status);

		if (message.status == QUIT) {
			printf("%s Received quit message\n", NODE);
			break;
		}

		printf("%s Received message from dispatcher\n", NODE);
		
		//worker_is_busy = 1;
		alarm(TQ);

		for (i = 0; i <= message.level; ++i) {
			stats[i].rearrangement = message.rearrangement[i];
			stats[i].rearr_index = message.rearr_index[i];
		}

		printf("%s Run, level = %d, minlevel = %d\n", NODE, message.level, message.min_level);
		b_free = (max_defects = n_step = n*(n-1) / 2) - 1;
		for (i = 0; i < n; i++) {
			a[i] = i;
		}
		run(message.level, message.min_level);
		//stats[0].rearrangement = 0;
		//stats[0].rearr_index = -1;
		//run(0, 0);

		message.status = FINISHED;
		//worker_is_busy = 0;
		printf("%s Finished\n", NODE);
		MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD);
	}
}

int main(int argc, char **argv) {
	int i;
	char s[80];
	char idstr[32];
	int numprocs;
	int myid;
	MPI_Status mpi_stat; 

	if (argc < 2) {
		printf(
			"Usage: %s -n N [-max-def D] [-o filename] [-full] [-stats]\n"
			"-n             line count;\n"
			"-max-def       the number of allowed defects;\n"
			"-full          output all found generator sets;\n"
			"-stats         show stats after every generator set;\n"
			"-o             output file.\n", argv[0]);
		return 0;
	}

	for (i = 1; i < argc; i++) {
		strcpy(s, argv[i]);
		if (!strcmp("-n", s))
			sscanf(argv[++i], "%d", &n);
		else if (!strcmp("-max-def", s))
			sscanf(argv[++i], "%d", &max_defects_default);
		else if (!strcmp("-o", s))
			strcpy(filename, argv[++i]);
		else if (!strcmp("-full", s))
			full = 1;
		else if (!strcmp("-stats", s))
			show_stat = 1;
		else {		
			printf(
				"Usage: %s -n N [-max-def D] [-o filename] [-full] [-stats]\n"
				"-n             line count;\n"
				"-max-def       the number of allowed defects;\n"
				"-full          output all found generator sets;\n"
				"-stats         show stats after every generator set;\n"
				"-o             output file.\n", argv[0]);
			return 0;
		}
	}

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD,&myid);

	if(myid == 0) {
		strcpy(NODE, "Dispatcher:");
		do_dispatcher(numprocs);
	} else {
		sprintf(NODE, "Worker %d:", myid);
		do_worker(myid);
	}

	printf("---------->>>%s terminated.\n", NODE);
	MPI_Finalize();
	return 0;
}
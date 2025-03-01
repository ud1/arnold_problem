#include <sys/time.h> 
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#define trace(format, ...) printf (format, __VA_ARGS__) 
// #define trace(format, ...) 

#define n1 43
#define n_step1 (n1*(n1-1) / 2)

#define plurality 3
#define rearrangement_count 3

#define TQ 5
#define TDUMP 600
#define TAG 0

int n, k = 0, n_step;

int triplets[rearrangement_count][plurality]; //= {{1, 2, -1}, {2, -1, 1}, {-1, 2, 1}};

typedef struct {
	int defects, generator, stack, processed;
	int rearrangement;
	int rearr_index;
} Stats;

Stats stats[n_step1 + 1];

int a[n1], d[n1 + 1];

int max_s;

int b_free;
int max_defects;

// User input
char filename[80] = "";
int max_defects_default = 0;
int full = 0;
int show_stat = 0;

// MPI
char NODE_NAME[64];
int was_alarm = 0;
int get_timings = 0; 

enum {BUSY, FORKED, FINISHED, QUIT};

typedef struct {
	int level;
	int min_level;
	int w_level;
	int status;
	int rearrangement[n_step1];
	int rearr_index[n_step1];
	int max_s;

	int d_search_time; 
	int w_run_time; 
	int w_idle_time; 
	int network_time; 
} Message;

// Message stack
Message **messages = NULL;
int msg_reserve, msg_count;

struct { 
	// Worker's timings 
	int w_run_time; 
	int w_idle_time; 

	// Dispatcher 
	int d_push_msg_time, push_count; 
	int d_run_time; 
	int d_idle_time; 

	// Network's timings 
	int network_time; 
} timings; 

void msg_init() {
	msg_reserve = 1000;
	msg_count = 0;
	timings.push_count = 0;
	messages = (Message**) malloc(msg_reserve*sizeof(Message*));
}

int count_compare;

int msg_compare (const void * a, const void * b) { 
	Message *ma, *mb;
	int *p1, *p2, *endp;

	ma = * (Message **) a;
	mb = * (Message **) b;

	p1 = ma->rearr_index;
	p2 = mb->rearr_index;
	endp = p1 + min(ma->level, mb->level); 

	for (; !(*p2 - *p1) && p1 <= endp; ++p1, ++p2); 

	count_compare++;

	return *p2 - *p1; 
} 

void msg_sort() {
	qsort(messages, msg_count, sizeof(Message *), msg_compare);
}

void push_msg_back(Message *msg) {
	++timings.push_count;
	if (++msg_count > msg_reserve)
		messages = (Message**) realloc(messages, (msg_reserve*=2)*sizeof(Message*));
	messages[msg_count - 1] = msg;
	count_compare = 0;
	msg_sort();
	printf("------->  %d in stack [%d times compared while sorting]  <-------\n", msg_count, count_compare);
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
	wrk_state = (int *) malloc(wrk_count * sizeof(int));
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

void send_timings_signal (int s) { 
	get_timings = 1; 
}

void tmr (int s) {
	was_alarm = 1;
}

void send_message_to_dispatcher(Message *msg) { 
	struct timeval t1, t2; 
	int send_time; 

	msg->w_idle_time = timings.w_idle_time; 
	msg->w_run_time = timings.w_run_time; 
	msg->network_time = timings.network_time; 

	gettimeofday(&t1, NULL); 
	MPI_Send((void *)msg, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD); 
	gettimeofday(&t2, NULL); 

	send_time =  (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec - t1.tv_usec)/1000; 
	timings.w_idle_time += send_time; 
	timings.network_time += send_time; 
} 

void count_gen (int level) {
	int s, i, max_defects1;
	FILE *f;

	s = - 1 + (n & 1);
	for (i = 1; i <= level + 1; i++) {
		s -= (stats[i].generator & 1)*2 - 1;
	}
	if (s < 0)
		s = -s;
	if (s < max_s || !full && s == max_s)
		return;

	max_s = s;
	max_defects1 = ((n*(n+1)/2 - k - 3) - 3*max_s)/2;

	if (strcmp(filename, "") == 0) {
		printf("%s A(%d, %d) = %d; %d %d)", NODE_NAME, n, k, s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			printf(" %d", stats[i].generator);
		}

		printf("\n");

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
		fclose(f);
	}
}

inline void set (int curr_generator, int t) {
	int i;

	b_free += -2*t+1;
	i = a[curr_generator];
	a[curr_generator] = a[curr_generator + 1];
	a[curr_generator + 1] = i;
}

void run (int level, int min_level) {
	int curr_generator, direct, i;
	Message message;

	max_defects = max_defects_default;

	for (i = n + 1; i--; ) {
		d[i] = 2;
	}

	// Initializing
	for (i = 0; i <= n_step; ++i) {
		stats[i].processed = 0;
	}

	for (i = 0; i < level; ++i) {
		stats[i + 1].generator = curr_generator = stats[i].generator + triplets[stats[i].rearrangement][stats[i].rearr_index];
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

			if (!b_free) {
				count_gen(level);
				continue;
			}
			stats[level + 1].defects = stats[level].defects;
			if (!(curr_generator % 2)/* && (b_free*2 > n)*/) { // even, white
				if (d[curr_generator] == 1) {
					stats[level + 1].defects++;
				}
				if (d[curr_generator + 2] == 1) {
					stats[level + 1].defects++;
				}
				if (stats[level + 1].defects > max_defects) {
					continue;
				}
				if (d[curr_generator + 1] < 3) // '<3' for 0 defects, '==1' for full search
					continue;

			}
			stats[level + 1].stack = d[curr_generator + 1];
			d[curr_generator + 1] = 0;
			d[curr_generator]++;
			d[curr_generator + 2]++;
			set(curr_generator, 1);
			level++;
			stats[level].rearrangement = direct > 0 ? 1 : 2;
			stats[level].rearr_index = -1;

			if (was_alarm) {
				trace("%s Alarm\n", NODE_NAME);

				was_alarm = 0;
				alarm(TQ);
				for (i = min_level; i < n_step; ++i) {
					if (stats[i + 1].processed > 1)
						break;
				}

				message.min_level = min_level; 
				message.level = i; 
				min_level = i; 
				min_level = message.level = i; 
				message.w_level = level;
				message.status = FORKED;

				trace("%s message.level = %d\n", NODE_NAME, message.level);
				for (i = 0; i <= message.w_level; ++i) {
					message.rearr_index[i] = stats[i].rearr_index;
					message.rearrangement[i] = stats[i].rearrangement;
				}
				message.max_s = max_s;

				send_message_to_dispatcher(&message);

				for (i = 0; i <= n_step; ++i)
					stats[i].processed = 1;
			}
		}
		else {
			curr_generator = stats[level].generator;
			level--;
			set(curr_generator, 0);
			d[curr_generator]--;
			d[curr_generator + 2]--;
			d[curr_generator + 1] = stats[level + 1].stack;
			if (level == min_level)
				break;
		}
	}
}

typedef struct { 
	int rearrangement[n_step1]; 
	int rearr_index[n_step1]; 
	int min_level, level; 
	int w_idle_time, w_run_time, network_time; 
	struct timeval t; 
} worker_info; 

worker_info *workers_info; 

void print_timings() { 
	FILE *f; 
	char filename[64]; 
	sprintf(filename, "%d_timings.txt", n); 
	f = fopen(filename, "a"); 
	fprintf(f, 
		"w_run_time = %dms\n" 
		"w_idle_time = %dms\n" 
		"d_push_msg_time = %du, count = %d, avg = %du\n" 
		"d_run_time = %dms\n" 
		"d_idle_time = %dms\n" 
		"network_time = %dms\n", 
		timings.w_run_time, 
		timings.w_idle_time, 
		timings.d_push_msg_time, timings.push_count, timings.d_push_msg_time/timings.push_count, 
		timings.d_run_time, 
		timings.d_idle_time, 
		timings.network_time); 
	fclose(f); 
} 

void copy_timings_from_message(worker_info *inf, const Message *msg) { 
	inf->w_idle_time = msg->w_idle_time; 
	inf->w_run_time = msg->w_run_time; 
	inf->network_time = msg->network_time; 
	gettimeofday(&inf->t, NULL); 
} 

void copy_gens_from_message(worker_info *inf, const Message *msg) { 
	memcpy(inf->rearrangement, msg->rearrangement, sizeof(msg->rearrangement)); 
	memcpy(inf->rearr_index, msg->rearr_index, sizeof(msg->rearr_index)); 
} 

void dump_queue() { 
	static int v = 1, i, j; 
	char filename[256]; 
	FILE *f; 
	v = !v; 

	sprintf(filename, "%d_%d_dump_gens_%d", n, k, v);
	f = fopen(filename, "w+"); 
	fprintf(f, "// n, k\n%d %d\n", n, k);
	fprintf(f, "// max_s\n"); 
	fprintf(f, "%d\n", max_s); 

	fprintf(f, "// min_lev, lev, rearrangement, rearr_index ...\n"); 
	for (i = 0; i < wrk_count; ++i) { 
		if (wrk_state[i] == BUSY) { 
			fprintf(f, "%d %d\n", workers_info[i].min_level, workers_info[i].level); 
			for (j = 0; j <= workers_info[i].level; ++j) 
				fprintf(f, "%d ", workers_info[i].rearrangement[j]); 
			fprintf(f, "\n"); 
			for (j = 0; j <= workers_info[i].level; ++j) 
				fprintf(f, "%d ", workers_info[i].rearr_index[j]); 
			fprintf(f, "\n"); 
		} 
	} 
	for (i = 0; i < msg_count; ++i) { 
		fprintf(f, "%d %d\n", messages[i]->min_level, messages[i]->level); 
		for (j = 0; j <= messages[i]->level; ++j) 
			fprintf(f, "%d ", messages[i]->rearrangement[j]); 
		fprintf(f, "\n"); 
		for (j = 0; j <= messages[i]->level; ++j) 
			fprintf(f, "%d ", messages[i]->rearr_index[j]); 
		fprintf(f, "\n"); 
	} 

	fclose(f); 
} 

void load_queue(const char *filename, int is_it_dispatcher) { 
	FILE *f; 
	int l, i, res; 
	Message *msg; 
	char *str = (char *) malloc(65536), *ptr; 
	l = 0; 

	f = fopen(filename, "r"); 
	if (!f) { 
		printf("File %s not found, exit\n", filename); 
		exit(0); 
	} 
	while(res = fscanf(f, "%[^\n]\n", str), res != -1 && res != 0) { 
		if (str[0] == '/') 
			continue; 
		switch (l) { 
		case 0: 
			ptr = str;
			n = strtol(ptr, &ptr, 10); 
			k = strtol(ptr, &ptr, 10); 
			if (!is_it_dispatcher) 
				return; 
			l = 1; 
			break; 
		case 1: 
			ptr = str; 
			max_s = strtol(ptr, &ptr, 10); 
			l = 2; 
			break; 
		case 2: 
			msg = (Message *) malloc(sizeof(Message)); 
			sscanf(str, "%d %d", &msg->min_level, &msg->level); 
			l = 3; 
			break; 
		case 3: 
			ptr = str; 
			for (i = 0; i <= msg->level; i++ ) { 
				msg->rearrangement[i] = strtol(ptr, &ptr, 10); 
			} 
			l = 4; 
			break; 
		case 4: 
			ptr = str; 
			for (i = 0; i <= msg->level; i++ ) { 
				msg->rearr_index[i] = strtol(ptr, &ptr, 10); 
			} 
			msg->max_s = max_s;
			msg->status = BUSY; 
			push_msg_back(msg); 
			l = 2; 
			break; 
		} 
	} 
	fclose(f); 
	free(str); 
} 

void send_work(int node, Message *msg) { 
	MPI_Send((void *)msg, sizeof(Message)/sizeof(int), MPI_INT, node, TAG, MPI_COMM_WORLD); 

	copy_gens_from_message(&workers_info[node-1], msg); 
	workers_info[node-1].level = msg->level; 
	workers_info[node-1].min_level = msg->min_level; 
}

void do_dispatcher(int numprocs, const char *dump_filename) {
	Message message, *msg;
	MPI_Status status;
	int worker, i;

	struct timeval t1, t2, t3, t4; 
	int push_time, idle_time, run_time; 
	int timeings_results; 

	timings.d_idle_time = timings.d_push_msg_time = timings.d_run_time = 0; 
	signal(SIGALRM, tmr); 
	signal(SIGUSR1, send_timings_signal); 

	printf("%s We have %d processes\n", NODE_NAME, numprocs);

	// Message stack initializing
	msg_init();

	// Workers state initializing
	wrk_init(numprocs-1);

	workers_info = (worker_info *) malloc((wrk_count)*sizeof(worker_info)); 

	for (i = 0; i < wrk_count; ++i) { 
		memset((void *) &workers_info[i], 0, sizeof(worker_info)); 
		gettimeofday(&workers_info[i].t, NULL); 
	} 

	message.max_s = max_s = 0;

	if (!dump_filename[0]) { 
		// Sending first peace of work (root) to the first worker 
		message.level = 0; 
		message.min_level = 0; 
		message.status = BUSY; 
		message.rearrangement[0] = 0; 
		workers_info[0].rearr_index[0] = message.rearr_index[0] = -1; 
		set_wrk_state(1, BUSY); 
		MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 1, TAG, MPI_COMM_WORLD); 
	} else { 
		load_queue(dump_filename, 1); 
		while ((worker = get_worker(FINISHED)) != -1) { 
			if (msg = pop_msg()) { 
				trace("%s Sending a peace of work to the node %d, pop from stack\n", NODE_NAME, worker); 
				send_work(worker, msg); 
				free(msg); 
				set_wrk_state(worker, BUSY); 
			} else break; 
		} 
	} 

	alarm(TDUMP); 

	// Main loop
	gettimeofday(&t3, NULL);
	for (;;) {
		if (get_timings) { 
			get_timings = 0; 
			timings.w_run_time = timings.w_idle_time = timings.network_time = 0; 

			for (i = 0; i < wrk_count; ++i) { 
				timings.w_run_time += workers_info[i].w_run_time; 
				timings.w_idle_time += workers_info[i].w_idle_time; 
				timings.network_time += workers_info[i].network_time; 
			} 

			print_timings(); 
			dump_queue(); 
		} 

		if (was_alarm) { 
			was_alarm = 0; 
			alarm(TDUMP); 
			dump_queue(); 
		} 

		trace("%s Waiting for a message\n", NODE_NAME);
		memset((void *)&message, 0, sizeof(Message));
		gettimeofday(&t4, NULL); 
		run_time =  (t4.tv_sec - t3.tv_sec)*1000 + (t4.tv_usec - t3.tv_usec)/1000; 
		timings.d_run_time += run_time; 
		MPI_Recv((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
		gettimeofday(&t3, NULL); 

		idle_time =  (t3.tv_sec - t4.tv_sec)*1000 + (t3.tv_usec - t4.tv_usec)/1000; 
		timings.d_idle_time += idle_time; 
		message.max_s = max_s = max(message.max_s, max_s);
		copy_timings_from_message(&workers_info[status.MPI_SOURCE-1], &message); 
		switch (message.status) {
			case FORKED:
				trace("%s Have a message, level = %d\n", NODE_NAME, message.level); 

				copy_gens_from_message(&workers_info[status.MPI_SOURCE-1], &message); 
				workers_info[status.MPI_SOURCE-1].level = message.w_level; 
				workers_info[status.MPI_SOURCE-1].min_level = message.level;
				worker = get_worker(FINISHED);
				if (worker != -1) {
					set_wrk_state(worker, BUSY);
					message.status = BUSY;
					trace("%s Sending a peace of work to the node %d\n", NODE_NAME, worker); 
					gettimeofday(&t1, NULL); 
					message.d_search_time = (t1.tv_sec - workers_info[worker-1].t.tv_sec)*1000 + (t1.tv_usec - workers_info[worker-1].t.tv_usec)/1000; 
					send_work(worker, &message); 
				}
				else {
					trace("%s Push to stack\n", NODE_NAME); 
					gettimeofday(&t1, NULL);
					msg = (Message *) malloc(sizeof(Message));
					if (msg == NULL) {
						printf ("Panic! Not enough memory!\n");
					}
					memcpy((void *) msg, (void *) &message, sizeof(Message));
					push_msg_back(msg);
					gettimeofday(&t2, NULL); 
					push_time =  (t2.tv_sec - t1.tv_sec)*1000000 + (t2.tv_usec - t1.tv_usec); 
					timings.d_push_msg_time += push_time;
				}
				break;
			case FINISHED:
				trace("%s Have 'finished' message from %d\n", NODE_NAME, status.MPI_SOURCE);
				if (msg = pop_msg()) {
					trace("%s Sending a peace of work to the node %d, pop from stack\n", NODE_NAME, status.MPI_SOURCE); 
					gettimeofday(&t1, NULL); 
					message.d_search_time = (t1.tv_sec - t3.tv_sec)*1000 + (t1.tv_usec - t3.tv_usec)/1000; 
					send_work(status.MPI_SOURCE, msg);
					free(msg);
				}
				else {
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
				printf("%s Status error\n", NODE_NAME);
		}
	}
}

void do_worker(int id, const char *dump_filename) {
	Message message;
	MPI_Status status;
	int i;

	struct timeval t1, t2; 
	int run_time, waiting_for_msg_time; 

	timings.w_run_time = 0; 
	timings.w_idle_time = 0; 
	timings.network_time = 0;

	// Timer initialization
	signal(SIGALRM, tmr);
	signal(SIGUSR1, send_timings_signal);

	for (i = 3; i < plurality; i++ ) {
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

	if (dump_filename[0]) { 
		load_queue(dump_filename, 0); 
	} 

	gettimeofday(&t2, NULL);

	for(;;) {
		// receive from dispatcher:
		trace("%s Waiting for a message from the dispatcher\n", NODE_NAME);
		MPI_Recv((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD, &status);
		gettimeofday(&t1, NULL);

		waiting_for_msg_time =  (t1.tv_sec - t2.tv_sec)*1000 + (t1.tv_usec - t2.tv_usec)/1000; 
		timings.w_idle_time += waiting_for_msg_time; 
		timings.network_time += (waiting_for_msg_time - message.d_search_time); 
		if (message.status == QUIT) {
			trace("%s Received 'quit' message\n", NODE_NAME);
			break;
		}

		trace("%s Received a message from the dispatcher\n", NODE_NAME);

		max_s = message.max_s;

		for (i = 0; i <= message.level; ++i) {
			stats[i].rearrangement = message.rearrangement[i];
			stats[i].rearr_index = message.rearr_index[i];
		}

		b_free = (max_defects = n_step = n*(n-1) / 2) - 1;
		for (i = 0; i < n; i++) {
			a[i] = i;
		}

		for (i = k; i--; ) {
			set(2*i, 1);
		}

		trace("%s Run, level = %d, minlevel = %d\n", NODE_NAME, message.level, message.min_level);

		alarm(TQ);
		run(message.level, message.min_level);
		gettimeofday(&t2, NULL); 
		run_time =  (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec - t1.tv_usec)/1000; 
		timings.w_run_time += run_time; 
		message.status = FINISHED;
		trace("%s Finished\n", NODE_NAME); 

		send_message_to_dispatcher(&message);	}
}

int main(int argc, char **argv) {
	int i;
	char s[80], dump_filename[80];
	int numprocs;
	int myid;
	MPI_Status mpi_stat; 

	if (argc < 2) {
		printf(
			"Usage: %s -n N [-k K] [-max-def D] [-o filename] [-full] [-d dump_filename]\n"
			"-n             line count;\n"
			"-k             0 by default;\n"
			"-max-def       the number of allowed defects;\n"
			"-full          output all found generator sets;\n"
			"-d             dump file name\n"
			"-o             output file.\n", argv[0]);
		return 0;
	}

	dump_filename[0] = '\0';

	for (i = 1; i < argc; i++) {
		strcpy(s, argv[i]);
		if (!strcmp("-n", s))
			sscanf(argv[++i], "%d", &n);
		else if (!strcmp("-k", s))
			sscanf(argv[++i], "%d", &k);
		else if (!strcmp("-max-def", s))
			sscanf(argv[++i], "%d", &max_defects_default);
		else if (!strcmp("-o", s))
			strcpy(filename, argv[++i]);
		else if (!strcmp("-full", s))
			full = 1;
		else if (!strcmp("-d", s)) 
			strcpy(dump_filename, argv[++i]);
		else {
			printf(
				"Usage: %s -n N [-k K] [-max-def D] [-o filename] [-full] [-d dump_filename]\n"
				"-n             line count;\n"
				"-k             0 by default;\n"
				"-max-def       the number of allowed defects;\n"
				"-full          output all found generator sets;\n"
				"-d             dump file name\n"
				"-o             output file.\n", argv[0]);
			return 0;
		}
	}

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD,&myid);

	if(myid == 0) {
		strcpy(NODE_NAME, "Dispatcher:");
		do_dispatcher(numprocs, dump_filename);
	}
	else {
		sprintf(NODE_NAME, "Worker %d:", myid);
		do_worker(myid, dump_filename);
	}

	printf("---------->>>%s terminated.\n", NODE_NAME);
	MPI_Finalize();
	return 0;
}
#include <signal.h>
#include <atomic>
#include "graceful_stop.hpp"

static std::atomic<bool> g_stop = false;
static void on_signal(int sig) {
    g_stop = true;
}

bool is_stopped() {
    return g_stop;
}

void init_graceful_stop() {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
}
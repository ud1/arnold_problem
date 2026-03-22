/*
 * p3_maximal_v11.c — Generate p3-maximal pseudoline arrangements.
 *
 * Algorithm: Bokowski-Roudneff-Strempel, Section 4.
 * Improvements over v10:
 *   - Precomputed predecessor/successor bitmasks for O(1) candidate check
 *     instead of O(|rem|) inner loop per candidate
 *
 * Compile: gcc -O2 -o p3v11c tools/p3_maximal_v11.c
 * Usage:   ./p3v11c <n> [max_results]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define N_MAX 64

/* ── Chi (chirotope) ───────────────────────────────────────────── */

static int8_t chi_v[N_MAX+1][N_MAX+1][N_MAX+1];

static int chi_trail[N_MAX*N_MAX*N_MAX*6];
static int chi_trail_len;

/* ── Ordering: predecessor/successor bitmasks ──────────────────── */
/* pred_mask[p][e] = bitmask of elements that must come BEFORE e on hyperline p
 * succ_mask[p][e] = bitmask of elements that must come AFTER e on hyperline p
 * Left candidate:  (pred_mask[p][e] & rem_mask[p]) == 0
 * Right candidate: (succ_mask[p][e] & rem_mask[p]) == 0
 */
static uint64_t pred_mask[N_MAX+1][N_MAX+1];
static uint64_t succ_mask[N_MAX+1][N_MAX+1];

/* Trail for ordering updates: each entry is (h, a, b) meaning
 * pred_mask[h][b] |= (1<<a) and succ_mask[h][a] |= (1<<b) was done.
 * On undo: clear those bits. */
static int ord_trail[N_MAX*N_MAX*N_MAX*12];
static int ord_trail_len;

static int ord_mark(void) { return ord_trail_len; }

static void ord_undo(int mark) {
    while (ord_trail_len > mark) {
        ord_trail_len--;
        int idx = ord_trail_len;
        int h = ord_trail[idx*3];
        int a = ord_trail[idx*3+1];
        int b = ord_trail[idx*3+2];
        pred_mask[h][b] &= ~(1ULL << a);
        succ_mask[h][a] &= ~(1ULL << b);
    }
}

/* ── Forward declarations ──────────────────────────────────────── */

static int n_val;
static int8_t sgn[N_MAX+1][N_MAX+1];
static uint64_t rem_mask[N_MAX+1];

/* Update ordering masks when chi_v[h][a][b] is newly set to val_hab */
static inline void update_ordering(int h, int a, int b, int val_hab) {
    if (h < 1 || h == n_val) return;
    if (a == h || b == h) return;
    uint64_t rh = rem_mask[h];
    if (!((rh & (1ULL << a)) && (rh & (1ULL << b)))) return;

    int expected = sgn[h][a] * sgn[h][b];
    if (val_hab == expected) {
        /* a before b on hyperline h */
        if (!(pred_mask[h][b] & (1ULL << a))) {
            pred_mask[h][b] |= (1ULL << a);
            succ_mask[h][a] |= (1ULL << b);
            int idx = ord_trail_len++;
            ord_trail[idx*3]   = h;
            ord_trail[idx*3+1] = a;
            ord_trail[idx*3+2] = b;
        }
    } else {
        /* b before a on hyperline h */
        if (!(pred_mask[h][a] & (1ULL << b))) {
            pred_mask[h][a] |= (1ULL << b);
            succ_mask[h][b] |= (1ULL << a);
            int idx = ord_trail_len++;
            ord_trail[idx*3]   = h;
            ord_trail[idx*3+1] = b;
            ord_trail[idx*3+2] = a;
        }
    }
}

static int chi_mark(void) { return chi_trail_len; }

static int chi_put(int i, int j, int k, int val) {
    int cur = chi_v[i][j][k];
    if (cur != 0)
        return cur == val;
    chi_v[i][j][k] = (int8_t)val;
    chi_v[i][k][j] = (int8_t)(-val);
    chi_v[j][i][k] = (int8_t)(-val);
    chi_v[j][k][i] = (int8_t)val;
    chi_v[k][i][j] = (int8_t)val;
    chi_v[k][j][i] = (int8_t)(-val);
    int idx = chi_trail_len;
    chi_trail[idx*3]   = i;
    chi_trail[idx*3+1] = j;
    chi_trail[idx*3+2] = k;
    chi_trail_len++;

    /* Update predecessor/successor masks for all 3 hyperline perspectives */
    update_ordering(i, j, k, val);
    update_ordering(j, i, k, -val);
    update_ordering(k, i, j, val);

    return 1;
}

static void chi_undo(int mark) {
    while (chi_trail_len > mark) {
        chi_trail_len--;
        int idx = chi_trail_len;
        int i = chi_trail[idx*3];
        int j = chi_trail[idx*3+1];
        int k = chi_trail[idx*3+2];
        chi_v[i][j][k] = 0;
        chi_v[i][k][j] = 0;
        chi_v[j][i][k] = 0;
        chi_v[j][k][i] = 0;
        chi_v[k][i][j] = 0;
        chi_v[k][j][i] = 0;
    }
}

/* ── Adjacency tracking ───────────────────────────────────────── */

static int nbr_count[N_MAX+1][N_MAX+1];
static int nbr_data[N_MAX+1][N_MAX+1][2];

static int adj_trail[N_MAX*N_MAX*N_MAX*4];
static int adj_trail_len;

static int adj_mark(void) { return adj_trail_len; }

static void adj_undo(int mark) {
    while (adj_trail_len > mark) {
        adj_trail_len--;
        int idx = adj_trail_len;
        int q = adj_trail[idx*2];
        int x = adj_trail[idx*2+1];
        nbr_count[q][x]--;
    }
}

static int add_adj(int q, int a, int b) {
    int nc_a = nbr_count[q][a];
    int a_has_b = (nc_a > 0 && nbr_data[q][a][0] == b) ||
                  (nc_a > 1 && nbr_data[q][a][1] == b);
    int nc_b = nbr_count[q][b];
    int b_has_a = (nc_b > 0 && nbr_data[q][b][0] == a) ||
                  (nc_b > 1 && nbr_data[q][b][1] == a);

    if (a_has_b && b_has_a)
        return 1;

    if (!a_has_b) {
        if (nc_a >= 2) return 0;
        nbr_data[q][a][nc_a] = b;
        nbr_count[q][a] = nc_a + 1;
        int idx = adj_trail_len;
        adj_trail[idx*2]   = q;
        adj_trail[idx*2+1] = a;
        adj_trail_len++;
    }
    if (!b_has_a) {
        if (nc_b >= 2) return 0;
        nbr_data[q][b][nc_b] = a;
        nbr_count[q][b] = nc_b + 1;
        int idx = adj_trail_len;
        adj_trail[idx*2]   = q;
        adj_trail[idx*2+1] = b;
        adj_trail_len++;
    }
    return 1;
}

static int can_add_adj(int q, int a, int b) {
    int nc_a = nbr_count[q][a];
    int a_has_b = (nc_a > 0 && nbr_data[q][a][0] == b) ||
                  (nc_a > 1 && nbr_data[q][a][1] == b);
    if (!a_has_b && nc_a >= 2) return 0;
    int nc_b = nbr_count[q][b];
    int b_has_a = (nc_b > 0 && nbr_data[q][b][0] == a) ||
                  (nc_b > 1 && nbr_data[q][b][1] == a);
    if (!b_has_a && nc_b >= 2) return 0;
    return 1;
}

/* ── Global state ─────────────────────────────────────────────── */

static int hn[N_MAX];
static int hn_len;
static int second_el[N_MAX+1];
static int last_el[N_MAX+1];

/* Partial hyperlines — bidirectional */
static int left_arr[N_MAX+1][N_MAX];
static int left_len[N_MAX+1];
static int right_arr[N_MAX+1][N_MAX];
static int right_len[N_MAX+1];

static int result_count;
static int max_results_val;
static long long node_count;
static int verbose;
static int debug_mode;
static struct timespec t0_global;

/* ── Result storage ───────────────────────────────────────────── */

#define MAX_STORE 256
static int stored_hls[MAX_STORE][N_MAX+1][N_MAX];
static int stored_hls_len[MAX_STORE][N_MAX+1];

/* ── apply_triangle ───────────────────────────────────────────── */

static int apply_triangle(int p, int a, int b) {
    if (!add_adj(p, a, b)) return 0;
    if (!add_adj(a, p, b)) return 0;
    if (!add_adj(b, p, a)) return 0;
    return 1;
}

/* ── Block-endpoint check ─────────────────────────────────────── */

static inline int is_block_endpoint(int p, int e) {
    int nc = nbr_count[p][e];
    if (nc < 2) return 1;
    uint64_t rp = rem_mask[p];
    int rem_nbrs = 0;
    for (int i = 0; i < nc; i++) {
        int nb = nbr_data[p][e][i];
        if (rp & (1ULL << nb))
            if (++rem_nbrs >= 2) return 0;
    }
    return 1;
}

/* ── get_left_candidates ──────────────────────────────────────── */

static int get_left_candidates(int p, int *cands) {
    uint64_t rp = rem_mask[p];
    int f = left_arr[p][left_len[p] - 1];
    int cnt = 0;

    uint64_t mask = rp;
    while (mask) {
        int e = __builtin_ctzll(mask);
        mask &= mask - 1;

        if (!is_block_endpoint(p, e))
            continue;
        if (!can_add_adj(p, f, e) || !can_add_adj(f, p, e) || !can_add_adj(e, p, f))
            continue;

        /* O(1) predecessor check: any remaining element must come before e? */
        if (pred_mask[p][e] & rp)
            continue;

        cands[cnt++] = e;
    }
    return cnt;
}

/* ── get_right_candidates ─────────────────────────────────────── */

static int get_right_candidates(int p, int *cands) {
    uint64_t rp = rem_mask[p];
    int f = right_arr[p][right_len[p] - 1];
    int cnt = 0;

    uint64_t mask = rp;
    while (mask) {
        int e = __builtin_ctzll(mask);
        mask &= mask - 1;

        if (!is_block_endpoint(p, e))
            continue;
        if (!can_add_adj(p, e, f) || !can_add_adj(e, p, f) || !can_add_adj(f, p, e))
            continue;

        /* O(1) successor check: any remaining element must come after e? */
        if (succ_mask[p][e] & rp)
            continue;

        cands[cnt++] = e;
    }
    return cnt;
}

/* ── place_left ───────────────────────────────────────────────── */

static int place_left(int p, int e) {
    int8_t *sp = sgn[p];
    int se = sp[e];
    int f = left_arr[p][left_len[p] - 1];

    rem_mask[p] &= ~(1ULL << e);
    left_arr[p][left_len[p]] = e;
    left_len[p]++;

    /* Chi: e after all left elements */
    for (int i = 0; i < left_len[p]; i++) {
        int x = left_arr[p][i];
        if (x == e) continue;
        if (!chi_put(p, x, e, sp[x] * se))
            return 0;
    }
    /* Chi: e before all right elements */
    for (int i = 0; i < right_len[p]; i++) {
        int x = right_arr[p][i];
        if (!chi_put(p, x, e, -(sp[x] * se)))
            return 0;
    }
    /* Chi: e before all remaining gap elements */
    uint64_t rmask = rem_mask[p];
    while (rmask) {
        int x = __builtin_ctzll(rmask);
        rmask &= rmask - 1;
        if (!chi_put(p, x, e, -(sp[x] * se)))
            return 0;
    }

    /* Triangle (p, f, e) */
    if (!apply_triangle(p, f, e))
        return 0;

    /* Gap closed? */
    if (rem_mask[p] == 0) {
        int rf = right_arr[p][right_len[p] - 1];
        if (!apply_triangle(p, e, rf))
            return 0;
    }

    return 1;
}

static void unplace_left(int p, int e) {
    left_len[p]--;
    rem_mask[p] |= (1ULL << e);
}

/* ── place_right ──────────────────────────────────────────────── */

static int place_right(int p, int e) {
    int8_t *sp = sgn[p];
    int se = sp[e];
    int f = right_arr[p][right_len[p] - 1];

    rem_mask[p] &= ~(1ULL << e);
    right_arr[p][right_len[p]] = e;
    right_len[p]++;

    /* Chi: all left elements come before e */
    for (int i = 0; i < left_len[p]; i++) {
        int x = left_arr[p][i];
        if (!chi_put(p, x, e, sp[x] * se))
            return 0;
    }
    /* Chi: all remaining gap elements come before e */
    uint64_t rmask = rem_mask[p];
    while (rmask) {
        int x = __builtin_ctzll(rmask);
        rmask &= rmask - 1;
        if (!chi_put(p, x, e, sp[x] * se))
            return 0;
    }
    /* Chi: all right elements come after e */
    for (int i = 0; i < right_len[p]; i++) {
        int x = right_arr[p][i];
        if (x == e) continue;
        if (!chi_put(p, x, e, -(sp[x] * se)))
            return 0;
    }

    /* Triangle (p, e, f) */
    if (!apply_triangle(p, e, f))
        return 0;

    /* Gap closed? */
    if (rem_mask[p] == 0) {
        int lf = left_arr[p][left_len[p] - 1];
        if (!apply_triangle(p, lf, e))
            return 0;
    }

    return 1;
}

static void unplace_right(int p, int e) {
    right_len[p]--;
    rem_mask[p] |= (1ULL << e);
}

/* ── extract_tri ──────────────────────────────────────────────── */

static int build_sequence(int p, int *seq) {
    int len = 0;
    for (int i = 0; i < left_len[p]; i++)
        seq[len++] = left_arr[p][i];
    for (int i = right_len[p] - 1; i >= 0; i--)
        seq[len++] = right_arr[p][i];
    return len;
}

static int extract_tri_count(void) {
    int n = n_val;
    static uint64_t adj[N_MAX+1][N_MAX+1];
    memset(adj, 0, sizeof(adj));

    for (int i = 0; i < hn_len; i++) {
        int j = (i + 1) % hn_len;
        adj[n][hn[i]] |= 1ULL << hn[j];
        adj[n][hn[j]] |= 1ULL << hn[i];
    }
    for (int p = 1; p < n; p++) {
        int seq[N_MAX];
        int len = build_sequence(p, seq);
        for (int i = 0; i < len; i++) {
            int j = (i + 1) % len;
            adj[p][seq[i]] |= 1ULL << seq[j];
            adj[p][seq[j]] |= 1ULL << seq[i];
        }
    }

    int count = 0;
    for (int i = 1; i <= n; i++)
        for (int j = i + 1; j <= n; j++)
            for (int k = j + 1; k <= n; k++) {
                if ((adj[i][j] & (1ULL << k)) &&
                    (adj[j][i] & (1ULL << k)) &&
                    (adj[k][i] & (1ULL << j)))
                    count++;
            }
    return count;
}

/* ── O-matrix and generator extraction ────────────────────────── */

static int omatrix[N_MAX][N_MAX];
static int omatrix_len[N_MAX];
static int gens_buf[N_MAX * N_MAX];

static void build_omatrix(int r, int n) {
    int old_to_new[N_MAX+1];
    int8_t hn_sgn[N_MAX+1];
    memset(old_to_new, -1, sizeof(old_to_new));
    memset(hn_sgn, 0, sizeof(hn_sgn));
    for (int i = 0; i < hn_len; i++) {
        old_to_new[hn[i]] = i;
        hn_sgn[hn[i]] = (i % 2 == 0) ? 1 : -1;
    }
    for (int p = 1; p < n; p++) {
        int new_line = old_to_new[p];
        int *hl = stored_hls[r][p];
        int hl_len = stored_hls_len[r][p];
        int row[N_MAX];
        int rlen = 0;
        for (int i = 0; i < hl_len; i++) {
            if (hl[i] == n) continue;
            row[rlen++] = old_to_new[hl[i]];
        }
        if (hn_sgn[p] == -1)
            for (int i = 0; i < rlen / 2; i++) {
                int t = row[i]; row[i] = row[rlen-1-i]; row[rlen-1-i] = t;
            }
        memcpy(omatrix[new_line], row, rlen * sizeof(int));
        omatrix_len[new_line] = rlen;
    }
}

/* Build O-matrix from temporary hyperline arrays (for immediate output) */
static void build_omatrix_from_hls(int n, int tmp_hl[][N_MAX], int *tmp_hl_len) {
    int old_to_new[N_MAX+1];
    int8_t hn_sgn[N_MAX+1];
    memset(old_to_new, -1, sizeof(old_to_new));
    memset(hn_sgn, 0, sizeof(hn_sgn));
    for (int i = 0; i < hn_len; i++) {
        old_to_new[hn[i]] = i;
        hn_sgn[hn[i]] = (i % 2 == 0) ? 1 : -1;
    }
    for (int p = 1; p < n; p++) {
        int new_line = old_to_new[p];
        int row[N_MAX];
        int rlen = 0;
        for (int i = 0; i < tmp_hl_len[p]; i++) {
            if (tmp_hl[p][i] == n) continue;
            row[rlen++] = old_to_new[tmp_hl[p][i]];
        }
        if (hn_sgn[p] == -1)
            for (int i = 0; i < rlen / 2; i++) {
                int t = row[i]; row[i] = row[rlen-1-i]; row[rlen-1-i] = t;
            }
        memcpy(omatrix[new_line], row, rlen * sizeof(int));
        omatrix_len[new_line] = rlen;
    }
}

static int omatrix_to_generators(int nn) {
    int lines[N_MAX], pos[N_MAX];
    for (int i = 0; i < nn; i++) { lines[i] = i; pos[i] = 0; }
    int total_incidences = 0;
    for (int i = 0; i < nn; i++) total_incidences += omatrix_len[i];
    int steps = total_incidences / 2;
    for (int step = 0; step < steps; step++) {
        int found = 0;
        for (int g = 0; g + 1 < nn; g++) {
            int left = lines[g], right = lines[g+1];
            if (pos[left] >= omatrix_len[left]) continue;
            if (pos[right] >= omatrix_len[right]) continue;
            if (omatrix[left][pos[left]] != right) continue;
            if (omatrix[right][pos[right]] != left) continue;
            pos[left]++; pos[right]++;
            lines[g] = right; lines[g+1] = left;
            gens_buf[step] = g;
            found = 1;
            break;
        }
        if (!found) return -1;
    }
    for (int i = 0; i < nn; i++)
        if (pos[i] != omatrix_len[i]) return -1;
    return steps;
}

/* ── Store result ─────────────────────────────────────────────── */

static void store_result(void) {
    int n = n_val;
    int idx = result_count;

    if (debug_mode) {
        if (idx >= MAX_STORE) return;
        for (int i = 0; i < hn_len; i++)
            stored_hls[idx][n][i] = hn[i];
        stored_hls_len[idx][n] = hn_len;
        for (int p = 1; p < n; p++)
            stored_hls_len[idx][p] = build_sequence(p, stored_hls[idx][p]);
    } else {
        int tmp_hl[N_MAX+1][N_MAX];
        int tmp_hl_len[N_MAX+1];
        for (int i = 0; i < hn_len; i++)
            tmp_hl[n][i] = hn[i];
        tmp_hl_len[n] = hn_len;
        for (int p = 1; p < n; p++)
            tmp_hl_len[p] = build_sequence(p, tmp_hl[p]);

        int nn = n - 1;
        build_omatrix_from_hls(n, tmp_hl, tmp_hl_len);
        int ngens = omatrix_to_generators(nn);

        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double elapsed = (t1.tv_sec - t0_global.tv_sec) +
                         (t1.tv_nsec - t0_global.tv_nsec) * 1e-9;

        printf("[%es] i=%e)", elapsed, (double)node_count);
        if (ngens >= 0)
            for (int i = 0; i < ngens; i++)
                printf(" %d", gens_buf[i]);
        else
            printf(" ERROR");
        printf("\n");
        fflush(stdout);
    }
}

/* ── solve ────────────────────────────────────────────────────── */

typedef struct {
    int p; int e; int cmk; int amk; int omk;
    int side;
} ForcedEntry;

static ForcedEntry forced_stack[N_MAX * N_MAX * N_MAX];
static int forced_stack_top;

static void solve(void) {
    int n = n_val;
    if (max_results_val > 0 && result_count >= max_results_val)
        return;

    node_count++;
    if (verbose && (node_count % 10000) == 0) {
        int done = 0, total_rem = 0;
        for (int p = 1; p < n; p++) {
            if (rem_mask[p] == 0) done++;
            total_rem += __builtin_popcountll(rem_mask[p]);
        }
        fprintf(stderr, "  nodes=%lld complete=%d/%d rem=%d results=%d\n",
                node_count, done, n - 1, total_rem, result_count);
    }

    int forced_base = forced_stack_top;
    int forced_count = 0;
    int contradiction = 0;

    int lcands[N_MAX], rcands[N_MAX];

    /* Unit propagation */
    while (!contradiction) {
        int found_forced = 0;
        for (int p = 1; p < n; p++) {
            if (rem_mask[p] == 0) continue;
            int nlc = get_left_candidates(p, lcands);
            int nrc = get_right_candidates(p, rcands);
            int total = nlc + nrc;
            if (total == 0) {
                contradiction = 1;
                break;
            }
            int do_force = 0, e = 0, side = 0;
            if (total == 1) {
                do_force = 1;
                if (nlc == 1) { e = lcands[0]; side = 0; }
                else          { e = rcands[0]; side = 1; }
            } else if (nlc == 1) {
                do_force = 1; e = lcands[0]; side = 0;
            } else if (nrc == 1) {
                do_force = 1; e = rcands[0]; side = 1;
            }
            if (do_force) {
                int cmk = chi_mark();
                int amk = adj_mark();
                int omk = ord_mark();
                int fi = forced_base + forced_count;
                forced_stack[fi].p = p;
                forced_stack[fi].e = e;
                forced_stack[fi].cmk = cmk;
                forced_stack[fi].amk = amk;
                forced_stack[fi].omk = omk;
                forced_stack[fi].side = side;
                forced_count++;
                forced_stack_top++;
                int ok = side == 0 ? place_left(p, e) : place_right(p, e);
                if (!ok) {
                    contradiction = 1;
                } else {
                    found_forced = 1;
                }
                break;
            }
        }
        if (!found_forced)
            break;
    }

    /* Find branching hyperline */
    int best_p = -1, best_count = N_MAX * 2, best_side = 0;
    int best_cands[N_MAX];
    int best_nc = 0;

    if (!contradiction) {
        int has_incomplete = 0;
        for (int p = 1; p < n; p++) {
            if (rem_mask[p] == 0) continue;
            has_incomplete = 1;
            int nlc = get_left_candidates(p, lcands);
            int nrc = get_right_candidates(p, rcands);
            if (nlc + nrc == 0) {
                contradiction = 1;
                break;
            }
            int nc, sd;
            int *src;
            if (nlc > 0) { nc = nlc; sd = 0; src = lcands; }
            else          { nc = nrc; sd = 1; src = rcands; }
            if (nc < best_count) {
                best_count = nc;
                best_p = p;
                best_side = sd;
                best_nc = nc;
                memcpy(best_cands, src, nc * sizeof(int));
            }
        }
        if (!contradiction && !has_incomplete) {
            int exp_tri = n * (n - 1) / 3;
            int tri = extract_tri_count();
            if (tri == exp_tri) {
                store_result();
                result_count++;
                if (verbose)
                    fprintf(stderr, "  FOUND #%d (%lld nodes, %d tri)\n",
                            result_count, node_count, tri);
            }
        }
    }

    if (!contradiction && best_p >= 0) {
        for (int i = 0; i < best_nc - 1; i++)
            for (int j = i + 1; j < best_nc; j++)
                if (best_cands[i] > best_cands[j]) {
                    int t = best_cands[i]; best_cands[i] = best_cands[j]; best_cands[j] = t;
                }

        for (int ci = 0; ci < best_nc; ci++) {
            if (max_results_val > 0 && result_count >= max_results_val) break;
            int e = best_cands[ci];
            int cmk = chi_mark();
            int amk = adj_mark();
            int omk = ord_mark();
            int ok = best_side == 0 ? place_left(best_p, e) : place_right(best_p, e);
            if (ok)
                solve();
            if (best_side == 0) unplace_left(best_p, e);
            else                unplace_right(best_p, e);
            chi_undo(cmk);
            adj_undo(amk);
            ord_undo(omk);
        }
    }

    /* Undo forced moves */
    for (int i = forced_count - 1; i >= 0; i--) {
        int fi = forced_base + i;
        if (forced_stack[fi].side == 0)
            unplace_left(forced_stack[fi].p, forced_stack[fi].e);
        else
            unplace_right(forced_stack[fi].p, forced_stack[fi].e);
        chi_undo(forced_stack[fi].cmk);
        adj_undo(forced_stack[fi].amk);
        ord_undo(forced_stack[fi].omk);
    }
    forced_stack_top = forced_base;
}

/* ── generate ─────────────────────────────────────────────────── */

static int generate(int n, int max_results) {
    int m = n / 2;
    n_val = n;
    max_results_val = max_results;
    result_count = 0;
    node_count = 0;
    forced_stack_top = 0;

    memset(chi_v, 0, sizeof(chi_v));
    chi_trail_len = 0;
    memset(nbr_count, 0, sizeof(nbr_count));
    memset(nbr_data, 0, sizeof(nbr_data));
    adj_trail_len = 0;
    memset(pred_mask, 0, sizeof(pred_mask));
    memset(succ_mask, 0, sizeof(succ_mask));
    ord_trail_len = 0;

    /* Build hn */
    hn_len = 0;
    for (int i = 1; i <= m; i++) {
        hn[hn_len++] = i;
        if (m + i <= 2 * m - 1)
            hn[hn_len++] = m + i;
    }

    int8_t hn_signs[N_MAX+1];
    int hn_pos[N_MAX+1];
    memset(hn_signs, 0, sizeof(hn_signs));
    memset(hn_pos, 0, sizeof(hn_pos));
    for (int i = 0; i < hn_len; i++) {
        hn_signs[hn[i]] = (i % 2 == 0) ? 1 : -1;
        hn_pos[hn[i]] = i;
    }

    for (int a = 0; a < hn_len; a++)
        for (int b = a + 1; b < hn_len; b++)
            chi_put(n, hn[a], hn[b], hn_signs[hn[a]] * hn_signs[hn[b]]);

    memset(sgn, 0, sizeof(sgn));
    for (int p = 1; p < n; p++) {
        sgn[p][n] = 1;
        for (int q = 1; q < n; q++) {
            if (q == p) continue;
            sgn[p][q] = -chi_v[n][p][q];
        }
    }

    memset(second_el, 0, sizeof(second_el));
    memset(last_el, 0, sizeof(last_el));
    for (int p = 1; p < n; p++) {
        int pos = hn_pos[p];
        second_el[p] = hn[(pos + 1) % hn_len];
        last_el[p] = hn[((pos - 1) + hn_len) % hn_len];
    }

    /* Initialize left, right, rem */
    memset(left_arr, 0, sizeof(left_arr));
    memset(left_len, 0, sizeof(left_len));
    memset(right_arr, 0, sizeof(right_arr));
    memset(right_len, 0, sizeof(right_len));
    memset(rem_mask, 0, sizeof(rem_mask));
    for (int p = 1; p < n; p++) {
        left_arr[p][0] = n;
        left_arr[p][1] = second_el[p];
        left_len[p] = 2;
        right_arr[p][0] = last_el[p];
        right_len[p] = 1;
        uint64_t mask = 0;
        for (int q = 1; q <= n; q++) {
            if (q == p || q == n || q == second_el[p] || q == last_el[p])
                continue;
            mask |= 1ULL << q;
        }
        rem_mask[p] = mask;
    }

    /* Initial chi values — these will trigger update_ordering via chi_put */
    int init_ok = 1;
    for (int p = 1; p < n && init_ok; p++) {
        int s = second_el[p], l = last_el[p];
        if (!chi_put(p, s, l, sgn[p][s] * sgn[p][l])) { init_ok = 0; break; }
        uint64_t rmask = rem_mask[p];
        uint64_t m2;
        m2 = rmask;
        while (m2 && init_ok) {
            int q = __builtin_ctzll(m2); m2 &= m2 - 1;
            if (!chi_put(p, s, q, sgn[p][s] * sgn[p][q])) init_ok = 0;
        }
        m2 = rmask;
        while (m2 && init_ok) {
            int q = __builtin_ctzll(m2); m2 &= m2 - 1;
            if (!chi_put(p, q, l, sgn[p][q] * sgn[p][l])) init_ok = 0;
        }
    }

    if (!init_ok) {
        if (verbose)
            fprintf(stderr, "  Contradiction during initialization — no solutions exist\n");
        return 0;
    }

    /* Initialize adjacencies */
    for (int i = 0; i < hn_len; i++) {
        int j = (i + 1) % hn_len;
        add_adj(n, hn[i], hn[j]);
    }
    for (int p = 1; p < n; p++) {
        add_adj(p, n, second_el[p]);
        add_adj(p, n, last_el[p]);
    }
    for (int p = 1; p < n; p++) {
        add_adj(second_el[p], p, n);
        add_adj(last_el[p], p, n);
    }
    adj_trail_len = 0;

    solve();
    return result_count;
}

/* ── main ─────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: p3v11 <n> [max_results] [--debug]\n");
        return 1;
    }

    /* Parse arguments: positional n, optional max_results, optional --debug */
    int n = 0, max_results = 0;
    debug_mode = 0;
    int pos_args[2], pos_count = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else {
            if (pos_count < 2) pos_args[pos_count] = atoi(argv[i]);
            pos_count++;
        }
    }
    if (pos_count < 1) {
        fprintf(stderr, "Usage: p3v11 <n> [max_results] [--debug]\n");
        return 1;
    }
    n = pos_args[0];
    if (pos_count >= 2) max_results = pos_args[1];

    if (n % 2 != 0 || n < 4 || n % 3 == 2) {
        fprintf(stderr, "Invalid n=%d: need n even, n>=4, n mod 3 != 2\n", n);
        return 1;
    }

    verbose = debug_mode;

    if (debug_mode) {
        int m = n / 2;
        int exp_tri = n * (n - 1) / 3;
        const char *lim = max_results ? " (limit)" : " (exhaustive)";
        printf("n=%d m=%d expected_tri=%d%s\n", n, m, exp_tri, lim);
    }

    clock_gettime(CLOCK_MONOTONIC, &t0_global);

    int count = generate(n, max_results);

    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0_global.tv_sec) +
                     (t1.tv_nsec - t0_global.tv_nsec) * 1e-9;

    if (debug_mode) {
        printf("\n%d arrangement(s) in %.3fs (%lld nodes)\n", count, elapsed, node_count);

        int show = count < MAX_STORE ? count : MAX_STORE;
        for (int r = 0; r < show; r++) {
            printf("\n=== #%d ===\n", r + 1);
            for (int p = 1; p <= n; p++) {
                printf("  L%d:", p);
                for (int i = 0; i < stored_hls_len[r][p]; i++)
                    printf(" %d", stored_hls[r][p][i]);
                printf("\n");
            }

            /* Triangle count */
            uint64_t adj_bits[N_MAX+1][N_MAX+1];
            memset(adj_bits, 0, sizeof(adj_bits));
            for (int p = 1; p <= n; p++) {
                int len = stored_hls_len[r][p];
                for (int i = 0; i < len; i++) {
                    int j = (i + 1) % len;
                    int a = stored_hls[r][p][i];
                    int b = stored_hls[r][p][j];
                    adj_bits[p][a] |= 1ULL << b;
                    adj_bits[p][b] |= 1ULL << a;
                }
            }
            int tri = 0;
            for (int i = 1; i <= n; i++)
                for (int j = i + 1; j <= n; j++)
                    for (int k = j + 1; k <= n; k++) {
                        if ((adj_bits[i][j] & (1ULL << k)) &&
                            (adj_bits[j][i] & (1ULL << k)) &&
                            (adj_bits[k][i] & (1ULL << j)))
                            tri++;
                    }
            printf("  Triangles (%d)\n", tri);

            /* O-matrix and generators */
            int nn = n - 1;
            build_omatrix(r, n);
            printf("  O-matrix (%d lines, 0-based):\n", nn);
            for (int i = 0; i < nn; i++) {
                printf("    [%d]:", i);
                for (int j = 0; j < omatrix_len[i]; j++)
                    printf(" %d", omatrix[i][j]);
                printf("\n");
            }

            int ngens = omatrix_to_generators(nn);
            if (ngens < 0) {
                printf("  Generators: ERROR (conversion failed)\n");
            } else {
                printf("  Generators (%d):", ngens);
                for (int i = 0; i < ngens; i++)
                    printf(" %d", gens_buf[i]);
                printf("\n");
            }
        }
    } else {
        fprintf(stderr, "%d arrangement(s) in %.3fs (%lld nodes)\n",
                count, elapsed, node_count);
    }

    return 0;
}

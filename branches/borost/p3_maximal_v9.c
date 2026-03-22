/*
 * p3_maximal_v9.c — Generate p3-maximal pseudoline arrangements.
 *
 * Algorithm: Bokowski-Roudneff-Strempel, Section 4.
 * Direct port of p3_maximal_v9.py.
 *
 * Compile: gcc -O2 -o p3v9c tools/p3_maximal_v9.c
 * Usage:   ./p3v9c <n> [max_results]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define N_MAX 32

/* ── Chi (chirotope) ───────────────────────────────────────────── */

static int8_t chi_v[N_MAX+1][N_MAX+1][N_MAX+1];

/* Trail for chi: store (i,j,k) packed as i*S*S + j*S + k */
static int chi_trail[N_MAX*N_MAX*N_MAX*6];
static int chi_trail_len;

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
    /* Store canonical triple for undo */
    int idx = chi_trail_len;
    chi_trail[idx*3]   = i;
    chi_trail[idx*3+1] = j;
    chi_trail[idx*3+2] = k;
    chi_trail_len++;
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

/* Trail for adj: store (q, x) */
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

static int n_val;          /* n */
static int hn[N_MAX];      /* hyperline n elements */
static int hn_len;
static int8_t sgn[N_MAX+1][N_MAX+1];
static int second_el[N_MAX+1];
static int last_el[N_MAX+1];

/* Partial hyperlines */
static int left_arr[N_MAX+1][N_MAX];
static int left_len[N_MAX+1];
static uint32_t rem_mask[N_MAX+1]; /* bitmask of remaining elements */

static int result_count;
static int max_results_val;
static long long node_count;
static int verbose;

/* ── Hyperline results storage (store up to 256 results) ─────── */

#define MAX_STORE 256
static int stored_hls[MAX_STORE][N_MAX+1][N_MAX]; /* stored_hls[result][p][...] */
static int stored_hls_len[MAX_STORE][N_MAX+1];

/* ── Helper: iterate bits ─────────────────────────────────────── */

/* Use __builtin_ctz for bit iteration */

/* ── apply_triangle ───────────────────────────────────────────── */

static int apply_triangle(int p, int a, int b) {
    if (!add_adj(p, a, b)) return 0;
    if (!add_adj(a, p, b)) return 0;
    if (!add_adj(b, p, a)) return 0;
    return 1;
}

/* ── get_left_candidates ──────────────────────────────────────── */

static int get_left_candidates(int p, int *cands) {
    int8_t *sp = sgn[p];
    uint32_t rp = rem_mask[p];
    int f = left_arr[p][left_len[p] - 1]; /* frontier */
    int cnt = 0;

    uint32_t mask = rp;
    while (mask) {
        int e = __builtin_ctz(mask);
        mask &= mask - 1;

        /* Pre-filter: triangle (p,f,e) feasible? */
        if (!can_add_adj(p, f, e) || !can_add_adj(f, p, e) || !can_add_adj(e, p, f))
            continue;

        /* Chi ordering check: no remaining predecessor */
        int se = sp[e];
        int ok = 1;
        uint32_t mask2 = rp & ~(1u << e);
        while (mask2) {
            int x = __builtin_ctz(mask2);
            mask2 &= mask2 - 1;
            int v = chi_v[p][x][e];
            if (v != 0 && v == sp[x] * se) {
                ok = 0;
                break;
            }
        }
        if (ok)
            cands[cnt++] = e;
    }
    return cnt;
}

/* ── place_left ───────────────────────────────────────────────── */

static int place_left(int p, int e) {
    int8_t *sp = sgn[p];
    int se = sp[e];
    int lp = last_el[p];
    int f = left_arr[p][left_len[p] - 1];

    rem_mask[p] &= ~(1u << e);
    left_arr[p][left_len[p]] = e;
    left_len[p]++;

    /* Chi: e after all in left (including itself skipped) */
    for (int i = 0; i < left_len[p]; i++) {
        int x = left_arr[p][i];
        if (x == e) continue;
        if (!chi_put(p, x, e, sp[x] * se))
            return 0;
    }
    /* Chi: last before e (negative) */
    if (!chi_put(p, lp, e, -(sp[lp] * se)))
        return 0;
    /* Chi: e before all remaining */
    uint32_t rmask = rem_mask[p];
    while (rmask) {
        int x = __builtin_ctz(rmask);
        rmask &= rmask - 1;
        if (!chi_put(p, x, e, -(sp[x] * se)))
            return 0;
    }

    /* Triangle (p, f, e) */
    if (!apply_triangle(p, f, e))
        return 0;

    /* Gap closed: triangle (p, e, last) */
    if (rem_mask[p] == 0) {
        if (!apply_triangle(p, e, lp))
            return 0;
    }

    return 1;
}

static void unplace_left(int p, int e) {
    left_len[p]--;
    rem_mask[p] |= (1u << e);
}

/* ── extract_tri ──────────────────────────────────────────────── */

static int extract_tri_count(void) {
    int n = n_val;
    /* Build adjacency sets for each hyperline */
    /* For each p, adj[p] is a set of pairs. Use a 2D bit-matrix. */
    /* adj[p][a] is a bitmask of elements adjacent to a on hyperline p */
    static uint32_t adj[N_MAX+1][N_MAX+1];
    memset(adj, 0, sizeof(adj));

    /* Hyperline n */
    for (int i = 0; i < hn_len; i++) {
        int j = (i + 1) % hn_len;
        adj[n][hn[i]] |= 1u << hn[j];
        adj[n][hn[j]] |= 1u << hn[i];
    }
    /* Hyperlines 1..n-1 */
    for (int p = 1; p < n; p++) {
        int len = left_len[p] + 1; /* left + last */
        /* Build full sequence */
        int seq[N_MAX];
        for (int i = 0; i < left_len[p]; i++)
            seq[i] = left_arr[p][i];
        seq[left_len[p]] = last_el[p];

        for (int i = 0; i < len; i++) {
            int j = (i + 1) % len;
            adj[p][seq[i]] |= 1u << seq[j];
            adj[p][seq[j]] |= 1u << seq[i];
        }
    }

    int count = 0;
    for (int i = 1; i <= n; i++)
        for (int j = i + 1; j <= n; j++)
            for (int k = j + 1; k <= n; k++) {
                if ((adj[i][j] & (1u << k)) &&
                    (adj[j][i] & (1u << k)) &&
                    (adj[k][i] & (1u << j)))
                    count++;
            }
    return count;
}

/* ── Store result ─────────────────────────────────────────────── */

static void store_result(void) {
    int n = n_val;
    int idx = result_count;
    if (idx >= MAX_STORE) return;

    /* Hyperline n */
    for (int i = 0; i < hn_len; i++)
        stored_hls[idx][n][i] = hn[i];
    stored_hls_len[idx][n] = hn_len;

    /* Hyperlines 1..n-1 */
    for (int p = 1; p < n; p++) {
        int len = 0;
        for (int i = 0; i < left_len[p]; i++)
            stored_hls[idx][p][len++] = left_arr[p][i];
        stored_hls[idx][p][len++] = last_el[p];
        stored_hls_len[idx][p] = len;
    }
}

/* ── solve ────────────────────────────────────────────────────── */

typedef struct { int p; int e; int cmk; int amk; } ForcedEntry;

static void solve(void) {
    int n = n_val;
    if (max_results_val > 0 && result_count >= max_results_val)
        return;

    node_count++;
    if (verbose && (node_count % 10000) == 0) {
        int done = 0, total_rem = 0;
        for (int p = 1; p < n; p++) {
            if (rem_mask[p] == 0) done++;
            total_rem += __builtin_popcount(rem_mask[p]);
        }
        fprintf(stderr, "  nodes=%lld complete=%d/%d rem=%d results=%d\n",
                node_count, done, n - 1, total_rem, result_count);
    }

    ForcedEntry forced[N_MAX * N_MAX];
    int forced_count = 0;
    int contradiction = 0;

    int cands[N_MAX];

    /* Unit propagation */
    while (!contradiction) {
        int found_forced = 0;
        for (int p = 1; p < n; p++) {
            if (rem_mask[p] == 0) continue;
            int nc = get_left_candidates(p, cands);
            if (nc == 0) {
                contradiction = 1;
                break;
            }
            if (nc == 1) {
                int cmk = chi_mark();
                int amk = adj_mark();
                int e = cands[0];
                forced[forced_count].p = p;
                forced[forced_count].e = e;
                forced[forced_count].cmk = cmk;
                forced[forced_count].amk = amk;
                forced_count++;
                if (!place_left(p, e)) {
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
    int incomplete_p[N_MAX];
    int incomplete_cands[N_MAX][N_MAX];
    int incomplete_ncands[N_MAX];
    int incomplete_count = 0;

    if (!contradiction) {
        for (int p = 1; p < n; p++) {
            if (rem_mask[p] == 0) continue;
            int nc = get_left_candidates(p, cands);
            if (nc == 0) {
                contradiction = 1;
                break;
            }
            incomplete_p[incomplete_count] = p;
            incomplete_ncands[incomplete_count] = nc;
            memcpy(incomplete_cands[incomplete_count], cands, nc * sizeof(int));
            incomplete_count++;
        }
    }

    if (!contradiction) {
        if (incomplete_count == 0) {
            /* Found a complete arrangement — verify */
            int exp_tri = n * (n - 1) / 3;
            int tri = extract_tri_count();
            if (tri == exp_tri) {
                store_result();
                result_count++;
                if (verbose) {
                    fprintf(stderr, "  FOUND #%d (%lld nodes, %d tri)\n",
                            result_count, node_count, tri);
                }
            }
        } else {
            /* Find most constrained hyperline */
            int best = 0;
            for (int i = 1; i < incomplete_count; i++) {
                if (incomplete_ncands[i] < incomplete_ncands[best])
                    best = i;
            }
            int best_p = incomplete_p[best];
            int best_nc = incomplete_ncands[best];

            /* Sort candidates for deterministic order */
            int *bc = incomplete_cands[best];
            for (int i = 0; i < best_nc - 1; i++)
                for (int j = i + 1; j < best_nc; j++)
                    if (bc[i] > bc[j]) { int t = bc[i]; bc[i] = bc[j]; bc[j] = t; }

            for (int ci = 0; ci < best_nc; ci++) {
                if (max_results_val > 0 && result_count >= max_results_val)
                    break;
                int e = bc[ci];
                int cmk = chi_mark();
                int amk = adj_mark();
                if (place_left(best_p, e))
                    solve();
                unplace_left(best_p, e);
                chi_undo(cmk);
                adj_undo(amk);
            }
        }
    }

    /* Undo forced moves */
    for (int i = forced_count - 1; i >= 0; i--) {
        unplace_left(forced[i].p, forced[i].e);
        chi_undo(forced[i].cmk);
        adj_undo(forced[i].amk);
    }
}

/* ── generate ─────────────────────────────────────────────────── */

static int generate(int n, int max_results) {
    int m = n / 2;
    n_val = n;
    max_results_val = max_results;
    result_count = 0;
    node_count = 0;

    /* Clear state */
    memset(chi_v, 0, sizeof(chi_v));
    chi_trail_len = 0;
    memset(nbr_count, 0, sizeof(nbr_count));
    memset(nbr_data, 0, sizeof(nbr_data));
    adj_trail_len = 0;

    /* Build hn: interleave [1, m+1, 2, m+2, 3, m+3, ..., m-1, 2m-1, m] */
    hn_len = 0;
    for (int i = 1; i <= m; i++) {
        hn[hn_len++] = i;
        if (m + i <= 2 * m - 1)
            hn[hn_len++] = m + i;
    }
    /* hn_len should be n-1 */

    /* hn_signs */
    int8_t hn_signs[N_MAX+1];
    int hn_pos[N_MAX+1];
    memset(hn_signs, 0, sizeof(hn_signs));
    memset(hn_pos, 0, sizeof(hn_pos));
    for (int i = 0; i < hn_len; i++) {
        hn_signs[hn[i]] = (i % 2 == 0) ? 1 : -1;
        hn_pos[hn[i]] = i;
    }

    /* Chi for hyperline n */
    for (int a = 0; a < hn_len; a++)
        for (int b = a + 1; b < hn_len; b++)
            chi_put(n, hn[a], hn[b], hn_signs[hn[a]] * hn_signs[hn[b]]);

    /* sgn */
    memset(sgn, 0, sizeof(sgn));
    for (int p = 1; p < n; p++) {
        sgn[p][n] = 1;
        for (int q = 1; q < n; q++) {
            if (q == p) continue;
            sgn[p][q] = -chi_v[n][p][q];
        }
    }

    /* second, last */
    memset(second_el, 0, sizeof(second_el));
    memset(last_el, 0, sizeof(last_el));
    for (int p = 1; p < n; p++) {
        int pos = hn_pos[p];
        second_el[p] = hn[(pos + 1) % hn_len];
        last_el[p] = hn[((pos - 1) + hn_len) % hn_len];
    }

    /* Initialize left and rem */
    memset(left_arr, 0, sizeof(left_arr));
    memset(left_len, 0, sizeof(left_len));
    memset(rem_mask, 0, sizeof(rem_mask));
    for (int p = 1; p < n; p++) {
        left_arr[p][0] = n;
        left_arr[p][1] = second_el[p];
        left_len[p] = 2;
        /* rem = {1..n} - {p, n, second[p], last[p]} */
        uint32_t mask = 0;
        for (int q = 1; q <= n; q++) {
            if (q == p || q == n || q == second_el[p] || q == last_el[p])
                continue;
            mask |= 1u << q;
        }
        rem_mask[p] = mask;
    }

    /* Initial chi values */
    int init_ok = 1;
    for (int p = 1; p < n && init_ok; p++) {
        int s = second_el[p], l = last_el[p];
        if (!chi_put(p, s, l, sgn[p][s] * sgn[p][l])) { init_ok = 0; break; }
        uint32_t rmask = rem_mask[p];
        uint32_t m2;
        m2 = rmask;
        while (m2 && init_ok) {
            int q = __builtin_ctz(m2); m2 &= m2 - 1;
            if (!chi_put(p, s, q, sgn[p][s] * sgn[p][q])) init_ok = 0;
        }
        m2 = rmask;
        while (m2 && init_ok) {
            int q = __builtin_ctz(m2); m2 &= m2 - 1;
            if (!chi_put(p, q, l, sgn[p][q] * sgn[p][l])) init_ok = 0;
        }
    }

    if (!init_ok) {
        if (verbose)
            fprintf(stderr, "  Contradiction during initialization — no solutions exist\n");
        return 0;
    }

    /* Initialize adjacencies for hyperline n */
    for (int i = 0; i < hn_len; i++) {
        int j = (i + 1) % hn_len;
        add_adj(n, hn[i], hn[j]);
    }

    /* Each hyperline p: (n, second) and (n, last) adjacent */
    for (int p = 1; p < n; p++) {
        add_adj(p, n, second_el[p]);
        add_adj(p, n, last_el[p]);
    }

    /* Initial triangles */
    for (int p = 1; p < n; p++) {
        add_adj(second_el[p], p, n);
        add_adj(last_el[p], p, n);
    }

    /* Clear adj trail since init adjacencies are permanent */
    adj_trail_len = 0;

    /* Solve */
    solve();

    return result_count;
}

/* ── main ─────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: p3_maximal_v9c <n> [max_results]\n");
        return 1;
    }
    int n = atoi(argv[1]);
    int max_results = (argc > 2) ? atoi(argv[2]) : 0;

    if (n % 2 != 0 || n < 4 || n % 3 == 2) {
        fprintf(stderr, "Invalid n=%d: need n even, n>=4, n mod 3 != 2\n", n);
        return 1;
    }

    verbose = 1;
    int m = n / 2;
    int exp_tri = n * (n - 1) / 3;
    const char *lim = max_results ? " (limit)" : " (exhaustive)";
    printf("n=%d m=%d expected_tri=%d%s\n", n, m, exp_tri, lim);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int count = generate(n, max_results);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    printf("\n%d arrangement(s) in %.3fs (%lld nodes)\n", count, elapsed, node_count);

    /* Print results (up to 5) */
    int show = count < 5 ? count : 5;
    for (int r = 0; r < show; r++) {
        printf("\n=== #%d ===\n", r + 1);
        for (int p = 1; p <= n; p++) {
            printf("  L%d:", p);
            for (int i = 0; i < stored_hls_len[r][p]; i++)
                printf(" %d", stored_hls[r][p][i]);
            printf("\n");
        }
        /* Count triangles for this result */
        /* Reuse the stored data — build adjacency and count */
        uint32_t adj_bits[N_MAX+1][N_MAX+1];
        memset(adj_bits, 0, sizeof(adj_bits));
        for (int p = 1; p <= n; p++) {
            int len = stored_hls_len[r][p];
            for (int i = 0; i < len; i++) {
                int j = (i + 1) % len;
                int a = stored_hls[r][p][i];
                int b = stored_hls[r][p][j];
                adj_bits[p][a] |= 1u << b;
                adj_bits[p][b] |= 1u << a;
            }
        }
        int tri = 0;
        for (int i = 1; i <= n; i++)
            for (int j = i + 1; j <= n; j++)
                for (int k = j + 1; k <= n; k++) {
                    if ((adj_bits[i][j] & (1u << k)) &&
                        (adj_bits[j][i] & (1u << k)) &&
                        (adj_bits[k][i] & (1u << j)))
                        tri++;
                }
        printf("  Triangles (%d)\n", tri);
    }

    return 0;
}

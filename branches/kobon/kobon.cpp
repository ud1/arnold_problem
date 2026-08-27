#include <iostream>
#include <array>
#include <vector>
#include <sstream>
#include <cassert>

const int N = 26;
const int RES = 204;
// const int N = 18;
// const int RES = 93;
// const int N = 8;
// const int RES = 15;
const int N1 = N + 1;
const int NM1 = N - 1;
const int NM2 = N - 2;
const int MAX_STARS = 20;
const int MAX_GENS = N*(N-1)/2;
const int MAX_GENS_NM1 = MAX_GENS - NM1;
const int R = 3;
// const int R = 1000; // no limit
const int TRI_RESERVE1 = 1;
const int TRI_RESERVE2 = 5;

const int D_EXT = -1000;
const int D_EXT1 = D_EXT + 1;

// Segment-capacity budget: a sound Tamura-type bound for star words.
//
//   3*a3 <= CAP - W   for every completion of the current prefix.
//
// Every bounded segment gets a capacity: 1 if both endpoints are simple
// crossings (two triangles sharing a side need a multiple point at an
// endpoint of that side), 2 if an endpoint is a star. There are
// n(n-2) - 3*t3 bounded segments and at most 6*t3 star-ray segments, so
// total capacity is at most n(n-2) + 3*t3 <= CAP below.
//
// W counts capacity units that are lost for good:
//   cap-1 segment: +1 when BOTH adjacent faces are known non-triangles
//     (a single non-triangle verdict loses nothing: the unit may still
//     serve the face on the other side);
//   cap-2 segment: +1 per non-triangle verdict (each side has its own unit).
//
// Face verdicts are final at close. A wire segment ends exactly at a
// crossing of its wire, and the face between the two crossing wires is
// closed by that very crossing, so one verdict is known immediately; the
// segment then waits on its other face (still open: the faces directly
// above and below a wire cannot close while the wire stays in its slot).
// External faces and faces never closed count as non-triangles; faces
// still open when the word ends are simply not charged (undercount is safe).
const long CAP = (long)N * NM2 + 3 * MAX_STARS;

// v2, color-resolved budget. Checkerboard color = wall parity (consistent
// also around stars), and every bounded segment borders exactly one black
// and one white face, so each segment carries one unit per color, owned by
// the face of that color adjacent to it:
//
//   3*tri_c <= S - D_c   per color c, with S = n(n-2) - 3*t3.
//
// D_c grows at face closes, with no waiting for the other side:
//   - face of color c closes as a bounded non-triangle (d >= 2): all its
//     d+2 sides' c-units die;
//   - an external face closes: it has exactly `flanks` = d+1000 bounded
//     sides (per chain: between consecutive flanks plus the side to the
//     closing vertex; the infinite ends are unbounded), all c-units die;
//   - cross-kill: a triangle closed at a simple crossing consumes its own
//     3 c-units, and for each of its two closing sides whose other
//     endpoint is simple too, the side's anti-color unit dies now (two
//     triangles sharing a side need a multiple point at an endpoint), the
//     anti-face being the still-open flanked face.  ck[i] counts sides of
//     the open face at wall i already charged this way, so its own close
//     pays (sides - ck) and nothing is counted twice.  Star-closed
//     triangles cross-kill nothing (their closing vertex is the star:
//     mixed black+white triangle pairs on star rays are exactly the
//     domain-wall configuration and stay legal).
//
// In a perfect-like region every black triangle kills the white units of
// its sides, so the combined bound degenerates to Tamura's S/3 there.

struct Gen {
    int g;
    bool star;
};

struct Level {
    Gen gen;
    int d1;
    int d2;
    // budget backtrack checkpoint
    long w;        // saved waste
    int fc[4][3];  // saved waiter counters (c1nt, c2nt, c2t) of faces g..g+3
    bool bs[3];    // saved born_at_star of the crossed wires
    // v2 checkpoint
    long dc[2];    // saved dcol
    int cks[4];    // saved ck of faces g..g+3 (g..g+2 for sigma)
};

std::vector<Gen> gens_str_to_vec(const std::string& s) {
    std::vector<Gen> result;
    std::stringstream ss(s);
    std::string token;

    while (ss >> token) {
        Gen g;
        g.star = false;

        if (!token.empty() && token.back() == '*') {
            g.star = true;
            token.pop_back();
        }

        if (token.empty()) {
            throw std::runtime_error("Invalid string");
        }
        g.g = std::stoul(token);
        result.push_back(g);
    }

    return result;
}

struct State {
    std::array<int, N> a;
    std::array<int, N1> d;
    std::array<Level, MAX_GENS> levels;
    // budget state
    std::array<int, N1> c1nt;    // waiters: cap-1, first verdict non-triangle
    std::array<int, N1> c2nt;    // waiters: cap-2, first verdict non-triangle
    std::array<int, N1> c2t;     // waiters: cap-2, first verdict triangle
    std::array<int, N> crossed;  // crossings played per wire
    std::array<bool, N> bstar;   // wire's open segment was born at a star
    std::array<int, N1> ck;      // v2: cross-killed sides of the open face
    long dcol[2] = {0, 0};       // v2: dead units per color (= wall parity)
    int tricol[2] = {0, 0};      // v2: closed triangles per color
    long waste = 0;
    int level = -1;
    int internal_gens = 0;
    int star_count = 0;
    int tri_count = 0;
    long iter_count = 0;
    long dead_end = 0;
    long found_count = 0;
    long budget_fires = 0;   // total pruned branches (v1 + v2)
    long budget2_fires = 0;  // of them, attributed to v2 (v1 passed)

    State() {
        for (int i = 0; i < N; i++) {
            a[i] = i;
            crossed[i] = 0;
            bstar[i] = false;
        }

        for (int i = 0; i <= N; i++) {
            d[i] = D_EXT;
            c1nt[i] = 0;
            c2nt[i] = 0;
            c2t[i] = 0;
            ck[i] = 0;
        }
    }

    // v2: pay the dead units of face i closing as a non-triangle with the
    // given (pre-update) flank count; caller resets ck[i].
    void face_close_v2_pay(int i, int dval) {
        const long sides = dval < 0 ? dval + 1000 : dval + 2;
        assert(sides >= ck[i]);
        dcol[i & 1] += sides - ck[i];
    }

    // One face closes with verdict tri; pay out the lost capacity of its
    // waiting segments and reset the counters for the face reopening there.
    void face_close(int i, bool tri) {
        if (tri) {
            waste += c2nt[i];
        } else {
            waste += c1nt[i] + 2L * c2nt[i] + c2t[i];
        }
        c1nt[i] = 0;
        c2nt[i] = 0;
        c2t[i] = 0;
    }

    // A completed segment starts waiting on face f; its known verdict is
    // tri, its capacity is 2 iff cap2.
    void add_waiter(int f, bool tri, bool cap2) {
        if (cap2) {
            if (tri) c2t[f]++; else c2nt[f]++;
        } else {
            if (!tri) c1nt[f]++;
        }
    }

    void save_faces(Level &lvl, int g, int cnt) {
        for (int k = 0; k < cnt; k++) {
            lvl.fc[k][0] = c1nt[g + k];
            lvl.fc[k][1] = c2nt[g + k];
            lvl.fc[k][2] = c2t[g + k];
        }
    }

    void restore_faces(Level &lvl, int g, int cnt) {
        for (int k = 0; k < cnt; k++) {
            c1nt[g + k] = lvl.fc[k][0];
            c2nt[g + k] = lvl.fc[k][1];
            c2t[g + k] = lvl.fc[k][2];
        }
    }

    void apply_gen(Gen g) {
        Level &lvl = levels[++level];
        lvl.gen = g;
        lvl.w = waste;
        const int p = g.g;
        if (!g.star) {
            save_faces(lvl, p, 3);
            const int wa = a[p], wb = a[p + 1];
            lvl.bs[0] = bstar[wa];
            lvl.bs[1] = bstar[wb];
            lvl.dc[0] = dcol[0];
            lvl.dc[1] = dcol[1];
            for (int k = 0; k < 3; k++) lvl.cks[k] = ck[p + k];

            const bool tri = d[p + 1] == 1;
            assert(d[p + 1] != 0);
            if (tri)
                ++tri_count;

            if (d[p + 1] >= 0)
                internal_gens++;

            face_close(p + 1, tri);
            // v2: close face p+1
            if (tri) {
                tricol[(p + 1) & 1]++;
                // cross-kill: the anti-units of the two closing sides;
                // both flanked faces p and p+2 have color p&1
                if (crossed[wa] > 0 && !bstar[wa]) { dcol[p & 1]++; ck[p]++; }
                if (crossed[wb] > 0 && !bstar[wb]) { dcol[p & 1]++; ck[p + 2]++; }
            } else {
                face_close_v2_pay(p + 1, d[p + 1]);
            }
            ck[p + 1] = 0;
            // the two ending segments border the closed face and wait on
            // the flanked faces p (lower wire) and p+2 (upper wire)
            if (crossed[wa] > 0) add_waiter(p, tri, bstar[wa]);
            if (crossed[wb] > 0) add_waiter(p + 2, tri, bstar[wb]);
            crossed[wa]++;
            crossed[wb]++;
            bstar[wa] = false;
            bstar[wb] = false;

            std::swap(a[p], a[p + 1]);
            lvl.d1 = d[p + 1];
            d[p]++;
            d[p + 1] = 0;
            d[p + 2]++;
        }
        else {
            save_faces(lvl, p, 4);
            const int wa = a[p], wm = a[p + 1], wb = a[p + 2];
            lvl.bs[0] = bstar[wa];
            lvl.bs[1] = bstar[wm];
            lvl.bs[2] = bstar[wb];
            lvl.dc[0] = dcol[0];
            lvl.dc[1] = dcol[1];
            for (int k = 0; k < 4; k++) lvl.cks[k] = ck[p + k];

            const bool tri1 = d[p + 1] == 1;
            const bool tri2 = d[p + 2] == 1;
            assert(d[p + 1] != 0 && d[p + 2] != 0);
            if (tri1)
                ++tri_count;
            if (tri2)
                ++tri_count;

            if (d[p + 1] >= 0 || d[p + 2] >= 0)
                internal_gens++;

            face_close(p + 1, tri1);
            face_close(p + 2, tri2);
            // v2: both closed faces; star-closed triangles cross-kill
            // nothing (their closing vertex is the star)
            if (tri1) tricol[(p + 1) & 1]++; else face_close_v2_pay(p + 1, d[p + 1]);
            if (tri2) tricol[p & 1]++; else face_close_v2_pay(p + 2, d[p + 2]);
            ck[p + 1] = 0;
            ck[p + 2] = 0;
            // lower wire waits on face p, upper wire on face p+3; the middle
            // wire's segment borders both closed faces: verdicts complete now
            if (crossed[wa] > 0) add_waiter(p, tri1, true);
            if (crossed[wb] > 0) add_waiter(p + 3, tri2, true);
            if (crossed[wm] > 0) waste += (tri1 ? 0 : 1) + (tri2 ? 0 : 1);
            crossed[wa] += 2;
            crossed[wm] += 2;
            crossed[wb] += 2;
            bstar[wa] = true;
            bstar[wm] = true;
            bstar[wb] = true;

            std::swap(a[p], a[p + 2]);
            lvl.d1 = d[p + 1];
            lvl.d2 = d[p + 2];
            d[p]++;
            d[p + 1] = 0;
            d[p + 2] = 0;
            d[p + 3]++;
            star_count++;
        }
    }

    void revert_gen() {
        Level &lvl = levels[level];
        Gen g = lvl.gen;
        const int p = g.g;
        if (!g.star) {
            std::swap(a[p], a[p + 1]);
            d[p]--;
            d[p + 1] = lvl.d1;
            d[p + 2]--;

            if (d[p + 1] == 1) {
                --tri_count;
                tricol[(p + 1) & 1]--;
            }

            const int wa = a[p], wb = a[p + 1];
            crossed[wa]--;
            crossed[wb]--;
            bstar[wa] = lvl.bs[0];
            bstar[wb] = lvl.bs[1];
            restore_faces(lvl, p, 3);
            for (int k = 0; k < 3; k++) ck[p + k] = lvl.cks[k];

            if (d[p + 1] >= 0)
                internal_gens--;
        }
        else {
            std::swap(a[p], a[p + 2]);
            d[p]--;
            d[p + 1] = lvl.d1;
            d[p + 2] = lvl.d2;
            d[p + 3]--;
            star_count--;

            if (d[p + 1] == 1) {
                --tri_count;
                tricol[(p + 1) & 1]--;
            }
            if (d[p + 2] == 1) {
                --tri_count;
                tricol[p & 1]--;
            }

            const int wa = a[p], wm = a[p + 1], wb = a[p + 2];
            crossed[wa] -= 2;
            crossed[wm] -= 2;
            crossed[wb] -= 2;
            bstar[wa] = lvl.bs[0];
            bstar[wm] = lvl.bs[1];
            bstar[wb] = lvl.bs[2];
            restore_faces(lvl, p, 4);
            for (int k = 0; k < 4; k++) ck[p + k] = lvl.cks[k];

            if (d[p + 1] >= 0 || d[p + 2] >= 0)
                internal_gens--;
        }
        waste = lvl.w;
        dcol[0] = lvl.dc[0];
        dcol[1] = lvl.dc[1];
        --level;
    }

    bool budget_feasible() {
        return CAP - waste >= 3L * RES;
    }

    // v2: per-color unit budget; S_up uses the current star count (more
    // stars only shrink the segment count, so this over-estimates safely)
    bool budget2_feasible() {
        const long s_up = (long)N * NM2 - 3L * star_count;
        const long av1 = s_up - dcol[1] - 3L * tricol[1];
        const long av0 = s_up - dcol[0] - 3L * tricol[0];
        assert(av0 >= 0 && av1 >= 0);
        return (long)RES - tri_count <= av0 / 3 + av1 / 3;
    }

    bool can_apply_gen(Gen g) {
        if (!g.star && a[g.g] > a[g.g + 1]) {
            return false;
        }

        if (g.star && (a[g.g] > a[g.g + 1] || a[g.g + 1] > a[g.g + 2])) {
            return false;
        }

        if (g.star && star_count >= MAX_STARS) {
            return false;
        }

        // Trailing-digon pruning
        if (!g.star) {
            if (g.g % 2 == 1 && a[g.g] + 1 == a[g.g + 1] && g.g + 1 + a[g.g + 1] != N)
                return false;
        }
        else {
        }

        // no external stars
        if (g.star && (d[g.g + 1] < 0 || d[g.g + 2] < 0)) {
            return false;
        }

        if (!g.star) {
            if (d[g.g + 1] >= 0) {
                bool genTri = (d[g.g + 1] == 1) || (d[g.g] == 0) || (d[g.g + 2] == 0);
                if (!genTri) {
                    return false;
                }
            }
        }
        else {
            if (d[g.g + 1] >= 0 || d[g.g + 2] >= 0) {
                int genTriCount = (d[g.g + 1] == 1 ? 1 : 0) + (d[g.g + 2] == 1 ? 1 : 0) + (d[g.g] == 0 ? 1 : 0) + (d[g.g + 3] == 0 ? 1 : 0);
                if (genTriCount < 2)
                    return false;
            }
        }

        // No quads
        if (!g.star && d[g.g + 1] == 2) {
            return false;
        }
        if (g.star && (d[g.g + 1] == 2 || d[g.g + 2] == 2)) {
            return false;
        }
        // no external quads
        if (!g.star && d[g.g + 1] == D_EXT1) {
            return false;
        }
        if (g.star && (d[g.g + 1] == D_EXT1 || d[g.g + 2] == D_EXT1)) {
            return false;
        }

        if (level > 0) {
            // int tris = 0;
            // if (!g.star && d[g.g + 1] == 1) {
            //     ++tris;
            // }
            // if (g.star && d[g.g + 1] == 1) {
            //     ++tris;
            // }
            // if (g.star && d[g.g + 2] == 1) {
            //     ++tris;
            // }
            // if (level > 25)
            {
                int expected_tris = (level + star_count * 2 + 1) * RES / MAX_GENS;
                if (expected_tris > tri_count + TRI_RESERVE2)
                    return false;
            }
            //
            //
            int expected_tris = (internal_gens + star_count * 2) * RES / MAX_GENS_NM1;
            if (expected_tris > tri_count + TRI_RESERVE1)
                return false;
        }
        //
        // if (level*2 < MAX_GENS && g.star)
        // {
        //     return false;
        // }

        return true;
    }

    // forced descend
    bool can_move_right(int prev_gen) {
        int cur_line = a[prev_gen];

        for (int i = prev_gen + 1; i < N; i++) {
            if (a[i] > cur_line) {
                return true;
            }
        }

        for (int i = 0; i < prev_gen; i++) {
            if (a[i] != NM1 - i) {
                return false;
            }
        }

        return true;
    }

    int apply_gen_next(Gen new_g) {
        ++iter_count;
        if (can_apply_gen(new_g)) {
            apply_gen(new_g);

            bool ok = budget_feasible();

            if (ok && !budget2_feasible()) {
                ok = false;
                ++budget2_fires;
            }

            if (ok) {
                next_level();
            } else {
                ++budget_fires;
            }
            revert_gen();
            return 1;
        }
        return 0;
    }

    void print(bool progress) {
        if (progress) {
            std::cout << "PROGRESS | ";
        }
        else {
            ++found_count;
        }
        std::cout << iter_count << " " << tri_count << " |";
        for (int l = 0; l <= level; l++) {
            Level &lvl = levels[l];
            std::cout << " " << lvl.gen.g;
            if (lvl.gen.star) {
                std::cout << "*";
            }
        }
        std::cout << std::endl;
    }

    void next_level() {
        Level &lvl = levels[level];

        if (level + star_count * 2 + 1 == MAX_GENS) {
            if (tri_count >= RES) {
                print(false);
            }
            return;
        }

        Gen new_g;
        int applied = 0;
        bool cmr = can_move_right(lvl.gen.g);
        if (cmr) {
            int imax = lvl.gen.g + R + (lvl.gen.star ? 1 : 0);
            if (star_count < MAX_STARS) {
                new_g.star = true;
                for (int i = lvl.gen.g + 2; i < NM2 && i < imax; i++) {
                    new_g.g = i;
                    applied += apply_gen_next(new_g);
                }
            }

            new_g.star = false;
            for (int i = lvl.gen.g + 2; i < NM1 && i < imax; i++) {
                new_g.g = i;
                applied += apply_gen_next(new_g);
            }

            new_g.g = lvl.gen.g + 1;
            if (star_count < MAX_STARS) {
                new_g.star = true;
                applied += apply_gen_next(new_g);
            }
            new_g.star = false;
            applied += apply_gen_next(new_g);
        }

        if (lvl.gen.g >= 2 && star_count < MAX_STARS) {
            new_g.g = lvl.gen.g - 2;
            new_g.star = true;
            applied += apply_gen_next(new_g);
        }

        if (lvl.gen.g >= 1) {
            new_g.g = lvl.gen.g - 1;
            new_g.star = false;
            applied += apply_gen_next(new_g);
        }

        if (applied == 0) {
            // print(true);
            dead_end++;
            if (dead_end % 10000000 == 0)
            {
                // print(true);
            }
        }
    }

    void search() {
        Gen new_g;
        new_g.star = false;
        new_g.g = 0;
        apply_gen_next(new_g);
    }

    void searchAll() {
        for (int g = 0; g < NM1; g++) {
            Gen new_g;
            new_g.star = false;
            new_g.g = g;
            apply_gen_next(new_g);
        }

        for (int g = 0; g < NM2; g++) {
            Gen new_g;
            new_g.star = true;
            new_g.g = g;
            apply_gen_next(new_g);
        }
    }

    void searchFromStartGens() {
        std::vector<Gen> gens = gens_str_to_vec("0 2 4 6 8 10 9 8 7 6 5 4 3 2 1 0 2 4 6 8 10 12 11 10 9 8 7 6 5 4 3 2 4 6 8 10 12 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 2 4 6 8 7 6 8");
        for (int i = 0; i < gens.size() - 1; i++) {
            apply_gen(gens[i]);
        }

        apply_gen_next(gens[gens.size() - 1]);
    }
};

int main() {
    State s;
    s.search();
    std::cout << s.iter_count << " " << s.found_count << std::endl;
}

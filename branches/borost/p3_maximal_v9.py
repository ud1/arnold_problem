#!/usr/bin/env python3
"""
Generate p3-maximal pseudoline arrangements with n = 2m elements.

Algorithm from Bokowski-Roudneff-Strempel, Section 4:
All n-1 hyperlines are built SIMULTANEOUSLY as partial circular sequences.
At each step, the most constrained hyperline is chosen (fewest candidates),
and candidates are tried with backtracking. Each placement sets chi values
that propagate constraints to other hyperlines.

Key optimizations:
- Left-only extension (no redundant left/right paths to same sequence)
- Unit propagation (forced moves made without branching)
- Cross-hyperline constraint propagation via shared chi values
- Array-based chi storage for O(1) access
- Adjacency tracking: each adjacency creates a triangle, constraining 2 other hyperlines
"""
import sys, time


class Chi:
    __slots__ = ['v', 'trail']

    def __init__(self, n):
        sz = n + 1
        self.v = [[[0] * sz for _ in range(sz)] for _ in range(sz)]
        self.trail = []

    def get(self, i, j, k):
        val = self.v[i][j][k]
        return val if val != 0 else None

    def put(self, i, j, k, val):
        cur = self.v[i][j][k]
        if cur != 0:
            return cur == val
        self.v[i][j][k] = val
        self.v[i][k][j] = -val
        self.v[j][i][k] = -val
        self.v[j][k][i] = val
        self.v[k][i][j] = val
        self.v[k][j][i] = -val
        self.trail.append((i, j, k))
        return True

    def mark(self):
        return len(self.trail)

    def undo(self, mark):
        v = self.v
        while len(self.trail) > mark:
            i, j, k = self.trail.pop()
            v[i][j][k] = 0
            v[i][k][j] = 0
            v[j][i][k] = 0
            v[j][k][i] = 0
            v[k][i][j] = 0
            v[k][j][i] = 0


def extract_tri(hls, n):
    adj = {}
    for p, seq in hls.items():
        s = set()
        for i in range(len(seq) - 1):
            s.add((min(seq[i], seq[i + 1]), max(seq[i], seq[i + 1])))
        s.add((min(seq[-1], seq[0]), max(seq[-1], seq[0])))
        adj[p] = s
    tri = set()
    for i in range(1, n + 1):
        for j in range(i + 1, n + 1):
            for k in range(j + 1, n + 1):
                if ((min(j, k), max(j, k)) in adj[i] and
                        (min(i, k), max(i, k)) in adj[j] and
                        (min(i, j), max(i, j)) in adj[k]):
                    tri.add((i, j, k))
    return tri


def generate(n, verbose=False, max_results=0):
    assert n % 2 == 0 and n >= 4 and n % 3 != 2
    m = n // 2
    exp_tri = n * (n - 1) // 3

    chi = Chi(n)

    hn = []
    for i in range(1, m + 1):
        hn.append(i)
        if m + i <= 2 * m - 1:
            hn.append(m + i)
    assert len(hn) == n - 1

    hn_signs = {}
    for i, e in enumerate(hn):
        hn_signs[e] = (-1) ** i
    hn_pos = {e: i for i, e in enumerate(hn)}

    for a in range(len(hn)):
        for b in range(a + 1, len(hn)):
            chi.put(n, hn[a], hn[b], hn_signs[hn[a]] * hn_signs[hn[b]])

    sgn = [[0] * (n + 1) for _ in range(n + 1)]
    for p in range(1, n):
        sgn[p][n] = 1
        for q in range(1, n):
            if q == p:
                continue
            sgn[p][q] = -chi.get(n, p, q)

    second = [0] * (n + 1)
    last = [0] * (n + 1)
    for p in range(1, n):
        pos = hn_pos[p]
        second[p] = hn[(pos + 1) % len(hn)]
        last[p] = hn[(pos - 1) % len(hn)]

    left = [None] * (n + 1)
    rem_sets = [None] * (n + 1)
    for p in range(1, n):
        left[p] = [n, second[p]]
        rem_sets[p] = set(range(1, n + 1)) - {p, n, second[p], last[p]}

    # Set initial chi values
    init_ok = True
    for p in range(1, n):
        sp = sgn[p]
        s, l = second[p], last[p]
        if not chi.put(p, s, l, sp[s] * sp[l]):
            init_ok = False
            break
        for q in rem_sets[p]:
            if not chi.put(p, s, q, sp[s] * sp[q]):
                init_ok = False
                break
        if not init_ok:
            break
        for q in rem_sets[p]:
            if not chi.put(p, q, l, sp[q] * sp[l]):
                init_ok = False
                break
        if not init_ok:
            break

    if not init_ok:
        if verbose:
            print("  Contradiction during initialization — no solutions exist")
        return []

    # Adjacency tracking: nbr[q][x] = set of known neighbors of x in hyperline q
    # Using a 2D array of small sets. Each element has at most 2 neighbors.
    # nbr_count[q][x] = len(nbr[q][x]) for fast access
    nbr_count = [[0] * (n + 1) for _ in range(n + 1)]
    # nbr_data[q][x] = [neighbor1, neighbor2] (at most 2)
    nbr_data = [[[0, 0] for _ in range(n + 1)] for _ in range(n + 1)]
    adj_trail = []

    def add_adj(q, a, b):
        """Record a,b adjacent in hyperline q. Returns False if >2 neighbors."""
        # Check if already recorded
        nc_a = nbr_count[q][a]
        nd_a = nbr_data[q][a]
        a_has_b = (nc_a > 0 and nd_a[0] == b) or (nc_a > 1 and nd_a[1] == b)

        nc_b = nbr_count[q][b]
        nd_b = nbr_data[q][b]
        b_has_a = (nc_b > 0 and nd_b[0] == a) or (nc_b > 1 and nd_b[1] == a)

        if a_has_b and b_has_a:
            return True  # already recorded

        if not a_has_b:
            if nc_a >= 2:
                return False
            nd_a[nc_a] = b
            nbr_count[q][a] = nc_a + 1
            adj_trail.append((q, a))

        if not b_has_a:
            if nc_b >= 2:
                return False
            nd_b[nc_b] = a
            nbr_count[q][b] = nc_b + 1
            adj_trail.append((q, b))

        return True

    def adj_mark():
        return len(adj_trail)

    def adj_undo(mark):
        while len(adj_trail) > mark:
            q, x = adj_trail.pop()
            nbr_count[q][x] -= 1

    # Initialize adjacencies
    # Hyperline n: all adjacent pairs from hn
    for i in range(len(hn)):
        j = (i + 1) % len(hn)
        add_adj(n, hn[i], hn[j])

    # Each hyperline p: (n, second) and (n, last) adjacent
    for p in range(1, n):
        add_adj(p, n, second[p])
        add_adj(p, n, last[p])

    # Initial triangles: (p, n, second[p]) → p,n adj in hyperline second[p]
    #                     (p, n, last[p]) → p,n adj in hyperline last[p]
    for p in range(1, n):
        add_adj(second[p], p, n)  # p and n adj in hyperline second[p]
        add_adj(last[p], p, n)    # p and n adj in hyperline last[p]

    # Clear the trail since init adjacencies are permanent
    adj_trail.clear()

    results = []
    nodes = [0]
    chi_v = chi.v

    def can_add_adj(q, a, b):
        """Check if add_adj(q,a,b) would succeed (without modifying state)."""
        nc_a = nbr_count[q][a]
        nd_a = nbr_data[q][a]
        a_has_b = (nc_a > 0 and nd_a[0] == b) or (nc_a > 1 and nd_a[1] == b)
        if not a_has_b and nc_a >= 2:
            return False
        nc_b = nbr_count[q][b]
        nd_b = nbr_data[q][b]
        b_has_a = (nc_b > 0 and nd_b[0] == a) or (nc_b > 1 and nd_b[1] == a)
        if not b_has_a and nc_b >= 2:
            return False
        return True

    def get_left_candidates(p):
        sp = sgn[p]
        rp = rem_sets[p]
        f = left[p][-1]  # current frontier
        cands = []
        for e in rp:
            # Pre-filter: triangle (p,f,e) must be feasible
            if not (can_add_adj(p, f, e) and can_add_adj(f, p, e) and can_add_adj(e, p, f)):
                continue
            # Chi ordering check: no remaining predecessor
            se = sp[e]
            ok = True
            for x in rp:
                if x == e:
                    continue
                v = chi_v[p][x][e]
                if v != 0 and v == sp[x] * se:
                    ok = False
                    break
            if ok:
                cands.append(e)
        return cands

    def apply_triangle(p, a, b):
        """Record triangle (p,a,b): a,b adj in hl p; p,b adj in hl a; p,a adj in hl b."""
        if not add_adj(p, a, b):
            return False
        if not add_adj(a, p, b):
            return False
        if not add_adj(b, p, a):
            return False
        return True

    def place_left(p, e):
        sp = sgn[p]
        se = sp[e]
        lp = last[p]
        f = left[p][-1]
        rem_sets[p].discard(e)
        left[p].append(e)

        for x in left[p]:
            if x == e:
                continue
            if not chi.put(p, x, e, sp[x] * se):
                return False
        if not chi.put(p, lp, e, -(sp[lp] * se)):
            return False
        for x in rem_sets[p]:
            if not chi.put(p, x, e, -(sp[x] * se)):
                return False

        # Triangle from adjacency (f, e) in hyperline p
        if not apply_triangle(p, f, e):
            return False

        # Gap closed: adjacency (e, last) in hyperline p
        if not rem_sets[p]:
            if not apply_triangle(p, e, lp):
                return False

        return True

    def unplace_left(p, e):
        rem_sets[p].add(e)
        left[p].pop()

    def solve():
        if max_results > 0 and len(results) >= max_results:
            return
        nodes[0] += 1
        if verbose and nodes[0] % 10000 == 0:
            done = sum(1 for p in range(1, n) if not rem_sets[p])
            total_rem = sum(len(rem_sets[p]) for p in range(1, n))
            print(f"  nodes={nodes[0]} complete={done}/{n - 1} rem={total_rem} results={len(results)}")

        forced = []
        contradiction = False

        while not contradiction:
            found_forced = False
            for p in range(1, n):
                if not rem_sets[p]:
                    continue
                cands = get_left_candidates(p)
                if len(cands) == 0:
                    contradiction = True
                    break
                if len(cands) == 1:
                    cmk = chi.mark()
                    amk = adj_mark()
                    e = cands[0]
                    forced.append((p, e, cmk, amk))
                    if not place_left(p, e):
                        contradiction = True
                    else:
                        found_forced = True
                    break
            if not found_forced:
                break

        if not contradiction:
            incomplete = []
            for p in range(1, n):
                if not rem_sets[p]:
                    continue
                cands = get_left_candidates(p)
                if len(cands) == 0:
                    contradiction = True
                    break
                incomplete.append((p, cands))

        if not contradiction:
            if not incomplete:
                hls = {n: hn[:]}
                for p in range(1, n):
                    hls[p] = left[p] + [last[p]]
                tri = extract_tri(hls, n)
                if len(tri) == exp_tri:
                    results.append(hls)
                    if verbose:
                        print(f"  FOUND #{len(results)} ({nodes[0]} nodes, {len(tri)} tri)")
            else:
                best_p, best_cands = min(incomplete, key=lambda x: len(x[1]))
                for e in sorted(best_cands):
                    if max_results > 0 and len(results) >= max_results:
                        break
                    cmk = chi.mark()
                    amk = adj_mark()
                    if place_left(best_p, e):
                        solve()
                    unplace_left(best_p, e)
                    chi.undo(cmk)
                    adj_undo(amk)

        for (p, e, cmk, amk) in reversed(forced):
            unplace_left(p, e)
            chi.undo(cmk)
            adj_undo(amk)

    solve()
    if verbose:
        print(f"  Total nodes: {nodes[0]}")
    return results


def main():
    if len(sys.argv) < 2:
        print("Usage: p3_maximal_v9.py <n> [max_results]")
        return
    n = int(sys.argv[1])
    max_results = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    if n % 2 != 0 or n < 4 or n % 3 == 2:
        print(f"Invalid n={n}: need n even, n>=4, n mod 3 != 2")
        return
    m = n // 2
    lim = f" (limit {max_results})" if max_results else " (exhaustive)"
    print(f"n={n} m={m} expected_tri={n * (n - 1) // 3}{lim}")
    t0 = time.time()
    r = generate(n, verbose=True, max_results=max_results)
    el = time.time() - t0
    print(f"\n{len(r)} arrangement(s) in {el:.1f}s")
    for i, h in enumerate(r[:5]):
        print(f"\n=== #{i + 1} ===")
        for l in sorted(h):
            print(f"  L{l}: {h[l]}")
        t = extract_tri(h, n)
        print(f"  Triangles ({len(t)})")


if __name__ == '__main__':
    main()

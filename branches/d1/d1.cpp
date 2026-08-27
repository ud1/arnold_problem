#include <iostream>
#include <array>
#include <vector>
#include <optional>
#include <bit>
#include <assert.h>
#include <ctime>
#include <tuple>
#include <memory>
#include <algorithm>

// 5 +
// 7 b4
// 9 +
// 11 -
// 13 b4
// 15 -
// 17 +
// 19 b4
// 21 +
// 23 -
// 25 b4
// 27 -
// 29 +
// 31 b4
// 33 +
// 35 -
// 37 b4

const unsigned N = 1;
const unsigned N1 = N - 1;
const unsigned N2 = N - 2;
const unsigned NM = (N + 1)/2;
const unsigned NM2 = N + 1;

const int TOT_GENS = (N + 2) * (N + 1)/2;
const int MID_GENS = (N + 1) / 2 + 2 * N;
const int HALF_GENS = (TOT_GENS - MID_GENS) / 2;
const int START_GENS = (N - 1) / 2;
const int MAX_GENS = HALF_GENS - START_GENS;
const int MAX_EVEN_GENS = (MAX_GENS) / 3;

const u_int8_t NONE_GEN = 250;

struct Prev {
    std::array<u_int8_t, 3> d;
    u_int8_t gen;
};

struct State {
    std::array<u_int8_t, N> lines;
    std::array<u_int8_t, N> line_intersection_count;
    // Два байта для каждой линии, в первом - число линий меньше текущей слева от текущей, второй число линий больше текущей справа от текущей
    std::array<u_int16_t, N> lines_lr;
    std::array<u_int8_t, N + 1> d;
    std::array<Prev, MAX_EVEN_GENS> prev;
    std::array<u_int8_t, MAX_EVEN_GENS * (N / 2)> available_gens;
    int prev_size;
    int available_gens_size;

    State() {
        prev_size = 0;
        available_gens_size = 0;

        for (u_int8_t i = 0; i < N; ++i) {
            lines[i] = i;
            line_intersection_count[i] = 0;
        }

        calc_lr();

        for (int i = 0; i < N + 1; ++i) {
            d[i] = 2;
        }

        for (int i = 1; i < N - 1; i += 2) {
            modify_lr_single_gen(i);
        }

        for (int i = 2; i <= N - 3; i += 2) {
            if (is_valid_gen(i))
                available_gens[available_gens_size++] = i;
        }

        validate_lr();
    }

    void apply_gen(u_int8_t i) {
        if (i > 0) {
            assert(lines[i] < lines[i + 1]);
            assert(lines[i] < lines[i + 2]);
            assert(lines[i - 1] < lines[i + 1]);
        }
        else
        {
            assert(lines[i] < lines[i + 1]);
            assert(lines[i] < lines[i + 2]);
        }

        prev[prev_size++] = {{d[i], d[i + 1], d[i + 2]}, i};
        modify_lr(i);
    }

    void revert_last_gen() {
        Prev p = prev[prev_size - 1];
        --prev_size;

        u_int8_t i = p.gen;
        if (i > 0) {
            std::tie(lines_lr[i - 1], lines_lr[i], lines_lr[i + 1], lines_lr[i + 2]) =
                    std::tuple(lines_lr[i] - 1, lines_lr[i + 2] - 2, lines_lr[i - 1] + 2*256, lines_lr[i + 1] + 1*256);
            d[i - 1]--;

            std::tie(lines[i - 1], lines[i], lines[i + 1], lines[i + 2]) =
                    std::tuple(lines[i], lines[i + 2], lines[i - 1], lines[i + 1]);

            line_intersection_count[lines[i - 1]]--;
            line_intersection_count[lines[i]] -= 2;
            line_intersection_count[lines[i + 1]] -= 2;
            line_intersection_count[lines[i + 2]]--;
        }
        else {
            std::tie(lines_lr[i], lines_lr[i + 1], lines_lr[i + 2]) =
                    std::tuple(lines_lr[i + 2] - 2, lines_lr[i] + 1*256, lines_lr[i + 1] + 1*256);

            std::tie(lines[i], lines[i + 1], lines[i + 2]) =
                    std::tuple(lines[i + 2], lines[i], lines[i + 1]);

            line_intersection_count[lines[i]] -= 2;
            line_intersection_count[lines[i + 1]]--;
            line_intersection_count[lines[i + 2]]--;
        }

        std::tie(d[i], d[i + 1], d[i + 2])
                = std::tuple(p.d[0], p.d[1], p.d[2]);
        d[i + 3]--;

//        validate_lr();
    }


    void calc_lr() {
        for (int l = 0; l < N; ++l) {
            int c1 = 0;
            int c2 = 0;

            for (int i = 0; i < l; ++ i) {
                if (lines[i] < lines[l])
                    ++c1;
            }

            for (int i = l + 1; i < N; ++ i) {
                if (lines[l] < lines[i])
                    ++c2;
            }

            lines_lr[l] = (c1 * 256) + (N - 1 - c2);
        }
    }

    void validate_lr() {
        for (int l = 0; l < N; ++l) {
            int c1 = 0;
            int c2 = 0;

            for (int i = 0; i < l; ++ i) {
                if (lines[i] < lines[l])
                    ++c1;
            }

            for (int i = l + 1; i < N; ++ i) {
                if (lines[l] < lines[i])
                    ++c2;
            }

            if (lines_lr[l] >> 8 != c1) {
                std::cout << "ERR" << std::endl;
            }

            int r = lines_lr[l] & 255;
            if ((r) != (N - 1 - c2)) {
                std::cout << "ERR" << std::endl;
            }
        }
    }

    void revert_applied_to(size_t count) {
        assert(prev_size >= count);
        while (prev_size > count) {
            revert_last_gen();
        }
    }

    void revert_available_to(size_t count) {
        assert(available_gens_size >= count);
        available_gens_size = count;
    }

    void modify_lr_single_gen(u_int8_t i) {
        std::tie(lines_lr[i], lines_lr[i + 1]) =
                std::tuple(lines_lr[i + 1] - 256, lines_lr[i] + 1);

        d[i]++;
        d[i+1] = 0;
        d[i+2]++;

        line_intersection_count[lines[i]]++;
        line_intersection_count[lines[i + 1]]++;
        std::tie(lines[i], lines[i + 1]) =
                std::tuple(lines[i + 1], lines[i]);
    }

    void modify_lr(u_int8_t i) {
        assert(i % 2 == 0);
        assert(i >= 0);
        assert(i <= N - 3);
        if (i > 0) {
            std::tie(lines_lr[i - 1], lines_lr[i], lines_lr[i + 1], lines_lr[i + 2]) =
                    std::tuple(lines_lr[i + 1] - 2*256, lines_lr[i - 1] + 1, lines_lr[i + 2] - 1*256, lines_lr[i] + 2);

            d[i-1]++;
            d[i] = 0;
            d[i+1] = 2;
            d[i+2] = 0;
            d[i+3]++;

            line_intersection_count[lines[i - 1]]++;
            line_intersection_count[lines[i]] += 2;
            line_intersection_count[lines[i + 1]] += 2;
            line_intersection_count[lines[i + 2]]++;

            std::tie(lines[i - 1], lines[i], lines[i + 1], lines[i + 2]) =
                    std::tuple(lines[i + 1], lines[i - 1], lines[i + 2], lines[i]);
        }
        else {
            std::tie(lines_lr[i], lines_lr[i + 1], lines_lr[i + 2]) =
                    std::tuple(lines_lr[i + 1] - 1*256, lines_lr[i + 2] - 1*256, lines_lr[i] + 2);

            d[i]++;
            d[i+1] = 1;
            d[i+2] = 0;
            d[i+3]++;

            line_intersection_count[lines[i]] += 2;
            line_intersection_count[lines[i + 1]]++;
            line_intersection_count[lines[i + 2]]++;

            std::tie(lines[i], lines[i + 1], lines[i + 2]) =
                    std::tuple(lines[i + 1], lines[i + 2], lines[i]);
        }

//        validate_lr();
    }

    bool is_valid_gen(u_int8_t g) {
        if (g <= 1)
            return false;

        if (this->d[g + 1] < 3) // without defects, disallow white triangles and squares
            return false;

        if (g == 0) {
            bool res = (this->lines_lr[1]) == 256 + N - 1; // генератор 0 может пересечь только прямые 0 и N-1

            //            bool res2 = s.lines[g] < s.lines[g + 1] &&
            //                        s.lines[g] < s.lines[g + 2];
            //
            //            if (res != res2) {
            //                std::cout << "ERR " << std::endl;
            //            }

            return res;
        }

        bool res = this->lines_lr[g] < this->lines_lr[g + 1] &&
                this->lines_lr[g] + 257 < this->lines_lr[g + 2] &&
                this->lines_lr[g - 1] + 257 < this->lines_lr[g + 1];

        //        bool res2 = s.lines[g] < s.lines[g + 1] &&
        //                    s.lines[g] < s.lines[g + 2] &&
        //                    s.lines[g - 1] < s.lines[g + 1];
        //
        //        if (res != res2) {
        //            std::cout << "ERR " << std::endl;
        //        }

        if (res) {
            // Прямые i и i+1 могут пересечься только в самом конце, в генераторе под номером i.
            if (this->lines[g] + 1 == this->lines[g + 2] && (g + this->lines[g] != (N - 3)))
                return false;

            if (this->lines[g - 1] + 1 == this->lines[g + 1] && (g + this->lines[g - 1] != (N - 1)))
                return false;
        }

        if (lines[g] + lines[g + 1] == NM2)
            return false;

        if (lines[g - 1] + lines[g + 1] == NM2)
            return false;

        if (lines[g] + lines[g + 2] == NM2)
            return false;

        if (lines[g - 1] > 1 && line_intersection_count[lines[g - 1]] + line_intersection_count[NM2 - lines[g - 1]] >= N2)
            return false;

        if (lines[g] > 1 && line_intersection_count[lines[g]] + line_intersection_count[NM2 - lines[g]] >= N1)
            return false;

        if (lines[g + 1] > 1 && line_intersection_count[lines[g + 1]] + line_intersection_count[NM2 - lines[g + 1]] >= N1)
            return false;

        if (lines[g + 2] > 1 && line_intersection_count[lines[g + 2]] + line_intersection_count[NM2 - lines[g + 2]] >= N2)
            return false;

        return res;
    }
};

struct Level {
    u_int64_t gens_mask; // единичные биты задают номера примененных available gens
    u_int64_t max_gens_mask;
    u_int64_t gens_mask_rnd;
    size_t available_gens_start_ind;
    size_t available_gens_end_ind;
    size_t applied_gens_start_ind;
    size_t applied_gens_end_ind;
    u_int8_t parent_min_changed_line;
    u_int8_t parent_max_changed_line;

    static Level first_level(State &s) {
        Level res;
        res.gens_mask = 0;
        res.max_gens_mask = 1ul << s.available_gens_size;
//        res.gens_mask_rnd = (rand() % res.max_gens_mask) | (1ul << (N - 5)/2);
        res.gens_mask_rnd = (1ul << (N - 5)/2);
        res.available_gens_start_ind = 0;
        res.available_gens_end_ind = s.available_gens_size;
        res.applied_gens_start_ind = 0;
        res.parent_min_changed_line = 0;
        res.parent_max_changed_line = N - 1;
        res.next_gens(s);
        res.applied_gens_end_ind = s.prev_size;
        return res;
    }

    std::optional<Level> next_level(State &s) {
        s.revert_applied_to(applied_gens_end_ind);
        s.revert_available_to(available_gens_end_ind);

        u_int8_t last_ind = NONE_GEN;
        for (size_t i = applied_gens_start_ind; i < applied_gens_end_ind; ++i) {
            u_int8_t g = s.prev[i].gen;

            // Генераторы след уровня могут только на -2/+2 отличаться от генераторов пред уровня
            if (g > 1 && last_ind != g - 2 && s.is_valid_gen(g - 2)) {
                s.available_gens[s.available_gens_size++] = (g - 2);
            }

            if (g < N - 4 && s.is_valid_gen(g + 2)) {
                s.available_gens[s.available_gens_size++] = (g + 2);
                last_ind = g + 2;
            }
        }

        size_t count = s.available_gens_size - available_gens_end_ind;
        if (!count)
            return std::nullopt;

        Level res;
        res.gens_mask = 0;
        res.max_gens_mask = (1ul << count);
        // if (count == 1)
            res.gens_mask_rnd = 0;
        // else
            // res.gens_mask_rnd = rand() % res.max_gens_mask;
        res.available_gens_start_ind = available_gens_end_ind;
        res.available_gens_end_ind = s.available_gens_size;
        res.applied_gens_start_ind = applied_gens_end_ind;
        res.parent_min_changed_line = std::max(0, s.prev[applied_gens_start_ind].gen - 1);
        res.parent_max_changed_line = std::min((u_int8_t) (N - 1), (u_int8_t) (s.prev[applied_gens_end_ind - 1].gen + 2));
        if (res.next_gens(s)) {
            return res;
        }
        return std::nullopt;
    }



    bool next_gens(State &s) {
        s.revert_available_to(available_gens_end_ind);
        s.revert_applied_to(applied_gens_start_ind);
        while (++gens_mask <= max_gens_mask) {
            if (apply_gens(s, (gens_mask - 1) ^ gens_mask_rnd)) {
                applied_gens_end_ind = s.prev_size;
                return true;
            }
        }
        return false;
    }

    bool apply_gens(State &s, u_int64_t gens_mask) {
//        if (std::popcount(gens_mask) < (available_gens_end_ind - available_gens_start_ind) / 5)
//            return false;

        if (gens_mask == 0)
            return false;

        if (available_gens_start_ind == 0)
        {
            if (!(gens_mask & (1ul << (N - 5)/2)))
                return false;
        }
        u_int8_t last_gen = NONE_GEN;
        u_int8_t min_gen;
        int j = -1;
        for (size_t i = available_gens_start_ind; i < available_gens_end_ind; ++i) {
            ++j;

            if ((gens_mask & (1ul << j)) == 0)
                continue;

            u_int8_t g = s.available_gens[i];
            if (g == last_gen + 2) {
                return false;
            }

            if (last_gen == NONE_GEN)
                min_gen = g;
            last_gen = g;
        }

        // Если слева от минимального генератора есть линии, которые не можем пересечь с правой стороны, отбрасываем
        for (int i = parent_min_changed_line; i < min_gen - 1; ++i) {
            if (s.lines_lr[i] > 256 && (s.lines_lr[i] & 255) == (N - 1))
                return false;
        }

        // Если справа от максимального генератора есть линии, которые не можем пересечь с левой стороны, отбрасываем
        for (int i = last_gen + 3; i <= parent_max_changed_line; ++i) {
            if (s.lines_lr[i] > 0 && s.lines_lr[i] < N - 1)
                return false;
        }

        j = -1;
        for (size_t i = available_gens_start_ind; i < available_gens_end_ind; ++i) {
            ++j;
            if (gens_mask & (1ul << j)) {
                u_int8_t g = s.available_gens[i];
                s.apply_gen(g);
            }
        }

        return true;
    }
};

struct Search {
    State state;
    Level levels[N * (N + 1) / 2];
    int min_remaining;
    int cur_level;
    int stop_level;
    long iters = 0;
    long *count;

    void init(int branch, long *count) {
        levels[0] = Level::first_level(state);
        cur_level = 0;
        stop_level = -1;
        this->count = count;
    }

    void search() {
        assert(cur_level >= 0);
        min_remaining = MAX_EVEN_GENS;
        int dead_end_iters = 0;
        while (true) {
            ++iters;
            auto next = levels[cur_level].next_level(state);
            if (next.has_value()) {
                levels[++cur_level] = next.value();
                continue;
            }

            ++dead_end_iters;
            int remaining_gens = MAX_EVEN_GENS - state.prev_size;
            if (min_remaining > remaining_gens)
                min_remaining = remaining_gens;

            if (state.prev_size == MAX_EVEN_GENS)
            {
                bool success = true;
                for (int i = 2; i < N - 2; i += 2) {
                    int sum = state.lines[i] + state.lines[i + 1];
                    if (sum != (N + 1)) {
                        success = false;
                        break;
                    }
                }

                if (success) {
                    (*count)++;
                    int res = (N - 1) / 2;
                    for (int i = 0; i < state.prev_size; ++i) {
                        u_int8_t g = state.prev[i].gen;
                        if (g > 0)
                            res++;
                    }

                    std::vector<int> result;
                    for (int i = 1; i < N - 1; i += 2) {
                        result.push_back(i);
                    }

                    for (int i = 0; i < state.prev_size; ++i) {
                        u_int8_t g = state.prev[i].gen;
                        result.push_back(g);
                        if (g > 0)
                            result.push_back(g - 1);
                        result.push_back(g + 1);
                    }

                    for (int i = N + 1; i --> 1;) {
                        result.push_back(i);
                    }

                    for (auto i : result) {
                        std::cout << i << " ";
                    }

                    std::cout << "0 ";

                    for (int i = 3; i < N + 1; i += 2) {
                        std::cout << i << " ";
                    }

                    for (auto it = result.rbegin(); it != result.rend(); ++it) {
                        std::cout << *it << " ";
                    }

                    // std::cout << " | " << (int) res << " | processed = " << *count << " | iter = " << iters << std::endl;
                    // for (auto l : state.lines) {
                    //     std::cout << (int) l << " ";
                    // }
                    std::cout << std::endl;
                }
            }

            while (true) {
                if (cur_level == stop_level) {
                    cur_level = -1;
                    return;
                }
                if (levels[cur_level].next_gens(state)) {
                    break;
                }

                cur_level--;
                if (cur_level < 0) {
                    return;
                }
            }

            if (dead_end_iters == 1000)
                return;
        }
    }

    void next_on_level(int level) {
        cur_level = level;

        while (true) {
            if (cur_level == stop_level) {
                cur_level = -1;
                return;
            }

            if (levels[cur_level].next_gens(state)) {
                break;
            }

            cur_level--;
            if (cur_level < 0) {
                return;
            }
        }

        search();
    }

    void setStopOnLevel(int level) {
        if (level > stop_level)
            stop_level = level;
    }
};

int main() {
    srand(time(0));

    long count = 0;

    Search search;
    search.init(0, &count);

    long iter = 0;
    long replacedModif = 0;
    long replaced = 0;

    while (true)
    {
        ++iter;
        search.search();

        if (search.cur_level < 0) {
            return 0;
        }

        if (iter % 100000 == 0)
        {
            std::cout << iter << " | PROGRESS | " << search.min_remaining << " |RMODIF " << replacedModif << " |RPL " << replaced << " | " << std::endl;
        }
    }
}

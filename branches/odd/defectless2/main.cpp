#include <iostream>
#include <array>
#include <vector>
#include <optional>
#include <assert.h>

const unsigned N = 21;

const int MAX_GENS = N * (N - 1) / 2;
const int MAX_EVEN_GENS = (MAX_GENS - (N - 1) / 2 + 1) / 3;

const u_int8_t NONE_GEN = 250;

struct Prev {
    std::array<u_int8_t, 3> d;
    u_int8_t gen;
};

struct State {
    std::array<u_int8_t, N> lines;
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
            lines_lr[i] = i * 256 + i;
        }

        validate_lr();

        for (int i = 0; i < N + 1; ++i) {
            d[i] = 2;
        }

        for (int i = 1; i < N - 1; i += 2) {
            modify_lr_single_gen(i);
        }

        for (int i = 2; i <= N - 3; i += 2) {
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
        }
        else {
            std::tie(lines_lr[i], lines_lr[i + 1], lines_lr[i + 2]) =
                    std::tuple(lines_lr[i + 2] - 2, lines_lr[i] + 1*256, lines_lr[i + 1] + 1*256);

            std::tie(lines[i], lines[i + 1], lines[i + 2]) =
                    std::tuple(lines[i + 2], lines[i], lines[i + 1]);
        }

        std::tie(d[i], d[i + 1], d[i + 2])
                = std::tuple(p.d[0], p.d[1], p.d[2]);
        d[i + 3]--;

//        validate_lr();
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

            std::tie(lines[i], lines[i + 1], lines[i + 2]) =
                    std::tuple(lines[i + 1], lines[i + 2], lines[i]);
        }

//        validate_lr();
    }
};

struct Level {
    u_int64_t gens_mask; // единичные биты задают номера примененных available gens
    u_int64_t max_gens_mask;
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
            if (g > 1 && last_ind != g - 2 && is_valid_gen(s, g - 2)) {
                s.available_gens[s.available_gens_size++] = (g - 2);
            }

            if (g < N - 4 && is_valid_gen(s, g + 2)) {
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

    bool is_valid_gen(State &s, u_int8_t g) {
        if (s.d[g + 1] < 3) // without defects, disallow white triangles and squares
            return false;

        if (g == 0) {
            bool res = (s.lines_lr[1]) == 256 + N - 1; // генератор 0 может пересечь только прямые 0 и N-1

//            bool res2 = s.lines[g] < s.lines[g + 1] &&
//                        s.lines[g] < s.lines[g + 2];
//
//            if (res != res2) {
//                std::cout << "ERR " << std::endl;
//            }

            return res;
        }

        bool res = s.lines_lr[g] < s.lines_lr[g + 1] &&
                   s.lines_lr[g] + 257 < s.lines_lr[g + 2] &&
                   s.lines_lr[g - 1] + 257 < s.lines_lr[g + 1];

//        bool res2 = s.lines[g] < s.lines[g + 1] &&
//                    s.lines[g] < s.lines[g + 2] &&
//                    s.lines[g - 1] < s.lines[g + 1];
//
//        if (res != res2) {
//            std::cout << "ERR " << std::endl;
//        }

        if (res) {
            // Прямые i и i+1 могут пересечься только в самом конце, в генераторе под номером i.
            if (s.lines[g] + 1 == s.lines[g + 2] && (g + s.lines[g] != (N - 3)))
                return false;

            if (s.lines[g - 1] + 1 == s.lines[g + 1] && (g + s.lines[g - 1] != (N - 1)))
                return false;
        }
        return res;
    }

    bool next_gens(State &s) {
        s.revert_available_to(available_gens_end_ind);
        s.revert_applied_to(applied_gens_start_ind);
        while (++gens_mask < max_gens_mask) {
            if (apply_gens(s, gens_mask)) {
                applied_gens_end_ind = s.prev_size;
                return true;
            }
        }
        return false;
    }

    bool apply_gens(State &s, u_int64_t gens_mask) {
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

int main() {
    State state;
    Level levels[N * (N + 1) / 2];
    levels[0] = Level::first_level(state);

    long count = 0;

    int cur_level = 0;
    int min_level = 0;
    long iters = 0;

    while (true) {
        if (iters % 100000000 == 0)
        {
            std::cout << iters << " | PROGRESS | ";
            for (int i = 1; i < N - 1; i += 2) {
                std::cout << i << " ";
            }

            for (int i = 0; i < state.prev_size; ++i) {
                u_int8_t g = state.prev[i].gen;
                std::cout << (int) g << " ";
                if (g > 0)
                    std::cout << (int) (g - 1) << " ";
                std::cout << (int) (g + 1) << " ";
            }

            std::cout << " | L " << cur_level << " | ";

            double progress = 0.0;
            double part = 1.0;
            for (int i = 0; i <= cur_level; ++i)
            {
                std::cout << levels[i].gens_mask << "/" << levels[i].max_gens_mask << " ";
                part /= (double) (levels[i].max_gens_mask - 1);
                progress += levels[i].gens_mask * part;
            }

            std::cout << " | PRC " << (progress * 100.0);

            std::cout << std::endl;
        }
        ++iters;
        auto next = levels[cur_level].next_level(state);
        if (next.has_value()) {
            levels[++cur_level] = next.value();
            continue;
        }

        if (state.prev_size == MAX_EVEN_GENS)
        {
            ++count;

//            for (int i = 0; i <= cur_level; ++i)
//            {
//                std::cout << levels[i].gens_mask << " ";
//            }
//            std::cout << std::endl;

            int res = (N - 1) / 2;
            for (int i = 0; i < state.prev_size; ++i) {
                u_int8_t g = state.prev[i].gen;
                if (g > 0)
                    res++;
            }

//            if (res >= max_res)
            {
                for (int i = 1; i < N - 1; i += 2) {
                    std::cout << i << " ";
                }

                for (int i = 0; i < state.prev_size; ++i) {
                    u_int8_t g = state.prev[i].gen;
                    std::cout << (int) g << " ";
                    if (g > 0)
                        std::cout << (int) (g - 1) << " ";
                    std::cout << (int) (g + 1) << " ";
                }
                std::cout << " | " << (int) res << " | processed = " << count << " | iter = " << iters << std::endl;
            }
        }

        while (true) {
            if (levels[cur_level].next_gens(state))
                break;

            cur_level--;
            if (cur_level < min_level) {
                std::cout << "processed = " << count << std::endl;
                std::cout << "iters = " << iters << std::endl;
                return 0;
            }
        }
    }

    return 0;
}

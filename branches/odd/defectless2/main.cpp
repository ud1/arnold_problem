#include <iostream>
#include <optional>
#include <bit>
#include <array>
#include <vector>
#include <sstream>

u_int64_t next_set_of_n_elements(u_int64_t x)
{
    u_int64_t smallest, ripple, new_smallest, ones;

    if (x == 0)
        return 0;
    smallest     = (x & -x);
    ripple       = x + smallest;
    new_smallest = (ripple & -ripple);
    ones         = ((new_smallest/smallest) >> 1) - 1;
    return ripple | ones;
}

// Возвращает числа в порядке возрастания числа единичных бит
struct BitSetGenerator
{
    unsigned max_bits;
    unsigned max_val;
    unsigned current_bits = 0;
    u_int64_t num = 0;

    explicit BitSetGenerator(unsigned int maxBits) : max_bits(maxBits) {
        max_val = (1ul << maxBits) - 1;
    }

    bool next() {
        if (current_bits > max_bits)
            return false;

        if (current_bits == 0 || (num = next_set_of_n_elements(num)) > max_val) {
            ++current_bits;
            num = (1ul << current_bits) - 1;
        }

        return current_bits <= max_bits;
    }

    u_int64_t current() {
        return num;
    }
};

const unsigned N = 23;

struct State {
    std::array<int, N> lines;
    std::array<int, N> lines_l; // для каждой линии количество линий слева от нее с порядковым номером меньше текущей
    std::array<int, N> lines_r; // для каждой линии количество линий справа от нее с порядковым номером больше текущей
    std::array<int, N + 1> d;
    std::vector<int> applied_gens;
    std::vector<int> old_d;
    std::vector<std::array<int, 4>> old_lines_l_r;

    void apply_gen(int i) {
        applied_gens.push_back(i);
        old_d.push_back(d[i+1]);
        std::swap(lines[i], lines[i + 1]);
        d[i]++;
        d[i+1] = 0;
        d[i+2]++;

        int li = lines_l[i];
        int li1 = lines_l[i + 1];
        lines_l[i] = li1 - 1;
        lines_l[i + 1] = li;

        int ri = lines_r[i];
        int ri1 = lines_r[i + 1];
        lines_r[i] = ri1;
        lines_r[i + 1] = ri - 1;

        old_lines_l_r.push_back({li, li1, ri, ri1});
    }

    void revert_last_gen() {
        size_t ind = applied_gens.size() - 1;
        int i = applied_gens[ind];
        std::swap(lines[i], lines[i + 1]);
        d[i]--;
        d[i+1] = old_d[ind];
        d[i+2]--;

        std::array<int, 4> &rl = old_lines_l_r[ind];
        lines_l[i] = rl[0];
        lines_l[i + 1] = rl[1];
        lines_r[i] = rl[2];
        lines_r[i + 1] = rl[3];

        applied_gens.pop_back();
        old_d.pop_back();
        old_lines_l_r.pop_back();
    }

    bool compute_is_final() {
        for (int i = 0; i < N - 1; ++i) {
            if (lines[i] < lines[i + 1]) {
                return false;
            }
        }

        return true;
    }

    int gens_count() {
        return applied_gens.size();
    }

    void revert_to(int count) {
        int num_to_revert = gens_count() - count;
        for (int i = 0; i < num_to_revert; ++i)
            revert_last_gen();
    }
};

// Генераторы сгруппированы по уровням, уровень содержит либо только четные, либо только нечетные генераторы.
// Четность зависит от порядкового номера уровня.
// Каждый следующий уровень может содержать только те генераторы, которые +1/-1 генераторов текущего уровня.
// Для нечетных генераторов перебора нет, они применяются все всегда, т.е. каждому четному генератору предыдущего уровня
//  соответствует два нечетных -1/+1 генератора, кроме случая, когда четный генератор был нулевой.
struct Level {
    u_int64_t available_gens;
    u_int64_t euristic; // Не особо полезно, помогает, но совсем мизер
    u_int64_t mandatory_gens;
    int applied_gens_count;
    BitSetGenerator bits = BitSetGenerator(0);
    int level;

    static Level level1(State &s) {
        Level res;
        res.level = 1;
        for (int i = 0; i < N; ++i) {
            s.lines[i] = i;
            s.lines_l[i] = i;
            s.lines_r[i] = (int) N - 1 - i;
        }
        for (int i = 0; i < N + 1; ++i) {
            s.d[i] = 2;
        }

        res.update_available_gens(s);
        res.bits = BitSetGenerator {(unsigned ) std::popcount(res.available_gens)};
        res.applied_gens_count = 0;
        res.mandatory_gens = 0ul;
        res.euristic = 0ul;
        return res;
    }

    std::optional<Level> next_level(bool &is_final, State &s) {
        s.revert_to(applied_gens_count);

        Level res;
        res.level = level + 1;
        u_int64_t gens = actual_gens();
        for (int i = 0; i < N - 1; ++i) {
            if (gens & (1ul << i)) {
                s.apply_gen(i);
            }
        }

        is_final = s.compute_is_final();

        res.update_available_gens(s);
        u_int64_t m = (gens << 1 | gens >> 1);

        int ods = res.level % 2;
        if (ods && res.available_gens != m)
            return std::nullopt;
        res.available_gens &= m;
        res.euristic &= m;
        res.update_mandatory_gens(s);
        unsigned available_gens_count = (unsigned ) std::popcount(res.available_gens);
        if (available_gens_count == 0)
            return std::nullopt;
        res.bits = BitSetGenerator {available_gens_count};
        while (!res.is_valid_gens(res.actual_gens(), s)) {
            if (!res.bits.next())
                return std::nullopt;
        }
        res.applied_gens_count = s.gens_count();
        return res;
    }

    bool next(State &s) {
        s.revert_to(applied_gens_count);

        int ods = level % 2;
        if (ods)
            return false;

        while (bits.next()) {
            if (!is_valid_gens(actual_gens(), s))
                continue;
            return true;
        }

        return false;
    }

    bool is_valid_gens(u_int64_t gens, State &s) {
        if (gens == 0)
            return false;

        int ods = level % 2;
        if (ods)
            return true;

        if ((gens & mandatory_gens) != mandatory_gens)
            return false;

        // Первая линия может пересекать только последнюю
        if ((gens & 1) && s.lines[1] != (N - 1))
            return false;

        // Если справа от максимального генератора есть линия с номером меньше, чем у любой другой линии левее нее, и
        // справа от этой линии есть линия с номером больше чем у нее, то конф не валидна.
        // Т.е. отбрасываем комбинации в который есть линия, которую мы не можем пересечь слева, а перебор делается слева,
        // и справа от нее линии еще не упорядочены.
        for (int i = N - 1; i --> 0;) {
            if (gens & (1ul << i)) {
                break;
            }

            if (s.lines_l[i + 1] == 0 && s.lines_r[i + 1] > 0)
                return false;
        }

        // Аналогично для минимального генератора
        for (int i = 0; i < N - 2; ++i) {
            if (gens & (1ul << i)) {
                break;
            }

            if (s.lines_l[i] > 0 && s.lines_r[i] == 0)
                return false;
        }

        return ((gens << 2) & gens) == 0 && ((gens >> 2) & gens) == 0; // defectless
    }

    void update_available_gens(State &s) {
        available_gens = 0;
        euristic = 0;
        int ods = level % 2;
        for (int i = 0; i < N - 1; ++i) {
            if (i % 2 == ods) {
                if (s.lines[i] < s.lines[i + 1]) {
                    if (!ods) {
                        if (s.d[i] == 1 || s.d[i + 1] == 1) // disallow black squares
                            continue;

                        if (s.d[i + 1] < 3) // without defects, disallow white triangles and squares
                            continue;

                        if (s.d[i + 1] > 4)
                            euristic |= (1ul << i);
                    }

                    available_gens |= (1ul << i);
                }
            }
        }

        // Для нулевого генератора первая линия может пересекать только последнюю
        if (available_gens & 1 && (s.lines[0] != 0 || s.lines[1] != (N - 1)))
            available_gens--;
    }

    void update_mandatory_gens(State &s)
    {
        mandatory_gens = 0ul;

        // Фиксируем предпоследний генератор на втором уровне, чтобы зафиксировать начальное вращение
        if (level == 2)
            mandatory_gens = (1ul << (N - 3));
    }

    u_int64_t actual_gens() {
        int ods = level % 2;
        if (ods)
            return available_gens;

        u_int64_t res = 0;
        int mi = 0;
        for (int i = 0; i < N; ++i) {
            if (available_gens & (1ul << i)) {
                if (bits.num & (1ul << mi)) {
                    res |= (1ul << i);
                }
                ++mi;
            }
        }

        res = res ^ euristic;
        return res;
    }

    Level next_level_with_gens(State &s, u_int64_t gens) {
        bool is_final;
        std::optional<Level> res = next_level(is_final, s);
        if (!res.has_value())
            throw std::runtime_error("E1 No specified combination");

        while(true) {
            if (res.value().actual_gens() == gens)
                return res.value();

            if (!res.value().next(s))
            {
                throw std::runtime_error("E2 No specified combination");
            }
        }
    }
};

u_int64_t gen(const std::vector<int> &l) {
    u_int64_t res = 0;

    for (auto v : l)
        res |= (1ul << v);

    return res;
}

int main()
{
    State state;
    Level levels[N * (N + 1) / 2];
    levels[0] = Level::level1(state);

    long count = 0;
//    int max_res = -1;
    int max_res = 39;

    int cur_level = 0;
    int min_level = 0;
    long iters = 0;

    {
        // Продолжение обсчета начиная с указанных генераторов
//        std::istringstream iss("1 3 5 7 9 11 13 15 17 19 21 8 12 20");
//
//        int g;
//        bool is_odd = true;
//        std::vector<int> level_gens;
//        int level = 1;
//        while (iss >> g) {
//            bool current_is_odd = g % 2 == 1;
//
//            if (current_is_odd != is_odd) {
//                if (level > 1 && !level_gens.empty()) {
//                    levels[cur_level + 1] = levels[cur_level].next_level_with_gens(state, gen(level_gens));
//                    ++cur_level;
//                }
//                is_odd = current_is_odd;
//                ++level;
//                level_gens.clear();
//            }
//
//            level_gens.push_back(g);
//        }
//
//        levels[cur_level + 1] = levels[cur_level].next_level_with_gens(state, gen(level_gens));
//        ++cur_level;

        // останавливать обсчет, если начальные генераторы больше не соответствуют переданным
//        min_level = level;
    }

    while (true) {
        if (iters % 10000000 == 0)
        {
            if (iters % 100000000 == 0)
            {
                std::cout << iters << " | PROGRESS | ";
                for (auto v : state.applied_gens) {
                    std::cout << v << " ";
                }
                std::cout << std::endl;
            }
            else {
                std::cout << iters << std::endl;
            }
        }
        ++iters;
        bool is_final;
        auto next = levels[cur_level].next_level(is_final, state);
        if (next.has_value()) {
            levels[++cur_level] = next.value();
            continue;
        }

        if (is_final) {
            ++count;

            int res = 0;
            for (auto g : state.applied_gens) {
                if (g % 2)
                    ++res;
                else
                    --res;
            }

            if (res >= max_res)
            {
                max_res = res;
                for (auto v : state.applied_gens) {
                    std::cout << v << " ";
                }
                std::cout << " | " << res << " | processed = " << count << " | iter = " << iters << std::endl;
            }
        }

        while (true) {
            if (levels[cur_level].next(state))
                break;

            cur_level--;
            if (cur_level < min_level) {
                std::cout << "processed = " << count << std::endl;
                std::cout << "iters = " << iters << std::endl;
                return 0;
            }
        }
    }
}

// Позволяет вывести список из нескольких начальных генераторов, чтобы можно было ориентироваться и оценить прогресс
int main2()
{
    State state;
    Level levels[N * (N + 1) / 2];
    levels[0] = Level::level1(state);

    int cur_level = 0;
    long iters = 0;

    std::vector<std::vector<int>> lines;
    long count = 0;
    while (true) {
        bool is_final;
        if (cur_level < 8) {
            auto next = levels[cur_level].next_level(is_final, state);
            if (next.has_value()) {
                levels[++cur_level] = next.value();
                continue;
            }
        }

        lines.push_back(state.applied_gens);
        ++count;
        while (true) {
            if (levels[cur_level].next(state))
                break;

            cur_level--;
            if (cur_level < 0) {
                double i = 0;
                for (auto &l : lines)
                {
                    ++i;
                    for (auto v : l) {
                        std::cout << v << " ";
                    }
                    std::cout << " | " << (i * 100.0 / lines.size()) << "%" << std::endl;
                }

                std::cout << count << std::endl;
                return 0;
            }
        }
    }
}
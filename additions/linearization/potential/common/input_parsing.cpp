#include "common/input_parsing.h"

#include <cctype>
#include <fstream>
#include <sstream>

bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::vector<size_t> parse_gens_numbers(const std::string& s) {
    std::vector<size_t> out;
    std::stringstream ss(s);
    size_t val = 0;
    while (ss >> val) out.push_back(val);
    return out;
}

std::string trim_copy(const std::string& s) {
    size_t l = 0;
    while (l < s.size() && std::isspace((unsigned char)s[l])) ++l;
    size_t r = s.size();
    while (r > l && std::isspace((unsigned char)s[r - 1])) --r;
    return s.substr(l, r - l);
}

bool is_numbers_only_line(const std::string& s) {
    bool has_digit = false;
    for (char c : s) {
        if (std::isdigit((unsigned char)c)) {
            has_digit = true;
            continue;
        }
        if (!std::isspace((unsigned char)c)) return false;
    }
    return has_digit;
}

std::vector<size_t> extract_nonnegative_ints(const std::string& s) {
    std::vector<size_t> out;
    unsigned long long cur = 0;
    bool in_num = false;
    for (char c : s) {
        if (std::isdigit((unsigned char)c)) {
            in_num = true;
            cur = cur * 10ULL + (unsigned long long)(c - '0');
        } else if (in_num) {
            out.push_back((size_t)cur);
            cur = 0;
            in_num = false;
        }
    }
    if (in_num) out.push_back((size_t)cur);
    return out;
}

bool read_next_filter_case(std::istream& in, FilterCaseStreamState& state, std::vector<size_t>& out) {
    out.clear();
    std::string line;

    while (std::getline(in, line)) {
        size_t close_paren_pos = line.find(')');
        if (close_paren_pos != std::string::npos) {
            if (!state.current.empty()) {
                out = std::move(state.current);
                state.current.clear();
                state.header_case_active = true;
                std::string tail = line.substr(close_paren_pos + 1);
                auto nums = extract_nonnegative_ints(tail);
                state.current.insert(state.current.end(), nums.begin(), nums.end());
                return true;
            }

            state.header_case_active = true;
            std::string tail = line.substr(close_paren_pos + 1);
            auto nums = extract_nonnegative_ints(tail);
            state.current.insert(state.current.end(), nums.begin(), nums.end());
            continue;
        }

        if (state.header_case_active) {
            auto nums = extract_nonnegative_ints(line);
            state.current.insert(state.current.end(), nums.begin(), nums.end());
            continue;
        }

        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) continue;
        if (is_numbers_only_line(trimmed)) {
            out = extract_nonnegative_ints(trimmed);
            if (!out.empty()) return true;
        }
    }

    if (!state.current.empty()) {
        out = std::move(state.current);
        state.current.clear();
        state.header_case_active = false;
        return true;
    }

    return false;
}

std::vector<std::vector<size_t>> parse_filter_cases_from_stream(std::istream& in) {
    std::vector<std::vector<size_t>> cases;
    FilterCaseStreamState state;
    std::vector<size_t> next_case;
    while (read_next_filter_case(in, state, next_case)) {
        cases.push_back(std::move(next_case));
    }
    return cases;
}

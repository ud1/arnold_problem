#pragma once

#include <string>
#include <istream>
#include <vector>

bool read_file(const std::string& path, std::string& out);

std::vector<size_t> parse_gens_numbers(const std::string& s);
std::string trim_copy(const std::string& s);
bool is_numbers_only_line(const std::string& s);
std::vector<size_t> extract_nonnegative_ints(const std::string& s);

struct FilterCaseStreamState {
    std::vector<size_t> current;
    bool header_case_active = false;
};

bool read_next_filter_case(std::istream& in, FilterCaseStreamState& state, std::vector<size_t>& out);
std::vector<std::vector<size_t>> parse_filter_cases_from_stream(std::istream& in);

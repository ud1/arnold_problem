#ifndef ARN_UTIL_DB_HPP
#define ARN_UTIL_DB_HPP

#include "conf.hpp"
#include "run_config.hpp"

enum class AddType {
    E, P, RAW
};

void add_to_db(const std::vector<Configuration> &configurations, AddType tp);
void process(const RunConfig &run_command, size_t concurrency);

#endif //ARN_UTIL_DB_HPP

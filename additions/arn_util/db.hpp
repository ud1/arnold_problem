#ifndef ARN_UTIL_DB_HPP
#define ARN_UTIL_DB_HPP

#include "conf.hpp"
#include "run_config.hpp"

void add_to_db(const std::vector<Configuration> &configurations);
void process(const RunConfig &run_command);

#endif //ARN_UTIL_DB_HPP

#ifndef ARN_UTIL_RUN_CONFIG_HPP
#define ARN_UTIL_RUN_CONFIG_HPP

#include <string>
#include <vector>

struct RunConfig {
    std::string name;
    std::vector<std::string> command;
};

std::vector<RunConfig> load_configs();

#endif //ARN_UTIL_RUN_CONFIG_HPP

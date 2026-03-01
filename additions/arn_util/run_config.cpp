#include <fstream>
#include <iostream>
#include "run_config.hpp"
#include "json.hpp"

using json = nlohmann::json;

std::vector<RunConfig> load_configs() {
    std::vector<RunConfig> result;

    std::ifstream f("arn_utils_config.json");
    if (!f) {
        std::cerr << "Could not open file arn_utils_config.json" << std::endl;
        return result;
    }
    json data = json::parse(f);

    auto runs = data["runs"];
    if (!runs.is_null()) {
        if (!runs.is_array()) {
            std::cerr << "Invalid runs config" << std::endl;
            return result;
        }

        for (auto r : runs) {
            RunConfig config;
            config.name = r["name"];
            config.command = r["command"];
            if (config.name.empty() || config.command.empty()) {
                std::cerr << "Invalid run config" << std::endl;
                return result;
            }
            result.push_back(config);
        }
    }

    return result;
}
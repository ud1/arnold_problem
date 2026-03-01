#include <iostream>
#include <optional>
#include <fstream>
#include <unordered_set>

#include "argparse.hpp"
#include "conf.hpp"
#include "db.hpp"
#include "graceful_stop.hpp"
#include "run_config.hpp"

struct Params {
    std::string ifile;
    size_t line_num = 0;
    Line rotate = 0;
    bool mirror = false;
    bool uniq = false;
    bool add = false;
    std::string run;
    std::vector<std::string> print;
};

std::vector<Line> gens_str_to_vec(const std::string& s) {
    std::vector<Line> numbers;
    std::stringstream ss(s);
    Line temp_int;

    while (ss >> temp_int) {
        numbers.push_back(temp_int);
    }

    return numbers;
}

int main(int argc, char **argv) {
    init_graceful_stop();

    argparse::ArgumentParser program("arn");
    Params params;

    program.add_argument("-i", "--ifile")
            .help("input file")
            .store_into(params.ifile);

    program.add_argument("-l", "--line")
            .help("line number (starts from 1)")
            .store_into(params.line_num);

    program.add_argument("-r", "--rotate")
            .help("rotate configuration")
            .store_into(params.rotate);

    program.add_argument("-m", "--mirror")
            .help("mirror configuration")
            .flag()
            .store_into(params.mirror);

    program.add_argument("-u", "--uniq")
            .help("filter unique configurations")
            .flag()
            .store_into(params.uniq);

    program.add_argument("-a", "--add")
            .help("add configurations to DB")
            .flag()
            .store_into(params.add);

    program.add_argument("-p", "--print")
            .help("print (gens, omatrix, n, k, pos, wire, wire_condensed, eid, lf)")
            .nargs(1, 20)
            .store_into(params.print);

    program.add_argument("--run")
            .help("run processing of configurations in DB")
            .store_into(params.run);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::vector<Configuration> configurations;

    if (!params.ifile.empty()) {
        std::ifstream file(params.ifile);
        if (!file) {
            std::cerr << "Could not open file " << params.ifile << std::endl;
            return -1;
        }

        std::string line;
        int i = 0;
        while (std::getline(file, line) && !is_stopped()) {
            ++i;

            if (params.line_num > 0 && i != params.line_num)
                continue;

            std::vector<Line> gens = gens_str_to_vec(line);
            if (params.line_num > 0 && gens.empty())
            {
                std::cerr << "Line is empty" << std::endl;
                return -1;
            }

            configurations.emplace_back(i, gens);
            if (params.line_num > 0)
                break;
        }
    }

    if (params.uniq) {
        std::unordered_set<OMatrixPtr, OMatrixPtrHash, OMatrixPtrEqual> o_set;
        std::vector<Configuration> new_configurations;
        for (auto &conf : configurations) {
            auto min_o = conf.get_min_o();
            if (min_o) {
                if (!o_set.count(min_o)) {
                    o_set.insert(min_o);
                    new_configurations.push_back(conf);
                }
            }
        }
        new_configurations.swap(configurations);
    }

    for (auto &conf : configurations) {
        if (is_stopped())
            return 0;

        if (!conf.o) {
            std::cout << "#INVALID CONFIGURATION" << std::endl;
            continue;
        }

        bool matrix_updated = false;

        if (params.mirror) {
            conf.o = conf.o->mirror();
            matrix_updated = true;
        }

        if (params.rotate > 0) {
            auto new_o = conf.o->rotate(params.rotate);
            if (!new_o) {
                std::cout << "#INVALID ROTATION" << std::endl;
                continue;
            }
            conf.o = new_o;
            matrix_updated = true;
        }

        if (matrix_updated) {
            conf.gens = conf.o->get_generators();
        }

        bool need_space = false;
        for (auto &print_cmd : params.print) {
            if (print_cmd == "gens") {
                for (auto g : conf.gens) {
                    if (need_space) {
                        std::cout << " ";
                    }
                    std::cout << g;
                    need_space = true;
                }
                std::cout << std::endl;
                need_space = false;
            }
            else if (print_cmd == "omatrix") {
                print_OMatrix(conf.o);
                need_space = false;
            }
            else if (print_cmd == "n") {
                if (need_space)
                    std::cout << " ";

                std::cout << "[n=" << conf.o->n() << "]";
                need_space = true;
            }
            else if (print_cmd == "k") {
                if (need_space)
                    std::cout << " ";

                std::cout << "[k=" << (conf.o->k()) << "]";
                need_space = true;
            }
            else if (print_cmd == "pos") {
                if (need_space)
                    std::cout << " ";
                std::cout << "#" << conf.line_number;
                need_space = true;
            }
            else if (print_cmd == "eid") {
                if (need_space)
                    std::cout << " ";
                std::cout << conf.o->get_eid();
                need_space = true;
            }
            else if (print_cmd == "lf") {
                std::cout << std::endl;
                need_space = false;
            }
            else if (print_cmd == "wire_condensed") {
                print_wire_condensed(conf.gens);
                std::cout << std::endl;
                need_space = false;
            }
            else if (print_cmd == "wire") {
                print_wire(conf.gens);
                std::cout << std::endl;
                need_space = false;
            }
        }
    }

    if (params.add) {
        add_to_db(configurations);
    }

    if (!params.run.empty()) {
        auto run_configs = load_configs();
        bool found = false;
        for (auto &run_conf : run_configs) {
            if (run_conf.name == params.run) {
                found = true;
                process(run_conf);
            }
        }
        if (!found) {
            std::cerr << "Run " << params.run << " not found" << std::endl;
            return -1;
        }
    }

    return 0;
}



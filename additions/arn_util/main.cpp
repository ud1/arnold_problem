#include <iostream>
#include <fstream>
#include <unordered_set>
#include <set>

#include "argparse.hpp"
#include "conf.hpp"
#include "db.hpp"
#include "graceful_stop.hpp"
#include "run_config.hpp"

struct Params {
    std::string ifile;
    std::string gens;
    size_t line_num = 0;
    size_t concurrency = 1;
    Line rotate = 0;
    Line sphere_rotate = 0;
    bool mirror = false;
    bool uniq = false;
    bool sph_uniq = false;
    bool move_last_to_inf = false;
    bool add = false;
    std::string run;
    std::vector<std::string> print;
    std::string remove_lines;
};

struct Processor {
    Params params;
    std::set<Line> remove_lines;
    std::unordered_set<OMatrixPtr, OMatrixPtrHash, OMatrixPtrEqual> o_set;
    std::vector<Configuration> confs_buf;

    Processor(const Params &params, const std::set<Line> &remove_lines)
        : params(params),
          remove_lines(remove_lines) {
    }

    void process(Configuration &&conf) {
        if (params.sph_uniq) {
            auto min_o = conf.get_min_po();
            if (min_o) {
                if (!o_set.count(min_o)) {
                    o_set.insert(min_o);
                }
                else {
                    return;
                }
            }
        }
        else if (params.uniq) {
            auto min_o = conf.get_min_o();
            if (min_o) {
                if (!o_set.count(min_o)) {
                    o_set.insert(min_o);
                }
                else {
                    return;
                }
            }
        }

        if (is_stopped())
            return;

        if (!conf.o) {
            std::cout << "#INVALID CONFIGURATION" << std::endl;
            return;
        }

        bool matrix_updated = false;

        if (!remove_lines.empty()) {
            conf.o = conf.o->remove_lines(remove_lines);
            matrix_updated = true;
        }

        if (params.mirror) {
            conf.o = conf.o->mirror();
            matrix_updated = true;
        }

        if (params.move_last_to_inf) {
            auto new_o = conf.o->move_last_line_to_infinity();
            if (!new_o) {
                std::cout << "#INVALID MOVE TO INFINITY COMMAND" << std::endl;
                return;
            }
            conf.o = new_o;
            matrix_updated = true;
        }

        if (params.sphere_rotate > 0) {
            auto new_o = conf.o->sphere_rotation(params.sphere_rotate);
            if (!new_o) {
                std::cout << "#INVALID SPHERE ROTATION" << std::endl;
                return;
            }
            conf.o = new_o;
            matrix_updated = true;
        }

        if (params.rotate > 0) {
            auto new_o = conf.o->rotate(params.rotate);
            if (!new_o) {
                std::cout << "#INVALID ROTATION" << std::endl;
                return;
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
            else if (print_cmd == "pid") {
                if (need_space)
                    std::cout << " ";
                std::cout << conf.o->get_pid();
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

        if (params.add) {
            confs_buf.push_back(conf);
            if (confs_buf.size() > 1000) {
                add_to_db(confs_buf);
                confs_buf.clear();
            }
        }
    }

    void flush_db() {
        if (confs_buf.size() > 0) {
            add_to_db(confs_buf);
            confs_buf.clear();
        }
    }
};

int main(int argc, char **argv) {
    init_graceful_stop();

    argparse::ArgumentParser program("arn");
    Params params;

    program.add_argument("-i", "--ifile")
            .help("input file")
            .store_into(params.ifile);

    program.add_argument("-g", "--gens")
            .help("input generators")
            .store_into(params.gens);

    program.add_argument("-l", "--line")
            .help("line number (starts from 1)")
            .store_into(params.line_num);

    program.add_argument("-r", "--rotate")
            .help("rotate configuration")
            .store_into(params.rotate);

    program.add_argument("-sr", "--sphere_rotate")
            .help("rotate configuration on sphere")
            .store_into(params.sphere_rotate);

    program.add_argument("-rm", "--remove_lines")
            .help("space-separated list of lines to remove")
            .store_into(params.remove_lines);

    program.add_argument("-m", "--mirror")
            .help("mirror configuration")
            .flag()
            .store_into(params.mirror);

    program.add_argument("-u", "--uniq")
            .help("filter unique configurations")
            .flag()
            .store_into(params.uniq);

    program.add_argument("-pru", "--pr_uniq")
            .help("filter projectively (sphere) unique configurations")
            .flag()
            .store_into(params.sph_uniq);

    program.add_argument("--mv_inf")
        .help("Move last line to infinity for even configurations")
        .flag()
        .store_into(params.move_last_to_inf);

    program.add_argument("-a", "--add")
            .help("add configurations to DB")
            .flag()
            .store_into(params.add);

    program.add_argument("-p", "--print")
            .help("print (gens, omatrix, n, k, pos, wire, wire_condensed, eid, pid, lf)")
            .nargs(1, 20)
            .store_into(params.print);

    program.add_argument("--run")
            .help("run processing of configurations in DB")
            .store_into(params.run);

    program.add_argument("--concurrency")
            .help("run concurrency")
            .store_into(params.concurrency);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::set<Line> remove_lines;
    if (!params.remove_lines.empty()) {
        for (auto l : numbers_str_to_vec(params.remove_lines))
            remove_lines.insert(l);
    }

    Processor processor(params, remove_lines);

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

            std::vector<Line> gens = numbers_str_to_vec(line);
            if (params.line_num > 0 && gens.empty())
            {
                std::cerr << "Line is empty" << std::endl;
                return -1;
            }

            if (!gens.empty())
                processor.process(Configuration(i, gens));

            if (params.line_num > 0)
                break;
        }
    }
    if (!params.gens.empty()) {
        processor.process(Configuration(0, numbers_str_to_vec(params.gens)));
    }

    processor.flush_db();

    if (!params.run.empty()) {
        auto run_configs = load_configs();
        bool found = false;
        for (auto &run_conf : run_configs) {
            if (run_conf.name == params.run) {
                found = true;
                process(run_conf, params.concurrency);
            }
        }
        if (!found) {
            std::cerr << "Run " << params.run << " not found" << std::endl;
            return -1;
        }
    }

    return 0;
}



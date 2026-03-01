#include <sqlite3.h>
#include <memory>
#include <string>
#include <system_error>
#include <iostream>
#include <chrono>
#include <format>
#include "db.hpp"
#include "graceful_stop.hpp"
#include "process.hpp"

struct DbDeleter {
    void operator()(sqlite3* ptr) {
        if (ptr) {
            sqlite3_close_v2(ptr);
        }
    }
};

struct StmtDeleter {
    void operator()(sqlite3_stmt* stmt) {
        sqlite3_finalize(stmt);
    }
};

class Statement {
public:
    Statement(sqlite3* db, const std::string& sql): db(db) {
        sqlite3_stmt* raw_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr);
        m_stmt.reset(raw_stmt);

        if (rc != SQLITE_OK) {
            throw std::runtime_error(std::string("Prepare failed: ") + sqlite3_errmsg(db));
        }
    }

    sqlite3_stmt* get() const { return m_stmt.get(); }

    bool step() {
        int rc = sqlite3_step(m_stmt.get());
        if (rc == SQLITE_ROW)
            return true;
        if (rc == SQLITE_DONE)
            return false;
        std::cerr << sqlite3_errmsg(db) << std::endl;
        throw std::runtime_error("Step error");
    }

    std::string get_column_text(int col) {
        const unsigned char* txt = sqlite3_column_text(m_stmt.get(), col);
        return txt ? std::string(reinterpret_cast<const char*>(txt)) : std::string();
    }

    int get_column_int(int col) {
        return sqlite3_column_int(m_stmt.get(), col);
    }
private:
    sqlite3* db;
    std::unique_ptr<sqlite3_stmt, StmtDeleter> m_stmt;
};

void exec_sql(sqlite3* db, const std::string &sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        std::string errMsgStr = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error(errMsgStr);
    }
}

struct Transaction {
    sqlite3* db;
    bool rollback = false;

    explicit Transaction(sqlite3 *db) : db(db) {
        exec_sql(db, "BEGIN TRANSACTION;");
    }

    ~Transaction() {
        try {
            if (rollback)
                exec_sql(db, "ROLLBACK;");
            else
                exec_sql(db, "COMMIT;");
        }
        catch (std::runtime_error e) {
            std::cerr << "COMMIT ERROR: " << e.what() << std::endl;
        }
    }
};

struct Db {
    explicit Db(const char *file_name) {
        sqlite3* raw = nullptr;
        int rc = sqlite3_open_v2(file_name, &raw,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);

        m_db.reset(raw);

        if (rc != SQLITE_OK) {
            std::string errMsg = sqlite3_errmsg(m_db.get());
            throw std::runtime_error("Failed to open DB: " + errMsg);
        }

        exec_sql(raw, "PRAGMA journal_mode=WAL;");
    }

    operator sqlite3*() const {
        return m_db.get();
    }

    sqlite3* get() const {
        return m_db.get();
    }

    Statement createStatement(const std::string& sql) {
        return Statement(m_db.get(), sql);
    }
private:
    std::unique_ptr<sqlite3, DbDeleter> m_db;
};

Db open_main_db() {
    Db db("arn_db.s3db");
    exec_sql(db.get(), R"(
CREATE TABLE IF NOT EXISTS configurations(
eid TEXT PRIMARY KEY,
n INTEGER NOT NULL,
k INTEGER NOT NULL,
date TEXT NOT NULL,
gens TEXT NOT NULL,
ignored NUMBER NOT NULL
);
)");

    exec_sql(db.get(), R"(
CREATE TABLE IF NOT EXISTS runs(
id INTEGER PRIMARY KEY AUTOINCREMENT,
name TEXT NOT NULL,
eid TEXT,
start_time TEXT NOT NULL,
duration_ms NUMBER TEXT NOT NULL,
status NUMBER NOT NULL
);
)");

    exec_sql(db.get(), "CREATE INDEX IF NOT EXISTS runs_eid_inx ON runs(eid)");

    exec_sql(db.get(), R"(
CREATE TABLE IF NOT EXISTS results(
id INTEGER PRIMARY KEY AUTOINCREMENT,
run_id INTEGER NOT NULL,
k TEXT NOT NULL,
v TEXT
);
)");

    return db;
}

std::string current_time_str () {
    auto now = std::chrono::zoned_time{std::chrono::current_zone(),
                                       floor < std::chrono::milliseconds > (std::chrono::system_clock::now())};
    return std::format("{:%Y-%m-%dT%H:%M:%S%z}", now);
}

void add_to_db(const std::vector<Configuration> &configurations) {
    for (auto &c : configurations) {
        if (is_stopped())
            return;
        auto _ = c.get_min_o();
    }

    Db db = open_main_db();
    Transaction transaction(db.get());

    for (auto &c : configurations) {
        if (is_stopped()) {
            transaction.rollback = true;
            return;
        }

        OMatrixPtr mo = c.o->min_o();
        std::string eid = c.o->get_eid();
        std::string date_time = current_time_str();

        std::vector<Line> gens = mo->get_generators();
        std::string gens_str;
        for (size_t i = 0; i < gens.size(); ++i) {
            if (i > 0)
                gens_str += " ";
            gens_str += std::to_string(gens[i]);
        }
        auto s = db.createStatement("INSERT INTO configurations(eid, n, k, date, gens, ignored) VALUES(?,?,?,?,?,0) ON CONFLICT(eid) DO NOTHING");
        sqlite3_bind_text(s.get(), 1, eid.data(), eid.size(), SQLITE_STATIC);
        sqlite3_bind_int(s.get(), 2, c.o->n());
        sqlite3_bind_int(s.get(), 3, c.o->k());
        sqlite3_bind_text(s.get(), 4, date_time.data(), date_time.size(), SQLITE_STATIC);
        sqlite3_bind_text(s.get(), 5, gens_str.data(), gens_str.size(), SQLITE_STATIC);
        s.step();
    }
}

std::vector<std::string> get_conf_eids(Db &db, const RunConfig &run_command) {
    std::vector<std::string> result;
    Statement s = db.createStatement("SELECT eid FROM configurations WHERE ignored = 0"
                                     " AND NOT EXISTS (SELECT 1 FROM runs WHERE runs.name = ? AND runs.eid = configurations.eid)");
    sqlite3_bind_text(s.get(), 1, run_command.name.data(), run_command.name.size(), SQLITE_STATIC);
    while (s.step()) {
        if (is_stopped())
            break;
        result.push_back(s.get_column_text(0));
    }

    return result;
}

struct SavedConf {
    std::string eid;
    std::string gens;
    Line n;
    Line k;
};

std::optional<SavedConf> get_saved_conf(Db &db, const std::string &eid) {
    Statement s = db.createStatement("SELECT gens, n, k FROM configurations WHERE eid = ?");
    sqlite3_bind_text(s.get(), 1, eid.data(), eid.size(), SQLITE_STATIC);
    if (s.step()) {
        SavedConf c;
        c.eid = eid;
        c.gens = s.get_column_text(0);
        c.n = s.get_column_int(1);
        c.k = s.get_column_int(2);
        return c;
    }

    return {};
}


struct TimerElapsed {
    std::chrono::steady_clock::time_point start;
    double &elapsed;
    TimerElapsed(double &elapsed) : elapsed(elapsed) {
        start = std::chrono::steady_clock::now();
    }

    ~TimerElapsed() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start);
        elapsed += duration.count();
    }
};

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); }));
}

static inline void rtrim(std::string &s) {
    s.erase(std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c){ return std::isspace(c); }).base(), s.end());
}

static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::vector<std::pair<std::string, std::string>> parse_output(const std::string &output, const std::string &err_output) {
    std::vector<std::pair<std::string, std::string>> result;

    std::stringstream ss(output);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        auto colon = line.find(':');
        if (colon == std::string::npos) {
            result.emplace_back("$", line);
        }
        else {
            auto k = line.substr(0, colon);
            auto v = line.substr(colon + 1);
            trim(v);

            result.emplace_back(k, v);
        }
    }

    if (!err_output.empty()) {
        result.emplace_back("$ERROR", err_output);
    }

    return result;
}

void process(const RunConfig &run_command) {
    Db db = open_main_db();
    std::vector<std::string> eids = get_conf_eids(db, run_command);
    std::cout << "Found " << eids.size() << " configurations to process" << std::endl;

    size_t i = 0;
    for (auto &eid : eids) {
        if (is_stopped())
            break;

        auto saved_conf = get_saved_conf(db, eid);
        if (!saved_conf) {
            std::cerr << "Configuration " << eid << " not found" << std::endl;
            continue;
        }

        std::vector<std::string> cmd;
        for (auto &cmd_v : run_command.command) {
            if (cmd_v == "%gens") {
                cmd.push_back(saved_conf->gens);
            }
            else if (cmd_v == "%n") {
                cmd.push_back(std::to_string(saved_conf->n));
            }
            else if (cmd_v == "%k") {
                cmd.push_back(std::to_string(saved_conf->k));
            }
            else if (cmd_v == "%eid") {
                cmd.push_back(saved_conf->eid);
            }
            else {
                cmd.push_back(cmd_v);
            }
        }

        std::string output;
        std::string err_output;
        double duration = 0.0;
        int status;
        std::string start_time = current_time_str();

        {
            TimerElapsed t(duration);
            TinyProcessLib::Process process(cmd, "", [&output](const char *bytes, size_t n) {
                output += std::string(bytes, n);
            }, [&err_output](const char *bytes, size_t n) {
                err_output += std::string(bytes, n);
            });
            status = process.get_exit_status();
        }

        auto output_vals = parse_output(output, err_output);

        Transaction transaction(db.get());
        auto insert_run = db.createStatement("INSERT INTO runs(name, eid, start_time, duration_ms, status) VALUES(?,?,?,?,?)");
        sqlite3_bind_text(insert_run.get(), 1, run_command.name.data(), run_command.name.size(), SQLITE_STATIC);
        sqlite3_bind_text(insert_run.get(), 2, eid.data(), eid.size(), SQLITE_STATIC);
        sqlite3_bind_text(insert_run.get(), 3, start_time.data(), start_time.size(), SQLITE_STATIC);
        sqlite3_bind_double(insert_run.get(), 4, duration);
        sqlite3_bind_int(insert_run.get(), 5, status);
        insert_run.step();

        sqlite3_int64 run_id = sqlite3_last_insert_rowid(db.get());

        ++i;
        std::cout << "Processed " << i << "/" << eids.size() << std::endl;

        auto insert_results = db.createStatement("INSERT INTO results(run_id, k, v) VALUES(?,?,?)");
        for (auto &p : output_vals) {
            sqlite3_bind_int64(insert_results.get(), 1, run_id);
            sqlite3_bind_text(insert_results.get(), 2, p.first.data(), p.first.size(), SQLITE_STATIC);
            sqlite3_bind_text(insert_results.get(), 3, p.second.data(), p.second.size(), SQLITE_STATIC);
            insert_results.step();
            sqlite3_reset(insert_results.get());
        }
    }
}
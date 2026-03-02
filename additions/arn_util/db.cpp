#include <sqlite3.h>
#include <memory>
#include <string>
#include <system_error>
#include <iostream>
#include <chrono>
#include <format>
#include <thread>
#include "db.hpp"
#include "graceful_stop.hpp"
#include "process.hpp"
#include "blocking_queue.hpp"

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

static std::mutex g_proc_mtx;
static std::unordered_set<TinyProcessLib::Process*> g_running;

struct ProcessGuard {
    TinyProcessLib::Process *p;
    explicit ProcessGuard(TinyProcessLib::Process &proc) : p(&proc) {
        std::lock_guard lk(g_proc_mtx);
        g_running.insert(p);
    }
    ~ProcessGuard() {
        std::lock_guard lk(g_proc_mtx);
        g_running.erase(p);
    }
};

static void kill_all_processes() {
    std::lock_guard lk(g_proc_mtx);
    for (auto *p : g_running) {
        try {
            p->kill();
        } catch (...) {}
    }
}

struct ProcRun {
    int status = -1;
    double duration_ms = 0.0;
    std::string out;
    std::string err;
};

static ProcRun run_capture(const std::vector<std::string> &cmd) {
    ProcRun r;
    TimerElapsed t(r.duration_ms);

    TinyProcessLib::Process proc(
            cmd, "",
            [&r](const char *bytes, size_t n){
                r.out.append(bytes, n);
            },
            [&r](const char *bytes, size_t n){
                r.err.append(bytes, n);
            }
    );
    ProcessGuard guard(proc);
    r.status = proc.get_exit_status();
    return r;
}

struct TaskResult {
    std::string eid;
    std::string start_time;

    int status = -1;
    double duration_ms = 0.0;
    std::string stdout_data;
    std::string stderr_data;
};

static std::vector<std::string> build_cmd(
        const std::vector<std::string> &tmpl,
        const std::string &gens_value
) {
    std::vector<std::string> cmd;
    cmd.reserve(tmpl.size());
    for (const auto &s : tmpl) {
        if (s == "%gens")
            cmd.push_back(gens_value);
        else
            cmd.push_back(s);
    }
    return cmd;
}

static std::string rtrim_newlines(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

static TaskResult run_one_task(const RunConfig &run_command, const SavedConf &saved_conf) {
    TaskResult tr;
    tr.eid = saved_conf.eid;
    tr.start_time = current_time_str();

    if (is_stopped())
        return tr;

    std::string gens_for_command = saved_conf.gens;

    if (!run_command.transform.empty() && !is_stopped()) {
        auto tcmd = build_cmd(run_command.transform, saved_conf.gens);
        auto t = run_capture(tcmd);

        if (is_stopped()) return tr;

        if (t.status != 0) {
            tr.status = t.status;
            tr.duration_ms = t.duration_ms;
            tr.stderr_data = "Transform error. " + t.err;
            tr.stdout_data = t.out;
            return tr;
        }

        gens_for_command = rtrim_newlines(t.out);
        if (gens_for_command.empty()) {
            tr.status = 2;
            tr.duration_ms = t.duration_ms;
            tr.stderr_data = "Transform produced empty gens";
            return tr;
        }
    }

    auto cmd = build_cmd(run_command.command, gens_for_command);
    auto r = run_capture(cmd);

    tr.status = r.status;
    tr.duration_ms = r.duration_ms;
    tr.stdout_data = std::move(r.out);
    tr.stderr_data = std::move(r.err);
    return tr;
}


void process(const RunConfig &run_command, size_t concurrency) {
    Db db = open_main_db();
    std::vector<std::string> eids = get_conf_eids(db, run_command);
    std::cout << "Found " << eids.size() << " configurations to process, concurrency: " << concurrency << std::endl;

    if (concurrency == 0)
        concurrency = 1;
    concurrency = std::min(concurrency, eids.size());

    BlockingQueue<std::pair<size_t, TaskResult>> completed;
    std::jthread killer([&]{
        while (!is_stopped())
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        completed.notify_one();
    });

    auto start_slot = [&](size_t slot, size_t eid_index) {
        return std::jthread([&, slot, eid_index]{
            auto eid = eids[eid_index];
            auto saved_conf = get_saved_conf(db, eid);
            if (!saved_conf) {
                std::cerr << "Configuration " << eid << " not found" << std::endl;
                completed.push({slot, TaskResult()});
                return;
            }
            TaskResult r = run_one_task(run_command, *saved_conf);
            completed.push({slot, std::move(r)});
        });
    };

    std::vector<std::jthread> slots;
    slots.resize(concurrency);

    size_t next_to_start = 0;
    for (size_t s = 0; s < concurrency; ++s) {
        slots[s] = start_slot(s, next_to_start++);
    }

    size_t finished = 0;

    while (finished < eids.size()) {
        if (is_stopped()) {
            break;
        }

        auto msg = completed.pop_wait();
        if (!msg)
            break;

        const size_t slot = msg->first;
        TaskResult &r = msg->second;

        {
            auto output_vals = parse_output(r.stdout_data, r.stderr_data);

            Transaction transaction(db.get());
            auto insert_run = db.createStatement("INSERT INTO runs(name, eid, start_time, duration_ms, status) VALUES(?,?,?,?,?)");
            sqlite3_bind_text(insert_run.get(), 1, run_command.name.data(), run_command.name.size(), SQLITE_STATIC);
            sqlite3_bind_text(insert_run.get(), 2, r.eid.data(), r.eid.size(), SQLITE_STATIC);
            sqlite3_bind_text(insert_run.get(), 3, r.start_time.data(), r.start_time.size(), SQLITE_STATIC);
            sqlite3_bind_double(insert_run.get(), 4, r.duration_ms);
            sqlite3_bind_int(insert_run.get(), 5, r.status);
            insert_run.step();

            sqlite3_int64 run_id = sqlite3_last_insert_rowid(db.get());

            auto insert_results = db.createStatement("INSERT INTO results(run_id, k, v) VALUES(?,?,?)");
            for (auto &p : output_vals) {
                sqlite3_bind_int64(insert_results.get(), 1, run_id);
                sqlite3_bind_text(insert_results.get(), 2, p.first.data(), p.first.size(), SQLITE_STATIC);
                sqlite3_bind_text(insert_results.get(), 3, p.second.data(), p.second.size(), SQLITE_STATIC);
                insert_results.step();
                sqlite3_reset(insert_results.get());
            }
        }

        ++finished;
        std::cout << "Processed " << finished << "/" << eids.size() << std::endl;

        if (!is_stopped() && next_to_start < eids.size()) {
            slots[slot] = start_slot(slot, next_to_start++);
        }
    }

    kill_all_processes();
}
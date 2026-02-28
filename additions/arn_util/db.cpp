#include <sqlite3.h>
#include <memory>
#include <string>
#include <system_error>
#include <iostream>
#include <chrono>
#include <format>
#include "db.hpp"
#include "graceful_stop.hpp"

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
    Statement(sqlite3* db, const std::string& sql) {
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
        throw std::runtime_error("Step error");
    }

    std::string getColumnText(int col) {
        const unsigned char* txt = sqlite3_column_text(m_stmt.get(), col);
        return txt ? std::string(reinterpret_cast<const char*>(txt)) : std::string();
    }
private:
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
    sqlite3* db;
    explicit Db() {
        sqlite3* raw = nullptr;
        int rc = sqlite3_open_v2("arn_db.s3db", &raw,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);

        m_db.reset(raw);

        if (rc != SQLITE_OK) {
            std::string errMsg = sqlite3_errmsg(m_db.get());
            throw std::runtime_error("Failed to open DB: " + errMsg);
        }

        exec_sql(raw, "PRAGMA journal_mode=WAL;");

        exec_sql(raw, R"(
CREATE TABLE IF NOT EXISTS configurations(
eid TEXT PRIMARY KEY,
n INTEGER NOT NULL,
k INTEGER NOT NULL,
date TEXT NOT NULL,
gens TEXT NOT NULL
);
)");
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

void add_to_db(const std::vector<Configuration> &configurations) {
    for (auto &c : configurations) {
        if (is_stopped())
            return;
        auto _ = c.get_min_o();
    }

    Db db;
    Transaction transaction(db.get());

    for (auto &c : configurations) {
        if (is_stopped()) {
            transaction.rollback = true;
            return;
        }

        OMatrixPtr mo = c.o->min_o();
        std::string eid = c.o->get_eid();
        auto now = std::chrono::zoned_time{std::chrono::current_zone(),
                                           floor < std::chrono::milliseconds > (std::chrono::system_clock::now())};
        std::string date_time = std::format("{:%Y-%m-%dT%H:%M:%S%z}", now);

        std::vector<Line> gens = mo->get_generators();
        std::string gens_str;
        for (size_t i = 0; i < gens.size(); ++i) {
            if (i > 0)
                gens_str += " ";
            gens_str += std::to_string(gens[i]);
        }
        auto s = db.createStatement("INSERT INTO configurations(eid, n, k, date, gens) VALUES(?,?,?,?,?) ON CONFLICT(eid) DO NOTHING");
        sqlite3_bind_text(s.get(), 1, eid.data(), eid.size(), SQLITE_STATIC);
        sqlite3_bind_int(s.get(), 2, c.o->n());
        sqlite3_bind_int(s.get(), 3, c.o->k());
        sqlite3_bind_text(s.get(), 4, date_time.data(), date_time.size(), SQLITE_STATIC);
        sqlite3_bind_text(s.get(), 5, gens_str.data(), gens_str.size(), SQLITE_STATIC);
        s.step();
    }
}
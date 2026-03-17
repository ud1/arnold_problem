#pragma once

#include <stdexcept>
#include <string>

enum class BackendKind {
    Highs,
    Custom,
    Both
};

static inline BackendKind parse_backend_kind(const std::string& value) {
    if (value == "highs") return BackendKind::Highs;
    if (value == "custom") return BackendKind::Custom;
    if (value == "both") return BackendKind::Both;
    throw std::runtime_error("Invalid --solver value, expected highs|custom|both");
}

static inline const char* backend_kind_name(BackendKind backend) {
    switch (backend) {
        case BackendKind::Highs: return "highs";
        case BackendKind::Custom: return "custom";
        case BackendKind::Both: return "both";
    }
    return "unknown";
}

template <typename Result, typename HighsSolver, typename CustomSolver>
static inline Result solve_with_backend_fallback(
    BackendKind backend,
    HighsSolver highs_solver,
    CustomSolver custom_solver
) {
    if (backend == BackendKind::Highs) {
        return highs_solver();
    }
    if (backend == BackendKind::Custom) {
        return custom_solver();
    }
    Result highs = highs_solver();
    if (highs.feasible) return highs;
    return custom_solver();
}

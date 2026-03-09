#pragma once

#include <stdexcept>
#include <string>

enum class BackendKind {
    Highs,
    Custom,
    Auto
};

static inline BackendKind parse_backend_kind(const std::string& value) {
    if (value == "highs") return BackendKind::Highs;
    if (value == "custom") return BackendKind::Custom;
    if (value == "auto") return BackendKind::Auto;
    throw std::runtime_error("Invalid --solver value, expected highs|custom|auto");
}

static inline const char* backend_kind_name(BackendKind backend) {
    switch (backend) {
        case BackendKind::Highs: return "highs";
        case BackendKind::Custom: return "custom";
        case BackendKind::Auto: return "auto";
    }
    return "unknown";
}

#pragma once

#include <stdexcept>
#include <utility>
#include "common/omatrix.h"

struct RotationSearchConfig {
    bool euclidean_rotations = false;   // -e
    bool projective_rotations = false;  // -p (implies -e)
    bool try_reflect = false;           // -r
};

struct RotationStats {
    int tried_projective = 0;
    int tried_euclidean = 0;
};

// Generic rotation search over projective × reflect × euclidean axes.
//
// SolveFn: (const OMatrix& o, bool reflected, int eucl_rot, int proj_rot) -> Result
//   Called for each (projective, reflect, euclidean) combination.
//   Must return a result object representing one solve attempt.
//
// BetterFn: (const Result& candidate, const Result& current_best) -> bool
//   Returns true if candidate is strictly better than current_best.
//
// The best result across all combinations is returned.
// stats is filled with total counts of tried rotations.

template <typename Result, typename SolveFn, typename BetterFn>
Result rotation_search(
    const OMatrix& base,
    const RotationSearchConfig& cfg,
    RotationStats& stats,
    SolveFn solve_fn,
    BetterFn better_fn
) {
    stats.tried_projective = 0;
    stats.tried_euclidean = 0;

    const bool do_euclidean = cfg.euclidean_rotations || cfg.projective_rotations;
    const bool do_projective = cfg.projective_rotations;

    const int proj_start = do_projective ? -1 : 0;
    const int proj_end = do_projective ? (int)base.n : 1;

    const int eucl_count = do_euclidean ? 2 * (int)base.n : 1;
    const int reflect_count = cfg.try_reflect ? 2 : 1;

    Result best{};
    bool have_best = false;

    for (int p = proj_start; p < proj_end; ++p) {
        OMatrix proj_base;
        int proj_tag = -1;

        if (do_projective && p >= 0) {
            if (!base.projective_rotate((size_t)p, proj_base)) continue;
            if (!validate_omatrix(proj_base, true)) continue;
            proj_tag = p;
        } else {
            proj_base = base;
            if (!validate_omatrix(proj_base, false)) continue;
        }
        ++stats.tried_projective;

        for (int reflect_pass = 0; reflect_pass < reflect_count; ++reflect_pass) {
            const bool reflected = (reflect_pass == 1);
            OMatrix reflect_base = reflected ? proj_base.reflect() : proj_base;

            for (int rot = 0; rot < eucl_count; ++rot) {
                OMatrix current;
                if (!reflect_base.rotate(rot, current)) continue;
                if (!validate_omatrix(current, false)) continue;
                ++stats.tried_euclidean;

                Result cur = solve_fn(current, reflected, rot, proj_tag);

                if (!have_best || better_fn(cur, best)) {
                    best = std::move(cur);
                    have_best = true;
                }
            }
        }
    }

    if (!have_best) {
        throw std::runtime_error("No valid O-matrix rotations available");
    }
    return best;
}

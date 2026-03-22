#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIGHS_DIR="/mnt/c/Data/git/arnold-research/code/HiGHS"
HIGHS_BUILD_DIR="$HIGHS_DIR/build"

g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  -I"$ROOT_DIR" \
  -I"$HIGHS_DIR/highs" \
  -I"$HIGHS_BUILD_DIR" \
  "$ROOT_DIR/parabolic_axis.cpp" \
  "$ROOT_DIR/common/input_parsing.cpp" \
  "$ROOT_DIR/common/omatrix.cpp" \
  -L"$HIGHS_BUILD_DIR/lib" \
  -Wl,-rpath,"$HIGHS_BUILD_DIR/lib" \
  -lhighs \
  -o "$ROOT_DIR/parabolic_axis"

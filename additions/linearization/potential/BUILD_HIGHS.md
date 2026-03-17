# Build Notes

## `parabolic_axis`

This repository expects a prebuilt HiGHS checkout at:

```text
/mnt/c/Data/git/arnold-research/code/HiGHS
```

Required artifacts:

- `highs/interfaces/highs_c_api.h`
- `build/HConfig.h`
- `build/lib/libhighs.so`

Build command:

```bash
./build_parabolic_axis.sh
```

Equivalent manual command:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  -I. \
  -I/mnt/c/Data/git/arnold-research/code/HiGHS/highs \
  -I/mnt/c/Data/git/arnold-research/code/HiGHS/build \
  parabolic_axis.cpp \
  common/input_parsing.cpp \
  common/omatrix.cpp \
  -L/mnt/c/Data/git/arnold-research/code/HiGHS/build/lib \
  -Wl,-rpath,/mnt/c/Data/git/arnold-research/code/HiGHS/build/lib \
  -lhighs \
  -o parabolic_axis
```

Quick smoke test:

```bash
printf '0 1 0 2\n0 1 2 0\n' | ./parabolic_axis --solver highs
```

Notes:

- `-I.../HiGHS/build` is needed for `HConfig.h`.
- `-Wl,-rpath,.../build/lib` avoids having to set `LD_LIBRARY_PATH` at runtime.
- If HiGHS is rebuilt in another location, update `HIGHS_DIR` in `build_parabolic_axis.sh`.

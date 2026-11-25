#!/usr/bin/env bash
# mayhem/build.sh — build esmini (sanitized, headless) + the libFuzzer harnesses in mayhem/harnesses/,
# and a normal-flags esmini/dat2csv for mayhem/test.sh (upstream's own smoke tests).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

# externals/fmt is a git submodule; populate it from the in-image cache when it was not checked out.
FMT_DEPS=/opt/toolchains/deps/fmt-40626af88bd7df9a5fb80be7b25ac85b122d6c21/src
if [ ! -f externals/fmt/CMakeLists.txt ]; then
    mkdir -p externals/fmt && cp -a "$FMT_DEPS/." externals/fmt/
fi

# Upstream's CMakeLists runs `git submodule update` (network) unless `<x>/.git` EXISTS for the
# relative paths test/OSC-ALKS-scenarios, test/OSC-NCAP-scenarios, externals/fmt. CMake resolves
# those relative paths against its working directory, so configure from inside the (gitignored)
# build dir and drop marker files there: no network, no edits to upstream files.
COMMON_OPTS=(-DUSE_OSG=OFF -DUSE_OSI=OFF -DUSE_SUMO=OFF -DUSE_GTEST=OFF -DUSE_IMPLOT=OFF -DUSE_PROJ=OFF
             -DDOWNLOAD_EXTERNALS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_INCLUDE_WHAT_YOU_USE=OFF)
configure() {  # configure <builddir> <extra cmake args...>
    local b=$1; shift
    mkdir -p "$b/test/OSC-ALKS-scenarios/.git" "$b/test/OSC-NCAP-scenarios/.git" "$b/externals/fmt/.git"
    (cd "$b" && cmake -S "$SRC" -B . "${COMMON_OPTS[@]}" "$@")
}

# 1) Test build (project's normal flags) -> esmini, dat2csv, replayer, odrplot in $SRC/bin and the
#    code examples in $SRC/code-examples-bin: what upstream's test/smoke_test.py (mayhem/test.sh) runs.
configure build-mayhem-test -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON \
    -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-mayhem-test -j"$MAYHEM_JOBS"
cmake --install build-mayhem-test >/dev/null

# 2) Sanitized fuzz build of the libraries (RelWithDebInfo: upstream strips Release links).
# SanCov on the library + harness objects so libFuzzer/Mayhem see edges in esmini itself.
COVFLAGS=-fsanitize=fuzzer-no-link
FFLAGS="$SANITIZER_FLAGS $COVFLAGS $DEBUG_FLAGS"
configure build-mayhem-fuzz -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="$FFLAGS" -DCMAKE_CXX_FLAGS="$FFLAGS" \
    -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O1 $DEBUG_FLAGS" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O1 $DEBUG_FLAGS" \
    -DBUILD_ODRPLOT=OFF
cmake --build build-mayhem-fuzz -j"$MAYHEM_JOBS" --target esminiLib_static ScenarioEngine Controllers CommonMini PlayerBase RoadManager fmt pugixml_lib

B=build-mayhem-fuzz
lib() { find "$B" -name "lib$1.a" | head -1; }
ES=EnvironmentSimulator
INC=(-I$ES/Libraries/esminiLib -I$ES/Libraries/esminiRMLib -I$ES/Applications/replayer
     -I$ES/Modules/ScenarioEngine/SourceFiles -I$ES/Modules/ScenarioEngine/OSCTypeDefs
     -I$ES/Modules/Controllers -I$ES/Modules/CommonMini -I$ES/Modules/RoadManager -I$ES/Modules/PlayerBase
     -Iexternals/pugixml -Iexternals/fmt/include -Iexternals/dirent/include -Iexternals/yaml -Iexternals/expr)
LIBS=("$(lib esminiLib_static)" "$(lib ScenarioEngine)" "$(lib Controllers)" "$(lib CommonMini)" "$(lib PlayerBase)"
      "$(lib ScenarioEngine)" "$(lib RoadManager)" "$(lib CommonMini)" "$(lib fmt)" "$(lib pugixml_lib)"
      -lstdc++fs -ldl -pthread)
CXXF=(-std=c++17 $SANITIZER_FLAGS $COVFLAGS $DEBUG_FLAGS -O1 "${INC[@]}")

mkdir -p mayhem/bin "$B/mayhem"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/mayhem/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/mayhem/lsan_off.o"
# per-harness extra sources compiled into the harness (not in any upstream library)
$CXX "${CXXF[@]}" -c $ES/Libraries/esminiRMLib/esminiRMLib.cpp -o "$B/mayhem/esminiRMLib.o"
$CXX "${CXXF[@]}" -c $ES/Applications/replayer/Replay.cpp -o "$B/mayhem/Replay.o"
declare -A EXTRA=(
  [fuzz_xodr]="$B/mayhem/esminiRMLib.o"
  [fuzz_dat]="$B/mayhem/Replay.o"
)
for h in mayhem/harnesses/fuzz_*.cpp; do
    n=$(basename "$h" .cpp)
    $CXX "${CXXF[@]}" -c "$h" -o "$B/mayhem/$n.o"
    $CXX "${CXXF[@]}" $LIB_FUZZING_ENGINE "$B/mayhem/$n.o" ${EXTRA[$n]:-} "$B/mayhem/lsan_off.o" "${LIBS[@]}" -o "mayhem/bin/$n"
    $CXX "${CXXF[@]}" "$B/mayhem/standalone_main.o" "$B/mayhem/$n.o" ${EXTRA[$n]:-} "$B/mayhem/lsan_off.o" "${LIBS[@]}" -o "mayhem/bin/$n-standalone"
done
ls -l mayhem/bin

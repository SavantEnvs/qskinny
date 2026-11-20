#!/usr/bin/env bash
#
# mayhem/build.sh — build QSkinny's fuzz harnesses and the functional self-test.
#
#  1. libqskinny (static) with $SANITIZER_FLAGS + SanCov + DWARF-3 in build/fuzz
#  2. three libFuzzer harnesses (+ -standalone reproducers) linked against it and system Qt 6
#  3. libqskinny with the project's NORMAL flags in build/test + mayhem/tests/qsk_selftest
#
# Qt itself (Debian packages) is not instrumented. Build dirs live under build/ (gitignored upstream).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# Upstream's QskFindMacros uses find_package(QT NAMES Qt6 Qt5 ...) and then links versionless Qt::
# targets; this project-include makes sure the Qt6 components (and their Qt:: aliases) exist.
QSK_CMAKE_INCLUDE="$SRC/mayhem/qt6-components.cmake"

QSK_OPTS=(
  -G Ninja
  -DCMAKE_PROJECT_INCLUDE="$QSK_CMAKE_INCLUDE"
  -DBUILD_QSKDLL=OFF -DBUILD_TOOLS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PLAYGROUND=OFF
  -DBUILD_QML_EXPORT=OFF -DBUILD_INPUTCONTEXT=OFF
)

# SanCov for the library: -fsanitize=fuzzer-no-link (only when sanitizing, so the empty
# SANITIZER_FLAGS off-switch gives a plain build).
LIB_COV_FLAGS=""
[ -n "$SANITIZER_FLAGS" ] && LIB_COV_FLAGS="-fsanitize=fuzzer-no-link"

# 1) sanitized library
cmake -S "$SRC" -B build/fuzz "${QSK_OPTS[@]}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $LIB_COV_FLAGS $DEBUG_FLAGS" \
  -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS $LIB_COV_FLAGS $DEBUG_FLAGS" \
  -DCMAKE_C_FLAGS_DEBUG="-O1" -DCMAKE_CXX_FLAGS_DEBUG="-O1"
cmake --build build/fuzz -j"$MAYHEM_JOBS" --target qskinny

FUZZ_LIB="$(find build/fuzz -name 'libqskinny.a' | head -1)"
[ -f "$FUZZ_LIB" ] || { echo "libqskinny.a not built" >&2; exit 1; }

QT_MODULES="Qt6Svg Qt6Quick Qt6Qml Qt6OpenGL Qt6Gui Qt6Core"
QT_CFLAGS="$(pkg-config --cflags $QT_MODULES)"
QT_LIBS="$(pkg-config --libs $QT_MODULES)"
QSK_INC=""
for d in "$SRC"/src/*/; do QSK_INC="$QSK_INC -I$d"; done
HARNESS_FLAGS="-std=c++17 -fPIC -DQSK_STATIC $QSK_INC -I$SRC/mayhem/harnesses $QT_CFLAGS"

mkdir -p build/fuzz/harness
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o build/fuzz/harness/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/fuzz/harness/standalone_main.o

# 2) harnesses
for h in fuzz_glyphtable fuzz_qvg fuzz_svg2qvg; do
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $HARNESS_FLAGS -c "$SRC/mayhem/harnesses/$h.cpp" \
    -o "build/fuzz/harness/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -fPIC "build/fuzz/harness/$h.o" \
    build/fuzz/harness/lsan_off.o "$FUZZ_LIB" $QT_LIBS -o "$SRC/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fPIC "build/fuzz/harness/$h.o" build/fuzz/harness/standalone_main.o \
    build/fuzz/harness/lsan_off.o "$FUZZ_LIB" $QT_LIBS -o "$SRC/$h-standalone"
done

# 3) functional self-test, NORMAL flags
cmake -S "$SRC" -B build/test "${QSK_OPTS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build/test -j"$MAYHEM_JOBS" --target qskinny
TEST_LIB="$(find build/test -name 'libqskinny.a' | head -1)"
$CXX -O1 $COVERAGE_FLAGS -std=c++17 -fPIC -DQSK_STATIC $QSK_INC $QT_CFLAGS \
  "$SRC/mayhem/tests/qsk_selftest.cpp" "$TEST_LIB" $QT_LIBS -o build/test/qsk_selftest

#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT_DIR="$ROOT_DIR/tests/bin"

mkdir -p "$OUT_DIR"

c++ -std=c++11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/test_main.cpp" \
  "$ROOT_DIR/tests/test_crc32.cpp" \
  "$ROOT_DIR/tests/test_cobs.cpp" \
  "$ROOT_DIR/tests/test_framer.cpp" \
  "$ROOT_DIR/tests/test_router.cpp" \
  "$ROOT_DIR/tests/test_node.cpp" \
  "$ROOT_DIR/tests/test_marshal.cpp" \
  -o "$OUT_DIR/umsg_tests"

"$OUT_DIR/umsg_tests"

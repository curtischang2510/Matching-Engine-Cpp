#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

"${CXX:-c++}" -std=c++23 -O1 -g -UNDEBUG -fsanitize=address,undefined \
    -I"$test_dir/.." -I"$test_dir/../../shared" \
    "$test_dir/order_book_test.cpp" \
    "$test_dir/../OrderBook/3_0/OrderBook3_0.cpp" \
    -o "$build_dir/order_book_test"
"$build_dir/order_book_test"

"${CXX:-c++}" -std=c++23 -O1 -g -UNDEBUG -fsanitize=address,undefined -pthread \
    -I"$test_dir/.." -I"$test_dir/../../shared" -I"$test_dir/../../data-structures" \
    "$test_dir/../main.cpp" "$test_dir/../OrderBook/3_0/OrderBook3_0.cpp" \
    "$test_dir/../OrderBook/3_1/OrderBook3_1.cpp" \
    -o "$build_dir/matching_engine_v3"
"$build_dir/matching_engine_v3" "$test_dir/../../test-data/big_data.txt" 3.0 | grep -q 'messages=10345 books=6 '
"$build_dir/matching_engine_v3" "$test_dir/../../test-data/big_data.txt" 3.1 | grep -q 'messages=10345 books=6 '
printf 'V3 tests passed\n'

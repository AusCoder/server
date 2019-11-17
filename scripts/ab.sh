#!/bin/bash
set -eu

for _ in $(seq 10)
do
    ab -n 100 -c 10 localhost:3711/kang-beach.jpg &
done

wait

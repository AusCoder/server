#!/bin/bash
set -eu

ab -n 1000 -c 100 localhost:3711/kang-beach.jpg

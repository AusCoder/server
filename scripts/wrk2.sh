#!/bin/bash
set -eu

/data/opt/bin/wrk2 -t1 -c5 -d30s -R600 http://localhost:3711/kang-beach.jpg

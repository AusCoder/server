#!/bin/bash
set -eu

nginx -p . -c scripts/nginx/nginx.conf "$@"

#!/bin/bash
set -eu

# Trim whitespace
sed -i -e 's/\s\+$//g' include/*.h
sed -i -e 's/\s\+$//g' src/*.c

# Eat trailing whitespace
#perl -pi -e "chomp if eof" include/*.h
#perl -pi -e "chomp if eof" src/*.c

# Format code
clang-format -i include/*.h
clang-format -i src/*.c

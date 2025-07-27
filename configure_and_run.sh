#!/usr/bin/bash

cd "$(pwd)/scripts"

python3 runner.py && source set_env.sh
cd "$(pwd)/../build/tests/" && ./tests_runner
#!/bin/bash

cppcheck --check-level=exhaustive --enable=all --std=c++17 --inline-suppr --suppress=missingIncludeSystem -I ./src ./src

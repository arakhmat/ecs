#!/usr/bin/env bash

set -e

# Formatters
black --check .

# Type Checkers
mypy --config-file mypy.ini ecs

# Linters
MYPYPATH=/dev/null flake8 --config .flake8 --mypy-config mypy.ini ecs unit_tests
pylint --rcfile .pylintrc ecs unit_tests

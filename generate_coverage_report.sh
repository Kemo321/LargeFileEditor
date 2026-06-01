#!/usr/bin/env bash
#
# generate_coverage_report.sh
# Configures the project with coverage instrumentation, runs the test suite,
# and produces an LCOV HTML report. On success the dashboard is left at
#   <build-dir>/coverage_report/index.html
#
# Usage:
#   ./generate_coverage_report.sh [build-dir]
#
# Environment:
#   BUILD_DIR   Override the build directory (default: build-coverage).

set -euo pipefail

# Resolve the repository root (directory containing this script).
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-${BUILD_DIR:-${REPO_ROOT}/build-coverage}}"

echo "==> Configuring with coverage instrumentation in '${BUILD_DIR}'"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "==> Building 'coverage' target (build tests, run them, capture & filter, genhtml)"
cmake --build "${BUILD_DIR}" --target coverage

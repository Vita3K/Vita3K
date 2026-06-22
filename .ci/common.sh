#!/usr/bin/env bash
set -euo pipefail

require_arg() {
    local value="${1:-}"
    local name="${2:-argument}"
    if [[ -z "$value" ]]; then
        echo "Missing required argument: $name" >&2
        exit 1
    fi
}

git_short_sha() {
    git rev-parse --short HEAD
}

#!/bin/bash

set -ex

if [ "$GITHUB_EVENT_NAME" == "pull_request" ]; then 
    for f in $(git diff --diff-filter=AM --name-only origin/$GITHUB_BASE_REF 'vita3k/**.cpp' 'vita3k/**.h' 'tools/**.cpp' 'tools/**.h'); do
        if [ "$(diff -u <(cat $f) <(clang-format $f))" != "" ]
        then
            echo "run format"
            exit 1
        fi
    done
fi
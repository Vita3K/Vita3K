#!/usr/bin/bash

set -ex

if [ "$GITHUB_EVENT_NAME" == "pull_request" ]; then 
    for f in $(git diff --name-only origin/$GITHUB_BASE_REF | grep -E "(vita3k|tools/gen-modules|tools/native-tool)/.*(.cpp|.h)" ); do
        if [ -f "$f" ]; then
            if [ "$(diff -u <(cat $f) <(clang-format $f))" != "" ]
            then
                echo "run format"
                exit 1
            fi
        fi
    done
fi
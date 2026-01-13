#!/bin/bash -x

# Fetch the base and head branches
git fetch origin

# Get changed files
files=$(git diff --diff-filter=AM --name-only origin/upstream_master -- 'vita3k/**.cpp' 'vita3k/**.h' 'tools/**.cpp' 'tools/**.h')

# Apply clang-format
for f in $files; do
    clang-format -i "$f"
done

# Commit if changes exist
if ! git diff --exit-code; then
    git config user.name "GitHub Actions"
    git config user.email "actions@github.com"
    echo "$files" | xargs git add --
    git commit -m "Apply automated clang-format fixes"
    git push origin $GITHUB_HEAD_REF
fi

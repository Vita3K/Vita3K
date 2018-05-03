#!/usr/bin/env bash
set -e

git clone https://github.com/Vita3K/Vita3K-builds.git
cp $https://ci.appveyor.com/BUILD_DIR/build/bin/Vita3K-Windows-nightly.zip Vita3K-builds/
cd Vita3K-builds

git config user.name "Ci.appveyor"
git config user.email "appveyor@appveyor-ci.org"
git add --all
git commit -m "Windows build ${APPVEYOR_BUILD_NUMBER}"
git push --force "https://${REPO_TOKEN}@github.com/Vita3K/Vita3K-builds.git" > /dev/null 2>&1

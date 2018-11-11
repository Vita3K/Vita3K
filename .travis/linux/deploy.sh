#!/usr/bin/env bash
set -e

git clone https://github.com/Vita3K/Vita3K-builds.git
cp $HOME/linux-build/bin/Vita3K-linux-nightly.zip Vita3K-builds/
cd Vita3K-builds

git config user.name "Travis CI"
git config user.email "travis@travis-ci.org"
git add --all
git commit -m "Linux build ${TRAVIS_BUILD_NUMBER}"
git push --force "https://${REPO_TOKEN}@github.com/Vita3K/Vita3K-builds.git" > /dev/null 2>&1

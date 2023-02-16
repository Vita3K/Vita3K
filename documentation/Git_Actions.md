# Vita3K's use of GitHub actions (and CI)

As many other projects on GitHub, Vita3K makes use of GitHub Actions to automate code analysis, formatting and building.

You can view the actions for C-C++ code generation (CI) and testing at `Vita3K/.github/workflows/c-cpp.yml`. CodeQL, the formatting checker, can be viewed at `Vita3K/.github/workflows/codeql-analysis.yml`. The executable file for formatting checks is at `Vita3K/.github/format-check.sh`.

GitHub actions generate the C/C++ code for the project for Windows, MacOS and Linux. If you don't want to compile a modified version on your machine, you could use the CI to do it for you. However, this has too many cons, you should just use a virtual machine or docker instead.

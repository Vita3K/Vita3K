#pragma once

#ifndef __APPLE__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
void rmkdir(const char *dir);
namespace fs {
bool create_directory(std::string path);
bool create_directories(std::string path);
int remove(std::string path);
} // namespace fs
#endif

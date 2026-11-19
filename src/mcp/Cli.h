#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace sf {
int runCli(const std::vector<std::string> &arguments, const std::filesystem::path &root);
}

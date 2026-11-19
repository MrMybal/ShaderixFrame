#include "core/Json.h"
#include "mcp/Cli.h"
// The shell API needs Windows types before its declarations.
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
int wmain(int argc, wchar_t **argv) {
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
        arguments.push_back(sx::Narrow(argv[i]));
    wchar_t executable[32768];
    GetModuleFileNameW(nullptr, executable, 32768);
    return sf::runCli(arguments, std::filesystem::path(executable).parent_path());
}

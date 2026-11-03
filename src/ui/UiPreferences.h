#pragma once
#include <filesystem>
namespace sf {
void initializeUiPreferences(const std::filesystem::path& file,float dpi);
void uiPreferencesMenu();
}

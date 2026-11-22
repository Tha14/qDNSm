#pragma once
#include <string>
#include "LanguageIds.generated.hpp"

bool Language_ParseCatalog(const std::string& text, std::wstring* values, bool* present);
bool Language_TextsCompatible(const std::wstring& english, const std::wstring& translated);

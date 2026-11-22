#pragma once
#include <string>

bool Language_ExternalFileExists(const std::string& code);
bool Language_ReadExternalFile(const std::string& code, std::string& out);

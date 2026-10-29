#pragma once
#include <string>

std::wstring DecodeUtf8(const char* text);

bool Language_IsValidCode(const std::string& code);
const char* Language_MapSystemLanguage(int raw);
int Language_IndexOfCode(const std::string& code);
const char* Language_CodeAtIndex(int index);
int Language_CodeCount();

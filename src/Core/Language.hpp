#pragma once
#include <string>
#include "LanguageIds.generated.hpp"

bool Language_Init(const std::string& code);
void Language_LoadAll();
void Language_Unload();

const std::wstring& TS(TextId id);
const wchar_t* T(TextId id);
std::wstring FormatText(TextId id, const std::wstring& argument);
std::wstring FormatText(TextId id, const std::wstring& first,
	const std::wstring& second);

const std::string& Language_ActiveCode();

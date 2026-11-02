#pragma once

#include <stddef.h>

bool NormalizeTitleId(const char* titleId, char normalized[10]);
bool TitleIdInList(const char* titleId, const char* const* list, size_t count);
bool IsSupportedTitleId(const char* titleId);
bool IsGothamCityImpostorsTitleId(const char* titleId);

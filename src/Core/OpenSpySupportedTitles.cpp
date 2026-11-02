#include "OpenSpySupportedTitles.hpp"

#include "OpenSpySupportedTitles.generated.hpp"

#include <string.h>

bool NormalizeTitleId(const char* titleId, char normalized[10])
{
	if (!titleId)
		return false;

	for (size_t i = 0; i < 9; i++)
	{
		char character = titleId[i];
		if (!character)
			return false;

		if (i < 4)
		{
			if ((character >= 'a') && (character <= 'z'))
				character -= 'a' - 'A';
			else if ((character < 'A') || (character > 'Z'))
				return false;
		}
		else if ((character < '0') || (character > '9'))
		{
			return false;
		}

		normalized[i] = character;
	}

	if (titleId[9] != '\0')
		return false;

	normalized[9] = '\0';
	return true;
}

bool TitleIdInList(const char* titleId, const char* const* list, size_t count)
{
	char normalized[10];

	if (!list || !NormalizeTitleId(titleId, normalized))
		return false;

	for (size_t i = 0; i < count; i++)
	{
		if (list[i] && (memcmp(list[i], normalized, 9) == 0) &&
			(list[i][9] == '\0'))
		{
			return true;
		}
	}

	return false;
}

bool IsSupportedTitleId(const char* titleId)
{
	return TitleIdInList(titleId, kSupportedGameTitleIds,
		kSupportedGameTitleIdCount);
}

bool IsGothamCityImpostorsTitleId(const char* titleId)
{
	static const char* const kGothamCityImpostorsTitleIds[] = {
		"NPUB30252",
		"NPEB00239"
	};

	return TitleIdInList(titleId, kGothamCityImpostorsTitleIds,
		sizeof(kGothamCityImpostorsTitleIds) /
			sizeof(kGothamCityImpostorsTitleIds[0]));
}

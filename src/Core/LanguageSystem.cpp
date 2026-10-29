#include "LanguageSystem.hpp"
#include "LanguageCodec.hpp"

extern "C" {
	#include "../Utils/xregistry.h"
}

std::string Language_ResolveConfiguredCode(const std::string& requested)
{
	if (requested != "auto")
		return requested;

	xsetting_0AF1F161_class* settings = xsettings();

	if (settings == nullptr || settings->GetSystemLanguage == nullptr)
		return "en";

	int32_t raw = 1;

	if (settings->GetSystemLanguage(&raw) != 0)
		return "en";

	return Language_MapSystemLanguage(static_cast<int>(raw));
}

#include "LanguageStorage.hpp"
#include "../Utils/FileSystem.hpp"

static std::string LanguagePath(const std::string& code)
{
	return GetCurrentDir() + "qDNSm_lang/" + code + ".ini";
}

bool Language_ExternalFileExists(const std::string& code)
{
	return FileExist(LanguagePath(code));
}

bool Language_ReadExternalFile(const std::string& code, std::string& out)
{
	return ReadFileText(LanguagePath(code), out, 64 * 1024);
}

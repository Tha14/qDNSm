#pragma once
#include <string>
#include <sys/prx.h>
#include <cell/cell_fs.h>

sys_prx_id_t GetModuleHandle(const char* moduleName);
sys_prx_module_info_t GetModuleInfo(sys_prx_id_t handle);
std::string GetModuleFilePath(const char* moduleName);
std::string RemoveBaseNameFromPath(const std::string& filePath);
std::string GetCurrentDir();

bool FileExist(const std::string& filePath);

/*
 * Reads a whole file into a std::string using cellFs - the same API family SaveFile() uses
 * for writing, which is known to work. The old ReadFile() used vsh::stat / vsh::fopen /
 * vsh::fread and had never once been observed to succeed, and it needed the VSH allocator.
 * Fixed internal buffer, so no allocation and no free.
 */
bool ReadFileText(const std::string& fileName, std::string& out);
void SaveFile(const std::string& fileName, const void* data, size_t size);
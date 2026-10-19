#include "FileSystem.hpp"

#include <vsh/stdc.hpp>
sys_prx_id_t GetModuleHandle(const char* moduleName)
{
   return (moduleName) ? sys_prx_get_module_id_by_name(moduleName, 0, nullptr) : sys_prx_get_my_module_id();
}

sys_prx_module_info_t GetModuleInfo(sys_prx_id_t handle)
{
   sys_prx_module_info_t info{};
   static sys_prx_segment_info_t segments[10]{};
   static char filename[SYS_PRX_MODULE_FILENAME_SIZE]{};

   vsh::memset(segments, 0, sizeof(segments));
   vsh::memset(filename, 0, sizeof(filename));

   info.size = sizeof(info);
   info.segments = segments;
   info.segments_num = sizeof(segments) / sizeof(sys_prx_segment_info_t);
   info.filename = filename;
   info.filename_size = sizeof(filename);

   sys_prx_get_module_info(handle, 0, &info);
   return info;
}

std::string GetModuleFilePath(const char* moduleName)
{
   sys_prx_module_info_t info = GetModuleInfo(GetModuleHandle(moduleName));
   return std::string(info.filename);
}

std::string RemoveBaseNameFromPath(const std::string& filePath)
{
   size_t lastpath = filePath.find_last_of("/");
   if (lastpath == std::string::npos)
      return filePath;
   return filePath.substr(0, lastpath);
}

std::string GetCurrentDir()
{
   static std::string cachedModulePath;
   if (cachedModulePath.empty())
   {
      std::string path = RemoveBaseNameFromPath(GetModuleFilePath(nullptr));

      path += "/";  // include trailing slash

      cachedModulePath = path;
   }
   return cachedModulePath;
}

bool FileExist(const std::string& filePath)
{
   CellFsStat stat;
   if (cellFsStat(filePath.c_str(), &stat) == CELL_FS_SUCCEEDED)
      return (stat.st_mode & CELL_FS_S_IFREG);
   return false;
}

/*
 * Largest file ReadFileText will take. Sized for a qDNSm.ini, not for arbitrary files: an
 * entry is roughly 60 bytes ("[DNSServer12]" + Name + Primary), so 8 KB holds well over a
 * hundred servers with room to spare. It was a fixed 8 KB buffer in BSS, which was 83% of
 * the module's zero-initialised data and was resident for the whole session even though it
 * is only used while the INI is being read.
 */
static const size_t kMaxFileSize = 8192;

bool ReadFileText(const std::string& fileName, std::string& out)
{
    CellFsStat stat;

    if (cellFsStat(fileName.c_str(), &stat) != CELL_FS_SUCCEEDED)
        return false;

    if (static_cast<size_t>(stat.st_size) >= kMaxFileSize)
        return false;

    int handle;
    if (cellFsOpen(fileName.c_str(), CELL_FS_O_RDONLY, &handle, NULL, 0) != CELL_FS_SUCCEEDED)
        return false;

    if (stat.st_size == 0)
    {
        cellFsClose(handle);
        out.clear();
        return true;
    }

    out.resize((size_t)stat.st_size);

    uint64_t read = 0;
    CellFsErrno err = cellFsRead(handle, &out[0], (uint64_t)stat.st_size, &read);

    cellFsClose(handle);

    if (err != CELL_FS_SUCCEEDED)
        return false;

    out.resize((size_t)read);
    return true;
}

void SaveFile(const std::string& fileName, const void* data, size_t size)
{
    int flags = CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY;

    cellFsChmod(fileName.c_str(), 0777); // set permissions for overwrite

    int fileHandle;
    if (cellFsOpen(fileName.c_str(), flags, &fileHandle, NULL, 0) != CELL_FS_SUCCEEDED)
        return;

    uint64_t read_size = 0;
    if (cellFsWrite(fileHandle, data, size, &read_size) != CELL_FS_SUCCEEDED)
    {
        cellFsClose(fileHandle);
        return;
    }

    cellFsClose(fileHandle);

    cellFsChmod(fileName.c_str(), 0777); // set permissions if created
}
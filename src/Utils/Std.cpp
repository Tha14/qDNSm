#include "Std.hpp"

template<>
std::string to_string(uint64_t value)
{
   char buf[25];
   vsh::memset(buf, 0, 25);
   int len = vsh::snprintf(buf, sizeof(buf), "%llu", value);
   return std::string(buf);
}

std::wstring to_wstring(float value, int decimalPlaces)
{
   wchar_t buf[25];
   vsh::memset(buf, 0, 25);
   int len = vsh::swprintf(buf, sizeof(buf), L"%.*f", decimalPlaces, value);
   return std::wstring(buf);
}

// StringUtil.cpp
#include "StringUtil.h"

#include <cwctype>

void MakeUpper(std::wstring& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (wchar_t)::towupper(s[i]);
}

std::wstring ToUpper(std::wstring s)
{
    MakeUpper(s);
    return s;
}

void Trim(std::wstring& s)
{
    size_t begin = 0, end = s.size();
    while (begin < end && iswspace(s[begin]))   ++begin;
    while (end > begin && iswspace(s[end - 1])) --end;
    s = s.substr(begin, end - begin);
}

bool Contains(const std::wstring& upper, const wchar_t* token)
{
    return upper.find(token) != std::wstring::npos;
}

// StringUtil.h
// Small string helpers.
#pragma once

#include <string>

void         MakeUpper(std::wstring& s);                                // in place
std::wstring ToUpper(std::wstring s);                                   // copy
void         Trim(std::wstring& s);                                     // strip surrounding white space
bool         Contains(const std::wstring& upper, const wchar_t* token); // substring test

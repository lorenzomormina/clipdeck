#include <cwctype>
#include <string>

std::wstring toLower(const std::wstring &input) {
    std::wstring result = input;

    for (wchar_t &ch : result) {
        ch = std::towlower(ch);
    }

    return result;
}
#include "OceanSecret.h"

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <wincrypt.h>
 #ifdef _MSC_VER
  #pragma comment (lib, "crypt32.lib")
 #endif
#endif

namespace ocean::secret
{
namespace
{
const char* const prefix = "dpapi:";
const char* const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string toBase64 (const unsigned char* data, size_t n)
{
    std::string out;
    for (size_t i = 0; i < n; i += 3)
    {
        const unsigned v = (unsigned) data[i] << 16 | (i + 1 < n ? (unsigned) data[i + 1] << 8 : 0u) | (i + 2 < n ? (unsigned) data[i + 2] : 0u);
        out += alphabet[(v >> 18) & 63];
        out += alphabet[(v >> 12) & 63];
        out += i + 1 < n ? alphabet[(v >> 6) & 63] : '=';
        out += i + 2 < n ? alphabet[v & 63] : '=';
    }
    return out;
}

bool fromBase64 (const std::string& in, std::string& out)
{
    out.clear();
    unsigned v = 0;
    int bits = 0;
    for (char c : in)
    {
        if (c == '=') break;
        const char* p = nullptr;
        for (const char* a = alphabet; *a != 0; ++a) if (*a == c) { p = a; break; }
        if (p == nullptr) return false;
        v = (v << 6) | (unsigned) (p - alphabet);
        bits += 6;
        if (bits >= 8) { bits -= 8; out += (char) ((v >> bits) & 0xff); }
    }
    return true;
}
}

std::string protect (const std::string& plain)
{
#if defined (_WIN32)
    if (plain.empty()) return {};
    DATA_BLOB in { (DWORD) plain.size(), (BYTE*) plain.data() }, out { 0, nullptr };
    if (CryptProtectData (&in, L"Cat Ocean stream key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        std::string s = prefix + toBase64 (out.pbData, out.cbData);
        LocalFree (out.pbData);
        return s;
    }
#endif
    return plain;
}

std::string unprotect (const std::string& stored)
{
    if (stored.rfind (prefix, 0) != 0) return stored;             // stored as it is (old settings, other systems)
#if defined (_WIN32)
    std::string raw;
    if (! fromBase64 (stored.substr (6), raw) || raw.empty()) return {};
    DATA_BLOB in { (DWORD) raw.size(), (BYTE*) raw.data() }, out { 0, nullptr };
    if (CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        std::string s ((const char*) out.pbData, out.cbData);
        SecureZeroMemory (out.pbData, out.cbData);
        LocalFree (out.pbData);
        return s;
    }
#endif
    return {};
}
}

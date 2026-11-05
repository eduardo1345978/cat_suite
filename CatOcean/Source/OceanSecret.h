#pragma once

#include <string>

/**
    Keeps the YouTube stream key out of plain sight in Cat Ocean's settings file.

    On Windows the key is locked with DPAPI (CryptProtectData): only the same Windows user on the same
    computer can unlock it, so a copied settings file is useless. Elsewhere it is stored as it is.
    The locked text starts with "dpapi:" (base64 after it).
*/
namespace ocean::secret
{
std::string protect (const std::string& plain);
/** The key again; empty when it can't be unlocked (another user / computer). Plain old text comes back as it is. */
std::string unprotect (const std::string& stored);
}

#include "SecretStore.h"

#if JUCE_MAC
#include <Security/Security.h>
#include <sys/stat.h>
#elif JUCE_WINDOWS
#include <windows.h>
#include <wincred.h>
#else
#include <sys/stat.h>
#endif

namespace
{
juce::File getFallbackFile()
{
    auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("DiscNativePro");
    if (!directory.exists())
        directory.createDirectory();
    return directory.getChildFile("secrets.json");
}

juce::String getFallbackValue(const juce::String& account)
{
    auto file = getFallbackFile();
    if (!file.existsAsFile())
        return {};

    auto json = juce::JSON::parse(file);
    if (json.isObject() && json.hasProperty(account))
        return json.getProperty(account, juce::var()).toString();
    return {};
}

bool setFallbackValue(const juce::String& account, const juce::String& value)
{
    auto file = getFallbackFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    json.getDynamicObject()->setProperty(account, value);
    if (!file.replaceWithText(juce::JSON::toString(json)))
        return false;

#if !JUCE_WINDOWS
    ::chmod(file.getFullPathName().toRawUTF8(), 0600);
#endif
    return true;
}

bool removeFallbackValue(const juce::String& account)
{
    auto file = getFallbackFile();
    if (!file.existsAsFile())
        return true;

    auto json = juce::JSON::parse(file);
    if (!json.isObject())
        return true;

    json.getDynamicObject()->removeProperty(account);
    if (!file.replaceWithText(juce::JSON::toString(json)))
        return false;

#if !JUCE_WINDOWS
    ::chmod(file.getFullPathName().toRawUTF8(), 0600);
#endif
    return true;
}

#if JUCE_MAC
const char* serviceName = "com.discpro.discnativepro.api-key";

CFMutableDictionaryRef makeKeychainQuery(const juce::String& account)
{
    auto query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    auto service = CFStringCreateWithCString(kCFAllocatorDefault, serviceName, kCFStringEncodingUTF8);
    auto accountString = CFStringCreateWithCString(kCFAllocatorDefault, account.toRawUTF8(), kCFStringEncodingUTF8);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, accountString);
    CFRelease(service);
    CFRelease(accountString);
    return query;
}

bool readKeychain(const juce::String& account, juce::String& result)
{
    auto query = makeKeychainQuery(account);
    const void* returnData = kCFBooleanTrue;
    CFDictionarySetValue(query, kSecReturnData, returnData);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef item = nullptr;
    const auto status = SecItemCopyMatching(query, &item);
    CFRelease(query);

    if (status == errSecItemNotFound)
    {
        result = {};
        return true;
    }
    if (status != errSecSuccess || item == nullptr)
        return false;

    auto data = static_cast<CFDataRef>(item);
    result = juce::String::fromUTF8(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                                    static_cast<int>(CFDataGetLength(data)));
    CFRelease(item);
    return true;
}

bool writeKeychain(const juce::String& account, const juce::String& value)
{
    auto deleteQuery = makeKeychainQuery(account);
    SecItemDelete(deleteQuery);
    CFRelease(deleteQuery);

    auto query = makeKeychainQuery(account);
    auto data = CFDataCreate(kCFAllocatorDefault,
                             reinterpret_cast<const UInt8*>(value.toRawUTF8()),
                             static_cast<CFIndex>(value.getNumBytesAsUTF8()));
    CFDictionarySetValue(query, kSecValueData, data);
    const auto status = SecItemAdd(query, nullptr);
    CFRelease(data);
    CFRelease(query);
    return status == errSecSuccess;
}

bool removeKeychain(const juce::String& account)
{
    auto query = makeKeychainQuery(account);
    const auto status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}
#elif JUCE_WINDOWS
juce::String getCredentialTarget(const juce::String& account)
{
    return "DiscNativePro/" + account;
}

bool readCredential(const juce::String& account, juce::String& result)
{
    PCREDENTIALW credential = nullptr;
    auto target = getCredentialTarget(account).toWideCharPointer();
    if (!CredReadW(target, CRED_TYPE_GENERIC, 0, &credential))
    {
        if (GetLastError() == ERROR_NOT_FOUND)
        {
            result = {};
            return true;
        }
        return false;
    }

    // CredentialBlob es LPBYTE (unsigned char*): MSVC exige reinterpret_cast
    // para pasar a const char* (static_cast solo vale entre tipos relacionados).
    result = juce::String::fromUTF8(reinterpret_cast<const char*>(credential->CredentialBlob),
                                    static_cast<int>(credential->CredentialBlobSize));
    CredFree(credential);
    return true;
}

bool writeCredential(const juce::String& account, const juce::String& value)
{
    auto target = getCredentialTarget(account);
    auto targetName = target.toWideCharPointer();
    auto blob = value.toRawUTF8();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    // CREDENTIALW::TargetName es LPWSTR (no const). El buffer pertenece a
    // `target`, que sigue vivo hasta el CredWriteW de abajo.
    credential.TargetName = const_cast<LPWSTR>(targetName);
    credential.CredentialBlobSize = static_cast<DWORD>(value.getNumBytesAsUTF8());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(blob));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential, 0) != FALSE;
}

bool removeCredential(const juce::String& account)
{
    auto target = getCredentialTarget(account);
    if (CredDeleteW(target.toWideCharPointer(), CRED_TYPE_GENERIC, 0))
        return true;
    return GetLastError() == ERROR_NOT_FOUND;
}
#endif
}

namespace SecretStore
{
juce::String read(const juce::String& account)
{
#if JUCE_MAC
    juce::String result;
    if (readKeychain(account, result))
        return result;
#elif JUCE_WINDOWS
    juce::String result;
    if (readCredential(account, result))
        return result;
#endif
    return getFallbackValue(account);
}

bool write(const juce::String& account, const juce::String& value)
{
#if JUCE_MAC
    if (writeKeychain(account, value))
        return true;
#elif JUCE_WINDOWS
    if (writeCredential(account, value))
        return true;
#endif
    return setFallbackValue(account, value);
}

bool remove(const juce::String& account)
{
#if JUCE_MAC
    if (removeKeychain(account))
    {
        removeFallbackValue(account);
        return true;
    }
#elif JUCE_WINDOWS
    if (removeCredential(account))
    {
        removeFallbackValue(account);
        return true;
    }
#endif
    return removeFallbackValue(account);
}

bool isAvailable()
{
#if JUCE_MAC || JUCE_WINDOWS
    return true;
#else
    // Linux sin Secret Service/libsecret: se usa el archivo de respaldo con permisos restrictivos.
    return false;
#endif
}
}

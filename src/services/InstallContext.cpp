#include "InstallContext.h"

#if JUCE_WINDOWS
    #include <windows.h>
#endif

InstallContext::Kind InstallContext::detect()
{
   #if JUCE_MAC
    return Kind::MacApp;
   #elif JUCE_WINDOWS
    // Empaquetado MSIX (Store): el proceso tiene identidad de paquete.
    // GetPackageFullName vive en kernel32 (Windows 8+) y devuelve
    // ERROR_APPMODEL_ERROR_NO_PACKAGE cuando el proceso es clásico (Setup.exe).
    using GetPackageFullNameFn = LONG (WINAPI*) (HANDLE, UINT32*, PWSTR*);
    if (auto* k32 = ::GetModuleHandleW (L"kernel32.dll"))
    {
        if (auto fn = reinterpret_cast<GetPackageFullNameFn> ((void*) ::GetProcAddress (k32, "GetPackageFullName")))
        {
            UINT32 len = 0;
            if (fn (::GetCurrentProcess(), &len, nullptr) != ERROR_APPMODEL_ERROR_NO_PACKAGE)
                return Kind::WinStore;
        }
    }
    return Kind::WinSetup;
   #else
    // .deb instalado: el binario vive bajo /usr o figura en la base de dpkg.
    const auto exePath = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
    if (exePath.startsWith ("/usr/") || exePath.startsWith ("/opt/"))
        return Kind::LinuxDeb;
    if (juce::File ("/var/lib/dpkg/info/discnativepro.list").existsAsFile())
        return Kind::LinuxDeb;
    return Kind::LinuxPortable;
   #endif
}

juce::String InstallContext::key (Kind kind)
{
    switch (kind)
    {
        case Kind::MacApp:         return "mac-app";
        case Kind::WinStore:       return "win-store";
        case Kind::WinSetup:       return "win-setup";
        case Kind::LinuxDeb:       return "linux-deb";
        case Kind::LinuxPortable:  return "linux-portable";
    }
    return {};
}

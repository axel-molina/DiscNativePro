#include "FfmpegLocator.h"

namespace FfmpegLocator
{

juce::File getInstallTargetFile()
{
    auto userAppDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return userAppDir.getChildFile("Application Support/DiscNativePro/bin/ffmpeg");
}

juce::File locateFfmpeg()
{
    // 1. Bin instalado por la app
    auto installed = getInstallTargetFile();
    if (installed.existsAsFile() && installed.hasWriteAccess())
        return installed;

    // 2. DiscNativePro.app/Contents/Resources/ffmpeg (bundled)
    auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto bundleRes = exeFile.getParentDirectory().getParentDirectory().getChildFile("Resources/ffmpeg");
    if (bundleRes.existsAsFile())
        return bundleRes;

    // 3. Homebrew Apple Silicon
    juce::File brewArm("/opt/homebrew/bin/ffmpeg");
    if (brewArm.existsAsFile())
        return brewArm;

    // 4. Homebrew Intel
    juce::File brewIntel("/usr/local/bin/ffmpeg");
    if (brewIntel.existsAsFile())
        return brewIntel;

    // 5. PATH
    juce::ChildProcess whichProc;
    if (whichProc.start("which ffmpeg"))
    {
        auto out = whichProc.readAllProcessOutput().trim();
        if (out.isNotEmpty() && juce::File(out).existsAsFile())
            return juce::File(out);
    }

    return {};
}

bool ensureInstalled()
{
    auto existing = locateFfmpeg();
    if (existing.existsAsFile())
        return true;

    auto target = getInstallTargetFile();
    target.getParentDirectory().createDirectory();

    auto targetPath = target.getFullPathName();
    auto zipPath = target.getSiblingFile("ffmpeg-download.zip").getFullPathName();

    // evermeet.cx sirve el binario estatico mas reciente de ffmpeg (incluye TLS/RTMPS)
    juce::String cmd = "curl -sL \"https://evermeet.cx/ffmpeg/getrelease/zip\" -o \"" + zipPath + "\""
                       + " && unzip -o -q \"" + zipPath + "\" -d \"" + target.getParentDirectory().getFullPathName() + "\""
                       + " && chmod +x \"" + targetPath + "\""
                       + " && rm -f \"" + zipPath + "\"";
    cmd = "/bin/sh -c '" + cmd.replace("'", "'\\''") + "'";

    juce::ChildProcess proc;
    if (!proc.start(cmd))
        return false;

    proc.waitForProcessToFinish(120000);
    return proc.getExitCode() == 0 && target.existsAsFile() && target.getSize() > 1000000;
}

} // namespace FfmpegLocator

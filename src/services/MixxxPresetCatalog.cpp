#include "MixxxPresetCatalog.h"

// Manifest embebido: los nombres corresponden a archivos reales de
// https://github.com/mixxxdj/mixxx/tree/main/res/controllers (verificados).
// Matching tolerante: el nombre que reporta el SO suele diferir en prefijos
// ("Hercules", "Pioneer") y separadores.
static const std::vector<MixxxPresetEntry> kCatalog = {
    // Hercules
    { "djcontrolstarlight", "Hercules DJControl Starlight.midi.xml" },
    { "djcontrolinpulse500", "Hercules_DJControl_Inpulse_500.midi.xml" },
    { "djcontrolinpulse300", "Hercules_DJControl_Inpulse_300.midi.xml" },
    { "djcontrolinpulse200", "Hercules_DJControl_Inpulse_200.midi.xml" },
    { "djcontrolcompact", "Hercules DJControl Compact.midi.xml" },
    { "djcontrolmix", "Hercules DJControl MIX.midi.xml" },
    { "djcontroljogvision", "Hercules_DJControl_Jogvision.midi.xml" },
    { "p32dj", "Hercules P32 DJ.midi.xml" },
    { "djconsole4mx", "Hercules DJ Console 4-Mx.midi.xml" },
    { "djconsolemk4", "Hercules DJ Console Mk4.midi.xml" },
    { "djconsolemk2", "Hercules DJ Console Mk2.midi.xml" },
    { "rmx2", "Hercules DJ Console RMX 2.midi.xml" },
    { "rmx", "Hercules DJ Console RMX.midi.xml" },
    { "mp3e2", "Hercules DJ Control MP3 e2.midi.xml" },
    { "instinct", "Hercules DJ Control Instinct.midi.xml" },
    { "steel", "Hercules DJ Control Steel.midi.xml" },
    { "air", "Hercules DJ Control AIR.midi.xml" },
    // Pioneer
    { "ddj400", "Pioneer-DDJ-400.midi.xml" },
    { "ddjflx4", "Pioneer-DDJ-FLX4.midi.xml" },
    { "ddj200", "Pioneer DDJ-200.midi.xml" },
    { "ddjsb3", "Pioneer-DDJ-SB3.midi.xml" },
    { "ddjsb2", "Pioneer-DDJ-SB2.midi.xml" },
    { "ddjsb", "Pioneer-DDJ-SB.midi.xml" },
    { "ddjsx", "Pioneer DDJ-SX.midi.xml" },
    // Numark
    { "mixtrackprofx", "Numark Mixtrack Pro FX.midi.xml" },
    { "mixtrackplatinumfx", "Numark Mixtrack Platinum FX.midi.xml" },
    { "mixtrackplatinum", "Numark Mixtrack Platinum.midi.xml" },
    { "mixtrackpro", "Numark Mixtrack Pro.midi.xml" },
    { "mixtrack2", "Numark Mixtrack 2.midi.xml" },
    { "mixtrack3", "Numark-Mixtrack-3.midi.xml" },
    { "dj2go2touch", "Numark_DJ2GO2_Touch.midi.xml" },
    { "dj2go", "Numark DJ2Go.midi.xml" },
    { "partymix", "Numark-Party-Mix.midi.xml" },
    { "ns4fx", "Numark NS4FX.midi.xml" },
    { "ns6ii", "Numark NS6II.midi.xml" },
    { "ns7", "Numark NS7.midi.xml" },
    { "n4", "Numark N4.midi.xml" },
    { "omnicontrol", "Numark Omni Control.midi.xml" },
    { "totalcontrol", "Numark Total Control.midi.xml" },
    // Reloop
    { "beatpad", "Reloop Beatpad.midi.xml" },
    { "beatmix", "Reloop-Beatmix-2-4-MK2.midi.xml" },
    { "digitaljockey", "Reloop-Digital-Jockey-2-IE.midi.xml" },
    { "jockey3", "Reloop Jockey 3 ME.midi.xml" },
    { "terminalmix", "Reloop Terminal Mix 2-4.midi.xml" },
    { "mixage", "Reloop-Mixage.midi.xml" },
    // Denon
    { "mc7000", "Denon-MC7000.midi.xml" },
    { "mc6000mk2", "Denon-MC6000MK2.midi.xml" },
    { "mc4000", "Denon MC4000.midi.xml" },
    { "mc3000", "Denon MC3000.midi.xml" },
    { "dnsc2000", "Denon DN SC2000.midi.xml" },
    // Otros fabricantes populares
    { "bcd3000", "Behringer BCD3000.midi.xml" },
    { "bcr2000", "Behringer BCR2000.midi.xml" },
    { "cmdmicro", "Behringer CMD Micro.midi.xml" },
    { "cmdmm1", "Behringer CMD MM1.midi.xml" },
    { "cmdstudio4a", "Behringer CMDStudio4a.midi.xml" },
    { "kontrolx1", "Traktor Kontrol X1.midi.xml" },
    { "nanokontrol2", "Korg nanoKONTROL 2.midi.xml" },
    { "launchpadmini", "Novation-Launchpad-Mini.midi.xml" },
    { "launchpad", "Novation Launchpad.midi.xml" },
    { "xonek2", "Allen and Heath Xone K2.midi.xml" },
    { "firstmix", "Gemini FirstMix.midi.xml" },
    { "vci100", "Vestax VCI-100.midi.xml" },
    { "vci300", "Vestax VCI-300.midi.xml" },
    { "vci400", "Vestax VCI-400.midi.xml" },
    { "discoverdj", "Ion Discover DJ.midi.xml" },
    { "vms4", "American Audio VMS4.midi.xml" },
    { "vms2", "American Audio VMS2.midi.xml" },
    { "djc4", "Stanton-DJC-4.midi.xml" },
    { "dj505", "Roland_DJ-505.midi.xml" },
    { "midifightertwister", "DJ TechTools MIDI Fighter Twister.midi.xml" },
    { "midifighterspectra", "DJ TechTools MIDI Fighter Spectra.midi.xml" },
};

const std::vector<MixxxPresetEntry>& MixxxPresetCatalog::entries()
{
    return kCatalog;
}

juce::String MixxxPresetCatalog::normalizeName(const juce::String& raw)
{
    juce::String s = raw.toLowerCase();
    s = s.replace(" ", "").replace("-", "").replace("_", "").replace(".", "");
    return s;
}

bool MixxxPresetCatalog::findForDevice(const juce::String& deviceName, MixxxPresetEntry& out)
{
    const auto dev = normalizeName(deviceName);
    if (dev.isEmpty())
        return false;

    int bestLen = 0;
    for (const auto& e : kCatalog)
    {
        const auto alias = normalizeName(e.deviceMatch);
        if (alias.isNotEmpty() && dev.contains(alias) && alias.length() > bestLen)
        {
            bestLen = (int)alias.length();
            out = e;
        }
    }
    return bestLen > 0;
}

juce::String MixxxPresetCatalog::presetUrl(const juce::String& presetFile)
{
    juce::String escaped;
    for (const auto c : presetFile)
    {
        const bool safe = juce::CharacterFunctions::isLetterOrDigit(c) || c == '.' || c == '-' || c == '_';
        if (safe)
            escaped << c;
        else
            escaped << "%" << juce::String::toHexString((int)(unsigned char)c).paddedLeft('0', 2);
    }
    return "https://raw.githubusercontent.com/mixxxdj/mixxx/main/res/controllers/" + escaped;
}

juce::File MixxxPresetCatalog::presetsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("DiscNativePro/controllers");
}

juce::File MixxxPresetCatalog::localPresetFile(const juce::String& presetFile)
{
    auto f = presetsFolder().getChildFile(presetFile);
    if (f.existsAsFile() && f.getSize() > 200)
        return f;
    return {};
}

bool MixxxPresetCatalog::downloadPreset(const juce::String& presetFile, juce::String& error)
{
    auto folder = presetsFolder();
    if (!folder.exists())
        folder.createDirectory();

    auto target = folder.getChildFile(presetFile);
    auto tmp = folder.getChildFile(presetFile + ".tmp");

    juce::URL url(presetUrl(presetFile));
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)));

    if (stream == nullptr)
    {
        error = "No se pudo conectar al repositorio de Mixxx";
        return false;
    }

    juce::FileOutputStream fos(tmp);
    if (!fos.openedOk())
    {
        error = "No se pudo escribir el archivo del preset";
        return false;
    }

    fos.writeFromInputStream(*stream, -1);
    fos.flush();

    if (tmp.getSize() < 200 || !tmp.loadFileAsString().contains("<"))
    {
        tmp.deleteFile();
        error = "El preset descargado no es válido";
        return false;
    }

    tmp.moveFileTo(target);
    return target.existsAsFile();
}

#include "Presets.h"

#include "ParamDefs.h"

namespace rdp {

const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> list = [] {
        std::vector<Preset> l;
        const auto& src = pt::presets();
        for (size_t i = 0; i < src.size(); ++i) l.push_back({src[i].name, src[i].category, src[i].description, static_cast<int>(i)});
        return l;
    }();
    return list;
}

static void resetToDefaults(juce::AudioProcessorValueTreeState& apvts) {
    for (const auto& s : pt::specs()) {
        if (s.id == "snapNow") continue;
        if (auto* p = apvts.getParameter(s.id)) p->setValueNotifyingHost(p->getDefaultValue());
    }
}

void applyPreset(juce::AudioProcessorValueTreeState& apvts, const Preset& preset) {
    const auto values = pt::resolvePreset(pt::presets()[static_cast<size_t>(preset.index)]);
    const auto& t = pt::specs();
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i].id == "snapNow") continue;
        if (auto* p = apvts.getParameter(t[i].id)) p->setValueNotifyingHost(p->convertTo0to1(values[i]));
    }
}

juce::File userPresetFolder() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("RealityDesync").getChildFile("Presets");
}

bool saveUserPreset(juce::AudioProcessorValueTreeState& apvts, const juce::String& name, const juce::String& description) {
    auto folder = userPresetFolder();
    if (!folder.createDirectory().wasOk()) return false;
    juce::String safe = juce::File::createLegalFileName(name.trim());
    if (safe.isEmpty()) return false;
    auto state = apvts.copyState();
    state.setProperty("presetName", name.trim(), nullptr);
    state.setProperty("presetDescription", description, nullptr);
    if (auto xml = state.createXml()) return xml->writeTo(folder.getChildFile(safe + ".rdpreset"));
    return false;
}

juce::Array<juce::File> listUserPresets() {
    juce::Array<juce::File> out;
    auto folder = userPresetFolder();
    if (folder.isDirectory()) out = folder.findChildFiles(juce::File::findFiles, false, "*.rdpreset");
    out.sort();
    return out;
}

bool loadUserPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file, juce::String* nameOut, juce::String* descOut) {
    if (auto xml = juce::XmlDocument::parse(file)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!tree.isValid()) return false;
        // Apply through the parameter objects (not replaceState) so every control and the host update.
        resetToDefaults(apvts);
        for (int i = 0; i < tree.getNumChildren(); ++i) {
            auto child = tree.getChild(i);
            const juce::String id = child.getProperty("id").toString();
            if (id == "snapNow") continue;
            if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(child.getProperty("value"))));
        }
        if (nameOut) *nameOut = tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString();
        if (descOut) *descOut = tree.getProperty("presetDescription", "").toString();
        return true;
    }
    return false;
}

}  // namespace rdp

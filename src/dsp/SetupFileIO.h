#pragma once

#include "AbakosTypes.h"
#include <filesystem>
#include <string>
#include <vector>

namespace abakos
{

struct SExpNode
{
    std::string key;
    std::vector<std::string> atoms;
    std::vector<SExpNode> children;

    const SExpNode* findChild(const std::string& childKey) const;
    std::vector<const SExpNode*> findChildren(const std::string& childKey) const;
    int getIntAtom(size_t idx = 0, int fallback = 0) const;
    std::string getStringAtom(size_t idx = 0, const std::string& fallback = "") const;
};

class SetupFileIO
{
public:
    // S-Expression AST parser matching setup_file_reader (0x10025a34)
    static bool parseSExp(const std::string& text, SExpNode& outRoot);

    // config.txt reader & writer (matching config.exe 0x004016b0)
    static bool loadConfigFile(const std::filesystem::path& path, ConfigSettings& outConfig);
    static bool saveConfigFile(const std::filesystem::path& path, const ConfigSettings& config);

    // setup.txt reader & writer (msb [bank] program [prog])
    static bool loadSetupFile(const std::filesystem::path& path, int& outBank, int& outProgram);
    static bool saveSetupFile(const std::filesystem::path& path, int bank, int program);

    // Preset .txt reader & writer (a/00.txt .. d/7F.txt)
    static bool parsePresetText(const std::string& text, PresetData& outPreset);
    static bool loadPresetFile(const std::filesystem::path& path, PresetData& outPreset);
    static std::string serializePresetText(const PresetData& preset);
    static bool savePresetFile(const std::filesystem::path& path, const PresetData& preset);

    // .scl.tun microtonal scale parser (matching 0x100083dc)
    static bool parseTuningText(const std::string& text, const std::string& nameHint, TuningScale& outScale);
    static bool loadTuningFile(const std::filesystem::path& path, TuningScale& outScale);
};

} // namespace abakos

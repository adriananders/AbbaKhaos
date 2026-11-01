#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

juce::AudioProcessorValueTreeState::ParameterLayout AbakosPROAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    const auto& metaTable = abakos::getParamMetaTable();
    params.reserve(metaTable.size());

    for (const auto& m : metaTable)
    {
        juce::ParameterID paramID { m.id, 1 };
        auto range = juce::NormalisableRange<float>(
            static_cast<float>(m.minVal),
            static_cast<float>(m.maxVal),
            1.0f);

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            paramID,
            juce::String(m.name),
            range,
            static_cast<float>(m.defaultVal)));
    }

    return { params.begin(), params.end() };
}

AbakosPROAudioProcessor::AbakosPROAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "AbakosPROParameters", createParameterLayout())
{
    const auto& metaTable = abakos::getParamMetaTable();
    for (size_t i = 0; i < abakos::kNumParams; ++i)
        paramPtrs_[i] = apvts_.getRawParameterValue(metaTable[i].id);

    locateAndLoadFactoryData();
}

AbakosPROAudioProcessor::~AbakosPROAudioProcessor() = default;

void AbakosPROAudioProcessor::locateAndLoadFactoryData()
{
    // Search candidate paths for HERCs Abakos PRO factory data
    std::vector<std::filesystem::path> candidates;

    auto execFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto execDir  = execFile.getParentDirectory();
    auto contentsDir = execDir.getParentDirectory();

    // 1. Inside bundle Resources/HERCs Abakos PRO or Resources/AbbaKhaos
    candidates.emplace_back(contentsDir.getChildFile("Resources").getChildFile("AbbaKhaos").getFullPathName().toStdString());
    candidates.emplace_back(contentsDir.getChildFile("Resources").getChildFile("HERCs Abakos PRO").getFullPathName().toStdString());
    // 2. Alongside plugin binary or bundle parent
    candidates.emplace_back(execDir.getChildFile("HERCs Abakos PRO").getFullPathName().toStdString());
    candidates.emplace_back(contentsDir.getParentDirectory().getChildFile("HERCs Abakos PRO").getFullPathName().toStdString());

    // 3. User Application Data (GlitchyGloomy/AbbaKhaos or HERCs Music Systems/Abakos PRO)
    auto userAppData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    candidates.emplace_back(userAppData.getChildFile("GlitchyGloomy").getChildFile("AbbaKhaos").getFullPathName().toStdString());
    candidates.emplace_back(userAppData.getChildFile("HERCs Music Systems").getChildFile("Abakos PRO").getFullPathName().toStdString());

#ifdef ABAKOS_DEFAULT_DATA_DIR
    candidates.emplace_back(ABAKOS_DEFAULT_DATA_DIR);
#endif

    for (const auto& c : candidates)
    {
        if (std::filesystem::exists(c / "a"))
        {
            dataDir_ = c;
            break;
        }
    }

    // Initialize all 5 banks with sensible defaults
    for (int b = 0; b < 5; ++b)
    {
        for (int p = 0; p < 128; ++p)
        {
            auto& slot = banks_[static_cast<size_t>(b)][static_cast<size_t>(p)];
            slot.bankIndex    = b;
            slot.programIndex = p;
            char nameBuf[32];
            std::snprintf(nameBuf, sizeof(nameBuf), "Init Patch %02X", p);
            slot.name = nameBuf;
        }
    }

    if (!dataDir_.empty())
    {
        // Load config.txt
        abakos::ConfigSettings cfg;
        if (abakos::SetupFileIO::loadConfigFile(dataDir_ / "config.txt", cfg))
            synthEngine_.setConfigSettings(cfg);

        // Load all 512 factory presets across banks a, b, c, d
        static const char* kBankFolders[4] = { "a", "b", "c", "d" };
        for (int b = 0; b < 4; ++b)
        {
            auto bankFolder = dataDir_ / kBankFolders[b];
            for (int p = 0; p < 128; ++p)
            {
                char fileBuf[16];
                std::snprintf(fileBuf, sizeof(fileBuf), "%02X.txt", p);
                auto filePath = bankFolder / fileBuf;
                if (!std::filesystem::exists(filePath))
                {
                    std::snprintf(fileBuf, sizeof(fileBuf), "%02x.txt", p);
                    filePath = bankFolder / fileBuf;
                }
                if (std::filesystem::exists(filePath))
                {
                    abakos::PresetData loaded;
                    if (abakos::SetupFileIO::loadPresetFile(filePath, loaded))
                    {
                        loaded.bankIndex    = b;
                        loaded.programIndex = p;
                        banks_[static_cast<size_t>(b)][static_cast<size_t>(p)] = loaded;
                    }
                }
            }
        }

        // Copy Bank A into External Bank 4 as initial fallback
        banks_[4] = banks_[0];

        // Load setup.txt initial bank & program
        int initBank = 0, initProg = 0;
        abakos::SetupFileIO::loadSetupFile(dataDir_ / "setup.txt", initBank, initProg);
        selectPreset(initBank, initProg);
    }
    else
    {
        selectPreset(0, 0);
    }
}

const juce::String AbakosPROAudioProcessor::getName() const
{
#ifdef JucePlugin_Name
    return JucePlugin_Name;
#else
    return "AbbaKhaos";
#endif
}

bool AbakosPROAudioProcessor::acceptsMidi() const { return true; }
bool AbakosPROAudioProcessor::producesMidi() const { return false; }
bool AbakosPROAudioProcessor::isMidiEffect() const { return false; }
double AbakosPROAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int AbakosPROAudioProcessor::getNumPrograms() { return 128; }

int AbakosPROAudioProcessor::getCurrentProgram()
{
    return currentProgram_.load(std::memory_order_relaxed);
}

void AbakosPROAudioProcessor::setCurrentProgram(int index)
{
    int prog = std::max(0, std::min(127, index));
    selectPreset(currentBank_.load(std::memory_order_relaxed), prog);
}

const juce::String AbakosPROAudioProcessor::getProgramName(int index)
{
    int prog = std::max(0, std::min(127, index));
    int bank = std::max(0, std::min(4, currentBank_.load(std::memory_order_relaxed)));
    const juce::ScopedLock sl(presetLock_);
    return juce::String(banks_[static_cast<size_t>(bank)][static_cast<size_t>(prog)].name);
}

void AbakosPROAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    int prog = std::max(0, std::min(127, index));
    int bank = std::max(0, std::min(4, currentBank_.load(std::memory_order_relaxed)));
    const juce::ScopedLock sl(presetLock_);
    banks_[static_cast<size_t>(bank)][static_cast<size_t>(prog)].name = newName.toStdString();
    if (prog == currentProgram_.load(std::memory_order_relaxed))
        activePresetName_ = newName;
}

void AbakosPROAudioProcessor::selectBank(int bankIndex)
{
    int b = std::max(0, std::min(4, bankIndex));
    selectPreset(b, currentProgram_.load(std::memory_order_relaxed));
}

void AbakosPROAudioProcessor::selectPreset(int bankIndex, int programIndex)
{
    int b = std::max(0, std::min(4, bankIndex));
    int p = std::max(0, std::min(127, programIndex));

    currentBank_.store(b, std::memory_order_relaxed);
    currentProgram_.store(p, std::memory_order_relaxed);

    abakos::PresetData presetCopy;
    {
        const juce::ScopedLock sl(presetLock_);
        presetCopy = banks_[static_cast<size_t>(b)][static_cast<size_t>(p)];
        activePresetName_     = presetCopy.name;
        activePresetCategory_ = presetCopy.category;
    }

    // Set bank indicator flags
    for (int i = 0; i < 5; ++i)
        presetCopy.params[static_cast<size_t>(abakos::kBankA + i)] = (i == b) ? 1 : 0;

    pushPresetToAPVTS(presetCopy);
    synthEngine_.applyPreset(presetCopy);

    if (!dataDir_.empty())
        abakos::SetupFileIO::saveSetupFile(dataDir_ / "setup.txt", b, p);
}

void AbakosPROAudioProcessor::stepPreset(int delta)
{
    int prog = currentProgram_.load(std::memory_order_relaxed) + delta;
    if (prog < 0) prog = 127;
    if (prog > 127) prog = 0;
    selectPreset(currentBank_.load(std::memory_order_relaxed), prog);
}

void AbakosPROAudioProcessor::pushPresetToAPVTS(const abakos::PresetData& preset)
{
    const auto& metaTable = abakos::getParamMetaTable();
    for (size_t i = 0; i < abakos::kNumParams; ++i)
    {
        if (auto* param = apvts_.getParameter(metaTable[i].id))
        {
            float plain = static_cast<float>(preset.params[i]);
            param->setValueNotifyingHost(param->convertTo0to1(plain));
        }
    }
}

abakos::PresetData AbakosPROAudioProcessor::captureCurrentPresetData() const
{
    abakos::PresetData data;
    data.bankIndex    = currentBank_.load(std::memory_order_relaxed);
    data.programIndex = currentProgram_.load(std::memory_order_relaxed);
    {
        const juce::ScopedLock sl(presetLock_);
        data.name     = activePresetName_.toStdString();
        data.category = activePresetCategory_.toStdString();
    }
    for (size_t i = 0; i < abakos::kNumParams; ++i)
    {
        if (paramPtrs_[i] != nullptr)
            data.params[i] = static_cast<int>(std::lround(paramPtrs_[i]->load(std::memory_order_relaxed)));
    }
    return data;
}

juce::String AbakosPROAudioProcessor::getCurrentPresetName() const
{
    const juce::ScopedLock sl(presetLock_);
    return activePresetName_;
}

juce::String AbakosPROAudioProcessor::getCurrentPresetCategory() const
{
    const juce::ScopedLock sl(presetLock_);
    return activePresetCategory_;
}

void AbakosPROAudioProcessor::setCurrentPresetName(const juce::String& name)
{
    int b = currentBank_.load(std::memory_order_relaxed);
    int p = currentProgram_.load(std::memory_order_relaxed);
    const juce::ScopedLock sl(presetLock_);
    activePresetName_ = name;
    banks_[static_cast<size_t>(b)][static_cast<size_t>(p)].name = name.toStdString();
}

bool AbakosPROAudioProcessor::loadExternalPresetFile(const juce::File& file)
{
    abakos::PresetData loaded;
    if (!abakos::SetupFileIO::loadPresetFile(file.getFullPathName().toStdString(), loaded))
        return false;

    int prog = currentProgram_.load(std::memory_order_relaxed);
    loaded.bankIndex    = 4; // External bank
    loaded.programIndex = prog;

    {
        const juce::ScopedLock sl(presetLock_);
        banks_[4][static_cast<size_t>(prog)] = loaded;
    }
    selectPreset(4, prog);
    return true;
}

bool AbakosPROAudioProcessor::saveCurrentPresetToFile(const juce::File& file)
{
    auto data = captureCurrentPresetData();
    {
        const juce::ScopedLock sl(presetLock_);
        banks_[static_cast<size_t>(data.bankIndex)][static_cast<size_t>(data.programIndex)] = data;
    }
    return abakos::SetupFileIO::savePresetFile(file.getFullPathName().toStdString(), data);
}

bool AbakosPROAudioProcessor::loadTuningFile(const juce::File& file)
{
    abakos::TuningScale scale;
    if (!abakos::SetupFileIO::loadTuningFile(file.getFullPathName().toStdString(), scale))
        return false;

    synthEngine_.setTuningScale(scale);
    const juce::ScopedLock sl(presetLock_);
    activeTuningName_ = juce::String(scale.name);
    return true;
}

void AbakosPROAudioProcessor::resetTuningTo12TET()
{
    abakos::TuningScale scale;
    scale.resetTo12TET();
    synthEngine_.setTuningScale(scale);
    const juce::ScopedLock sl(presetLock_);
    activeTuningName_ = "12-TET Standard";
}

juce::String AbakosPROAudioProcessor::getActiveTuningName() const
{
    const juce::ScopedLock sl(presetLock_);
    return activeTuningName_;
}

juce::File AbakosPROAudioProcessor::getTuningDirectory() const
{
    if (!dataDir_.empty() && std::filesystem::exists(dataDir_ / "Tuning Files"))
        return juce::File(juce::String((dataDir_ / "Tuning Files").string()));
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

juce::File AbakosPROAudioProcessor::getDataDirectory() const
{
    if (!dataDir_.empty())
        return juce::File(juce::String(dataDir_.string()));
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

void AbakosPROAudioProcessor::applyConfigSettings(const abakos::ConfigSettings& cfg, bool saveToDisk)
{
    synthEngine_.setConfigSettings(cfg);
    if (saveToDisk && !dataDir_.empty())
        abakos::SetupFileIO::saveConfigFile(dataDir_ / "config.txt", cfg);
}

void AbakosPROAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synthEngine_.prepare(sampleRate, samplesPerBlock);
    keyboardState_.reset();
    syncEngineFromAPVTS();
}

void AbakosPROAudioProcessor::releaseResources()
{
    keyboardState_.reset();
    synthEngine_.allNotesOff();
}

bool AbakosPROAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void AbakosPROAudioProcessor::syncEngineFromAPVTS() noexcept
{
    for (int i = 0; i < abakos::kNumParams; ++i)
    {
        if (paramPtrs_[static_cast<size_t>(i)] != nullptr)
        {
            float v = paramPtrs_[static_cast<size_t>(i)]->load(std::memory_order_relaxed);
            synthEngine_.setParameterValue(i, v);
        }
    }
}

void AbakosPROAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int i = 2; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Sync lock-free atomic parameters
    syncEngineFromAPVTS();

    // Sync host transport BPM for the 24-PPQN Arpeggiator
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                synthEngine_.setHostBpm(*bpm);
        }
    }

    // Merge GUI on-screen keyboard events into midiMessages
    keyboardState_.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    float* outL = buffer.getWritePointer(0);
    float* outR = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : outL;

    int currentSample = 0;

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const int msgSample = std::max(currentSample, std::min(numSamples, metadata.samplePosition));

        if (msgSample > currentSample)
        {
            synthEngine_.renderBlock(outL + currentSample, outR + currentSample, msgSample - currentSample);
            currentSample = msgSample;
        }

        if (msg.isNoteOn())
        {
            synthEngine_.noteOn(msg.getNoteNumber(), msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            synthEngine_.noteOff(msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            synthEngine_.allNotesOff();
        }
        else if (msg.isPitchWheel())
        {
            float normBend = (static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f;
            synthEngine_.setPitchBend(normBend);
        }
        else if (msg.isController())
        {
            if (msg.getControllerNumber() == 1) // Mod wheel CC#1
                synthEngine_.setModWheel(static_cast<float>(msg.getControllerValue()) / 127.0f);
            else if (msg.getControllerNumber() == 7) // Volume CC#7
                synthEngine_.setParameterValue(abakos::kMasterLevel, (static_cast<float>(msg.getControllerValue()) * 100.0f) / 127.0f);
        }
    }

    if (currentSample < numSamples)
        synthEngine_.renderBlock(outL + currentSample, outR + currentSample, numSamples - currentSample);

    if (totalNumOutputChannels == 1)
    {
        // Mono sum if host requested mono bus
        for (int s = 0; s < numSamples; ++s)
            outL[s] = 0.5f * (outL[s] + outR[s]);
    }
}

bool AbakosPROAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AbakosPROAudioProcessor::createEditor()
{
    return new AbakosPROAudioProcessorEditor(*this);
}

void AbakosPROAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute("currentBank", currentBank_.load(std::memory_order_relaxed));
        xml->setAttribute("currentProgram", currentProgram_.load(std::memory_order_relaxed));
        xml->setAttribute("presetName", getCurrentPresetName());
        xml->setAttribute("presetCategory", getCurrentPresetCategory());
        xml->setAttribute("tuningFile", juce::String(synthEngine_.getTuningScale().filePath));

        const auto& cfg = synthEngine_.getConfigSettings();
        xml->setAttribute("cfgPolyphony", cfg.polyphony);
        xml->setAttribute("cfgDcBlocking", cfg.dcBlockingFreq);
        xml->setAttribute("cfgOversampling", cfg.oversampling);
        xml->setAttribute("cfgHeadroom", cfg.headroom);
        xml->setAttribute("cfgMultitimbral", cfg.multitimbral);

        copyXmlToBinary(*xml, destData);
    }
}

void AbakosPROAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
    {
        currentBank_.store(xmlState->getIntAttribute("currentBank", 0), std::memory_order_relaxed);
        currentProgram_.store(xmlState->getIntAttribute("currentProgram", 0), std::memory_order_relaxed);

        {
            const juce::ScopedLock sl(presetLock_);
            activePresetName_     = xmlState->getStringAttribute("presetName", "Abakos Pro");
            activePresetCategory_ = xmlState->getStringAttribute("presetCategory", "SYNTH");
        }

        abakos::ConfigSettings cfg = synthEngine_.getConfigSettings();
        cfg.polyphony      = xmlState->getIntAttribute("cfgPolyphony", cfg.polyphony);
        cfg.dcBlockingFreq = xmlState->getIntAttribute("cfgDcBlocking", cfg.dcBlockingFreq);
        cfg.oversampling   = xmlState->getIntAttribute("cfgOversampling", cfg.oversampling);
        cfg.headroom       = xmlState->getIntAttribute("cfgHeadroom", cfg.headroom);
        cfg.multitimbral   = xmlState->getIntAttribute("cfgMultitimbral", cfg.multitimbral);
        synthEngine_.setConfigSettings(cfg);

        juce::String tunPath = xmlState->getStringAttribute("tuningFile");
        if (tunPath.isNotEmpty() && juce::File(tunPath).existsAsFile())
            loadTuningFile(juce::File(tunPath));

        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
        syncEngineFromAPVTS();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AbakosPROAudioProcessor();
}

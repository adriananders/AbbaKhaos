#include "SetupFileIO.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace abakos
{

const SExpNode* SExpNode::findChild(const std::string& childKey) const
{
    for (const auto& c : children)
    {
        if (c.key == childKey)
            return &c;
    }
    return nullptr;
}

std::vector<const SExpNode*> SExpNode::findChildren(const std::string& childKey) const
{
    std::vector<const SExpNode*> result;
    for (const auto& c : children)
    {
        if (c.key == childKey)
            result.push_back(&c);
    }
    return result;
}

int SExpNode::getIntAtom(size_t idx, int fallback) const
{
    if (idx >= atoms.size())
        return fallback;
    char* endPtr = nullptr;
    long v = std::strtol(atoms[idx].c_str(), &endPtr, 10);
    if (endPtr == atoms[idx].c_str())
        return fallback;
    return static_cast<int>(v);
}

std::string SExpNode::getStringAtom(size_t idx, const std::string& fallback) const
{
    if (idx >= atoms.size())
        return fallback;
    return atoms[idx];
}

namespace
{

enum class TokType
{
    Identifier,
    String,
    LBracket,
    RBracket
};

struct Token
{
    TokType type;
    std::string text;
};

std::vector<Token> tokenizeSExp(const std::string& src)
{
    std::vector<Token> tokens;
    size_t i = 0;
    const size_t n = src.size();

    while (i < n)
    {
        char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            ++i;
            continue;
        }
        if (c == '[')
        {
            tokens.push_back({ TokType::LBracket, "[" });
            ++i;
            continue;
        }
        if (c == ']')
        {
            tokens.push_back({ TokType::RBracket, "]" });
            ++i;
            continue;
        }
        if (c == '"')
        {
            ++i;
            std::string str;
            while (i < n && src[i] != '"')
            {
                str.push_back(src[i]);
                ++i;
            }
            if (i < n && src[i] == '"')
                ++i;
            tokens.push_back({ TokType::String, str });
            continue;
        }
        // Identifier or number
        std::string id;
        while (i < n && !std::isspace(static_cast<unsigned char>(src[i])) && src[i] != '[' && src[i] != ']' && src[i] != '"')
        {
            id.push_back(src[i]);
            ++i;
        }
        if (!id.empty())
            tokens.push_back({ TokType::Identifier, id });
    }
    return tokens;
}

void parseBracketBody(const std::vector<Token>& tokens, size_t& pos, SExpNode& node)
{
    while (pos < tokens.size())
    {
        if (tokens[pos].type == TokType::RBracket)
        {
            ++pos;
            return;
        }
        if ((tokens[pos].type == TokType::Identifier || tokens[pos].type == TokType::String) &&
            pos + 1 < tokens.size() && tokens[pos + 1].type == TokType::LBracket)
        {
            SExpNode child;
            child.key = tokens[pos].text;
            pos += 2; // consume key and '['
            parseBracketBody(tokens, pos, child);
            node.children.push_back(std::move(child));
        }
        else if (tokens[pos].type == TokType::LBracket)
        {
            SExpNode anon;
            ++pos;
            parseBracketBody(tokens, pos, anon);
            node.children.push_back(std::move(anon));
        }
        else
        {
            node.atoms.push_back(tokens[pos].text);
            ++pos;
        }
    }
}

int clampInt(int v, int lo, int hi)
{
    return std::max(lo, std::min(hi, v));
}

int map16383To100(int raw)
{
    return clampInt(static_cast<int>(std::lround((static_cast<double>(raw) * 100.0) / 16383.0)), 0, 100);
}

int map100To16383(int val)
{
    return clampInt(static_cast<int>(std::lround((static_cast<double>(val) * 16383.0) / 100.0)), 0, 16383);
}

void splitPitch768(int rawFreq, int& outSemis, int& outCents)
{
    // In HERCs Abakos PRO: 768 units = 1 octave = 12 semitones => 64 units = 1 semitone
    double totalSemis = static_cast<double>(rawFreq) / 64.0;
    int semis = static_cast<int>(std::lround(totalSemis));
    semis = clampInt(semis, -24, 24);
    double remSemis = totalSemis - static_cast<double>(semis);
    int cents = static_cast<int>(std::lround(remSemis * 100.0));
    cents = clampInt(cents, -50, 50);
    outSemis = semis;
    outCents = cents;
}

int combinePitch768(int semis, int cents)
{
    double totalSemis = static_cast<double>(semis) + static_cast<double>(cents) / 100.0;
    return static_cast<int>(std::lround(totalSemis * 64.0));
}

int mapAttenuationTo100(int rawAmp)
{
    // rawAmp is in [-16383..0], where 0 = 100% level and -16383 = 0% level
    if (rawAmp >= 0)
        return 100;
    if (rawAmp <= -16383)
        return 0;
    return clampInt(static_cast<int>(std::lround((1.0 + static_cast<double>(rawAmp) / 16383.0) * 100.0)), 0, 100);
}

int map100ToAttenuation(int level)
{
    level = clampInt(level, 0, 100);
    return static_cast<int>(std::lround(((static_cast<double>(level) / 100.0) - 1.0) * 16383.0));
}

} // namespace

bool SetupFileIO::parseSExp(const std::string& text, SExpNode& outRoot)
{
    outRoot = SExpNode {};
    outRoot.key = "root";
    auto tokens = tokenizeSExp(text);
    if (tokens.empty())
        return false;

    size_t pos = 0;
    parseBracketBody(tokens, pos, outRoot);
    return !outRoot.children.empty() || !outRoot.atoms.empty();
}

bool SetupFileIO::loadConfigFile(const std::filesystem::path& path, ConfigSettings& outConfig)
{
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();

    SExpNode root;
    if (!parseSExp(ss.str(), root))
        return false;

    if (const auto* n = root.findChild("polyphony"))
        outConfig.polyphony = clampInt(n->getIntAtom(0, 8), 1, 64);
    if (const auto* n = root.findChild("dc_blocking_freq"))
        outConfig.dcBlockingFreq = clampInt(n->getIntAtom(0, 12), 1, 100);
    if (const auto* n = root.findChild("oversampling"))
        outConfig.oversampling = clampInt(n->getIntAtom(0, 1), 1, 4);
    if (const auto* n = root.findChild("headroom"))
        outConfig.headroom = clampInt(n->getIntAtom(0, 2), 1, 8);
    if (const auto* n = root.findChild("multitimbral"))
        outConfig.multitimbral = clampInt(n->getIntAtom(0, 0), 0, 1);

    return true;
}

bool SetupFileIO::saveConfigFile(const std::filesystem::path& path, const ConfigSettings& config)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return false;

    out << "polyphony [" << config.polyphony << "]\n"
        << "dc_blocking_freq [" << config.dcBlockingFreq << "]\n"
        << "oversampling [" << config.oversampling << "]\n"
        << "headroom [" << config.headroom << "]\n"
        << "multitimbral [" << config.multitimbral << "]\n";
    return true;
}

bool SetupFileIO::loadSetupFile(const std::filesystem::path& path, int& outBank, int& outProgram)
{
    outBank    = 0;
    outProgram = 0;

    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();

    SExpNode root;
    if (!parseSExp(ss.str(), root))
        return false;

    const SExpNode* scope = &root;
    if (const auto* ch = root.findChild("channel"))
        scope = ch;

    if (const auto* msb = scope->findChild("msb"))
        outBank = clampInt(msb->getIntAtom(0, 0), 0, 4);
    if (const auto* prog = scope->findChild("program"))
        outProgram = clampInt(prog->getIntAtom(0, 0), 0, 127);
    return true;
}

bool SetupFileIO::saveSetupFile(const std::filesystem::path& path, int bank, int program)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return false;

    out << "msb [" << clampInt(bank, 0, 4) << "]\n"
        << "program [" << clampInt(program, 0, 127) << "]\n";
    return true;
}

bool SetupFileIO::parsePresetText(const std::string& text, PresetData& outPreset)
{
    SExpNode root;
    if (!parseSExp(text, root))
        return false;

    const SExpNode* prog = root.findChild("program");
    if (!prog)
        return false;

    outPreset = PresetData {};

    if (const auto* cat = prog->findChild("category"))
        outPreset.category = cat->getStringAtom(0, "SYNTH");
    if (const auto* nm = prog->findChild("name"))
        outPreset.name = nm->getStringAtom(0, "INIT PRO");

    // Global mode & portamento
    outPreset.params[kMode] = (prog->findChild("mono") != nullptr) ? 1 : 0;
    if (const auto* pt = prog->findChild("porta_time"))
        outPreset.params[kPortamento] = map16383To100(pt->getIntAtom(0, 0));
    else if (prog->findChild("porta_on") != nullptr)
        outPreset.params[kPortamento] = 25;
    else
        outPreset.params[kPortamento] = 0;

    // Arpeggiator
    outPreset.params[kArpOnOff] = (prog->findChild("arpeggiator_on") != nullptr) ? 1 : 0;
    outPreset.params[kArpHold]  = (prog->findChild("arpeggiator_hold_on") != nullptr) ? 1 : 0;
    if (const auto* pat = prog->findChild("pattern"))
        outPreset.params[kArpPattern] = clampInt(pat->getIntAtom(2, 0), 0, 23);

    // Global LFO controls
    if (const auto* spd = prog->findChild("speed"))
        outPreset.params[kLfoRate] = map16383To100(spd->getIntAtom(0, 5734));
    if (const auto* vib = prog->findChild("vibrato"))
        outPreset.params[kLfoPitch] = map16383To100(vib->getIntAtom(0, 0));
    else if (const auto* vx = prog->findChild("vector_x"))
        outPreset.params[kLfoPitch] = map16383To100(std::abs(vx->getIntAtom(0, 0) - 8192));
    if (const auto* trm = prog->findChild("tremolo"))
        outPreset.params[kLfoFilter] = map16383To100(trm->getIntAtom(0, 0));
    if (const auto* lpan = prog->findChild("lfo_pan"))
        outPreset.params[kLfoPan] = map16383To100(lpan->getIntAtom(0, 0));

    // Master Volume & Pan
    if (const auto* vol = prog->findChild("volume"))
        outPreset.params[kMasterLevel] = map16383To100(vol->getIntAtom(0, 13925));
    if (const auto* pn = prog->findChild("pan"))
        outPreset.pan = pn->getIntAtom(0, 0);

    // Delay Mix & DSP block
    if (const auto* dw = prog->findChild("dry_wet"))
        outPreset.params[kDelayMix] = map16383To100(dw->getIntAtom(0, 0));
    else
        outPreset.params[kDelayMix] = 0;

    if (const auto* dspNode = prog->findChild("dsp"))
    {
        auto dspParams = dspNode->findChildren("parameter");
        for (const auto* dp : dspParams)
        {
            int pIdx = dp->getIntAtom(0, -1);
            int pVal = dp->getIntAtom(1, 0);
            if (pIdx == 0)
                outPreset.params[kDelay1] = map16383To100(pVal);
            else if (pIdx == 1)
            {
                outPreset.params[kDelay2] = map16383To100(pVal);
                outPreset.params[kDelayFeedback] = map16383To100(pVal);
            }
            else if (pIdx == 2)
                outPreset.params[kDelayFeedback] = map16383To100(pVal);
        }
    }

    // Top-level algo_parameter (if present outside stripe)
    for (const auto* ap : prog->findChildren("algo_parameter"))
    {
        int code = ap->getIntAtom(0, -1);
        int val  = ap->getIntAtom(1, 0);
        if (code == 124)
            outPreset.params[kOsc1Wave] = clampInt(val, 0, 5);
        else if (code == 123)
            outPreset.params[kFilterResonance] = map16383To100(val);
        else if (code == 121)
            outPreset.params[kFilterHpfLpf] = (val != 0) ? 1 : 0;
    }

    // Parse stripes 0 (OSC 1), 1 (OSC 2), 2 (Filter & Noise)
    for (const auto* st : prog->findChildren("stripe"))
    {
        const auto* stIdNode = st->findChild("stripe");
        int stIdx = stIdNode ? stIdNode->getIntAtom(0, -1) : -1;

        if (stIdx == 0)
        {
            // Stripe 0: OSC 1 + ADS1 + ADSR1
            int rawFreq = 0;
            if (const auto* fn = st->findChild("freq"))
                rawFreq = fn->getIntAtom(0, 0);
            splitPitch768(rawFreq, outPreset.params[kOsc1Semis], outPreset.params[kOsc1Cents]);

            if (const auto* ampNode = st->findChild("amp"))
                outPreset.params[kOsc1Level] = mapAttenuationTo100(ampNode->getIntAtom(0, 0));
            else
                outPreset.params[kOsc1Level] = 100;

            if (const auto* idxNode = st->findChild("index"))
                outPreset.params[kNoisePitchMod] = map16383To100(idxNode->getIntAtom(0, 0));

            for (const auto* ap : st->findChildren("algo_parameter"))
            {
                int code = ap->getIntAtom(0, -1);
                int val  = ap->getIntAtom(1, 0);
                if (code == 124)
                    outPreset.params[kOsc1Wave] = clampInt(val, 0, 5);
            }

            if (const auto* feg = st->findChild("freq_eg"))
            {
                if (const auto* lv = feg->findChild("level"))
                {
                    int modRaw = lv->getIntAtom(1, 0);
                    outPreset.params[kAds1Mod] = clampInt(static_cast<int>(std::lround(static_cast<double>(modRaw) * 50.0 / 16383.0)), -50, 50);
                    int susRaw = lv->getIntAtom(2, 0);
                    outPreset.params[kAds1Sustain] = map16383To100( std::abs(susRaw) );
                }
                if (const auto* tm = feg->findChild("time"))
                {
                    outPreset.params[kAds1Attack] = map16383To100(tm->getIntAtom(0, 0));
                    outPreset.params[kAds1Decay]  = map16383To100(tm->getIntAtom(1, 0));
                }
            }

            if (const auto* aeg = st->findChild("amp_eg"))
            {
                if (const auto* lv = aeg->findChild("level"))
                    outPreset.params[kAdsr1Sustain] = map16383To100(lv->getIntAtom(2, 16383));
                if (const auto* tm = aeg->findChild("time"))
                {
                    outPreset.params[kAdsr1Attack]  = map16383To100(tm->getIntAtom(0, 0));
                    outPreset.params[kAdsr1Decay]   = map16383To100(tm->getIntAtom(1, 0));
                    outPreset.params[kAdsr1Release] = map16383To100(tm->getIntAtom(3, 2000));
                }
            }
        }
        else if (stIdx == 1)
        {
            // Stripe 1: OSC 2 + Sync + ADSR2
            int rawFreq = 0;
            if (const auto* fn = st->findChild("freq"))
                rawFreq = fn->getIntAtom(0, 0);
            splitPitch768(rawFreq, outPreset.params[kOsc2Semis], outPreset.params[kOsc2Cents]);

            if (const auto* ampNode = st->findChild("amp"))
                outPreset.params[kOsc2Level] = mapAttenuationTo100(ampNode->getIntAtom(0, 0));
            else
                outPreset.params[kOsc2Level] = 100;

            if (const auto* idxNode = st->findChild("index"))
                outPreset.params[kOsc2Sync] = map16383To100(idxNode->getIntAtom(0, 0));
            else
                outPreset.params[kOsc2Sync] = 0;

            if (const auto* algoNode = st->findChild("algo"))
            {
                outPreset.subAlgo = algoNode->getIntAtom(2, 0);
                outPreset.params[kOsc2Wave] = clampInt(outPreset.subAlgo % 6, 0, 5);
            }

            if (const auto* aeg = st->findChild("amp_eg"))
            {
                if (const auto* lv = aeg->findChild("level"))
                    outPreset.params[kAdsr2Sustain] = map16383To100(lv->getIntAtom(2, 16383));
                if (const auto* tm = aeg->findChild("time"))
                {
                    outPreset.params[kAdsr2Attack]  = map16383To100(tm->getIntAtom(0, 0));
                    outPreset.params[kAdsr2Decay]   = map16383To100(tm->getIntAtom(1, 0));
                    outPreset.params[kAdsr2Release] = map16383To100(tm->getIntAtom(3, 2000));
                }
            }
        }
        else if (stIdx == 2)
        {
            // Stripe 2: Filter & Noise + ADS2 Filter Envelope
            if (const auto* fn = st->findChild("freq"))
            {
                // Filter freq in presets typically ranges around [-4608..+4608]
                int rawCut = fn->getIntAtom(0, 0);
                int cut100 = clampInt(static_cast<int>(std::lround(((static_cast<double>(rawCut) + 4608.0) / 9216.0) * 100.0)), 0, 100);
                outPreset.params[kFilterCutoff] = cut100;
            }

            if (const auto* ampNode = st->findChild("amp"))
            {
                int rawNoiseAmp = ampNode->getIntAtom(0, -16383);
                outPreset.params[kNoiseLevel] = mapAttenuationTo100(rawNoiseAmp);
            }
            else
            {
                outPreset.params[kNoiseLevel] = 0;
            }

            if (const auto* idxNode = st->findChild("index"))
            {
                int rawColor = idxNode->getIntAtom(0, 8192);
                outPreset.params[kNoiseColor] = clampInt(static_cast<int>(std::lround(((static_cast<double>(rawColor) - 8192.0) / 8192.0) * 50.0)), -50, 50);
            }

            for (const auto* ap : st->findChildren("algo_parameter"))
            {
                int code = ap->getIntAtom(0, -1);
                int val  = ap->getIntAtom(1, 0);
                if (code == 123)
                    outPreset.params[kFilterResonance] = map16383To100(val);
                else if (code == 121)
                    outPreset.params[kFilterHpfLpf] = (val != 0) ? 1 : 0;
                else if (code == 122)
                    outPreset.params[kFilterPitchMod] = map16383To100(val);
            }

            if (const auto* feg = st->findChild("freq_eg"))
            {
                if (const auto* lv = feg->findChild("level"))
                {
                    int modRaw = lv->getIntAtom(1, 0);
                    outPreset.params[kAds2Mod] = clampInt(static_cast<int>(std::lround(static_cast<double>(modRaw) * 50.0 / 16383.0)), -50, 50);
                    int susRaw = lv->getIntAtom(2, 0);
                    outPreset.params[kAds2Sustain] = map16383To100(std::abs(susRaw));
                }
                if (const auto* tm = feg->findChild("time"))
                {
                    outPreset.params[kAds2Attack] = map16383To100(tm->getIntAtom(0, 0));
                    outPreset.params[kAds2Decay]  = map16383To100(tm->getIntAtom(1, 0));
                }
            }
        }
    }

    return true;
}

bool SetupFileIO::loadPresetFile(const std::filesystem::path& path, PresetData& outPreset)
{
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    return parsePresetText(ss.str(), outPreset);
}

std::string SetupFileIO::serializePresetText(const PresetData& p)
{
    std::ostringstream out;
    out << "program [\n";
    out << " category [\"" << p.category << "\"]\n";
    out << " name [\"" << p.name << "\"]\n";
    out << " algo [\"PRESET ALGO BANK\" 0]\n";
    if (p.params[kArpPattern] > 0 || p.params[kArpOnOff] != 0)
        out << " pattern [\"PRESET PATTERN BANK\" 0 " << clampInt(p.params[kArpPattern], 0, 23) << "]\n";

    if (p.params[kLfoRate] > 0)
        out << " speed [" << map100To16383(p.params[kLfoRate]) << "]\n";
    if (p.params[kLfoPitch] > 0)
        out << " vibrato [" << map100To16383(p.params[kLfoPitch]) << "]\n";
    if (p.params[kLfoFilter] > 0)
        out << " tremolo [" << map100To16383(p.params[kLfoFilter]) << "]\n";
    if (p.params[kLfoPan] > 0)
        out << " lfo_pan [" << map100To16383(p.params[kLfoPan]) << "]\n";
    if (p.pan != 0)
        out << " pan [" << p.pan << "]\n";
    out << " volume [" << map100To16383(p.params[kMasterLevel]) << "]\n";

    // Stripe 0 (OSC 1)
    int osc1Freq = combinePitch768(p.params[kOsc1Semis], p.params[kOsc1Cents]);
    int osc1Amp  = map100ToAttenuation(p.params[kOsc1Level]);
    int ads1ModRaw = static_cast<int>(std::lround(static_cast<double>(p.params[kAds1Mod]) * 16383.0 / 50.0));
    int ads1SusRaw = map100To16383(p.params[kAds1Sustain]);

    out << " stripe [\n";
    out << "  stripe [0]\n";
    if (p.params[kOsc1Wave] > 0)
        out << "  algo_parameter [124 " << p.params[kOsc1Wave] << "]\n";
    if (osc1Freq != 0)
        out << "  freq [" << osc1Freq << "]\n";
    if (osc1Amp != 0)
        out << "  amp [" << osc1Amp << "]\n";
    if (p.params[kNoisePitchMod] > 0)
        out << "  index [" << map100To16383(p.params[kNoisePitchMod]) << "]\n";
    out << "  freq_eg [\n";
    out << "   level [0 " << ads1ModRaw << " " << ads1SusRaw << " 0]\n";
    out << "   time [" << map100To16383(p.params[kAds1Attack]) << " "
        << map100To16383(p.params[kAds1Decay]) << " 0 0]\n";
    out << "  ]\n";
    out << "  amp_eg [\n";
    out << "   level [0 16383 " << map100To16383(p.params[kAdsr1Sustain]) << " 16383]\n";
    out << "   time [" << map100To16383(p.params[kAdsr1Attack]) << " "
        << map100To16383(p.params[kAdsr1Decay]) << " 0 "
        << map100To16383(p.params[kAdsr1Release]) << "]\n";
    out << "  ]\n";
    out << " ]\n";

    // Stripe 1 (OSC 2)
    int osc2Freq = combinePitch768(p.params[kOsc2Semis], p.params[kOsc2Cents]);
    int osc2Amp  = map100ToAttenuation(p.params[kOsc2Level]);
    int subAlgo  = (p.subAlgo / 6) * 6 + clampInt(p.params[kOsc2Wave], 0, 5);

    out << " stripe [\n";
    out << "  stripe [1]\n";
    out << "  algo [\"PRESET ALGO BANK\" 5 " << subAlgo << "]\n";
    if (osc2Freq != 0)
        out << "  freq [" << osc2Freq << "]\n";
    if (osc2Amp != 0)
        out << "  amp [" << osc2Amp << "]\n";
    if (p.params[kOsc2Sync] > 0)
        out << "  index [" << map100To16383(p.params[kOsc2Sync]) << "]\n";
    out << "  amp_eg [\n";
    out << "   level [0 16383 " << map100To16383(p.params[kAdsr2Sustain]) << " 16383]\n";
    out << "   time [" << map100To16383(p.params[kAdsr2Attack]) << " "
        << map100To16383(p.params[kAdsr2Decay]) << " 0 "
        << map100To16383(p.params[kAdsr2Release]) << "]\n";
    out << "  ]\n";
    out << " ]\n";

    // Stripe 2 (Filter & Noise)
    int rawFilterFreq = static_cast<int>(std::lround((static_cast<double>(p.params[kFilterCutoff]) / 100.0) * 9216.0 - 4608.0));
    int rawNoiseAmp   = map100ToAttenuation(p.params[kNoiseLevel]);
    int rawNoiseColor = static_cast<int>(std::lround(((static_cast<double>(p.params[kNoiseColor]) / 50.0) + 1.0) * 8192.0));
    int ads2ModRaw    = static_cast<int>(std::lround(static_cast<double>(p.params[kAds2Mod]) * 16383.0 / 50.0));
    int ads2SusRaw    = map100To16383(p.params[kAds2Sustain]);

    out << " stripe [\n";
    out << "  stripe [2]\n";
    if (p.params[kFilterHpfLpf] != 0)
        out << "  algo_parameter [121 " << p.params[kFilterHpfLpf] << "]\n";
    out << "  algo_parameter [122 " << map100To16383(p.params[kFilterPitchMod]) << "]\n";
    if (p.params[kFilterResonance] > 0)
        out << "  algo_parameter [123 " << map100To16383(p.params[kFilterResonance]) << "]\n";
    out << "  freq [" << rawFilterFreq << "]\n";
    if (rawNoiseAmp != 0)
        out << "  amp [" << rawNoiseAmp << "]\n";
    if (rawNoiseColor != 8192)
        out << "  index [" << rawNoiseColor << "]\n";
    out << "  freq_eg [\n";
    out << "   level [0 " << ads2ModRaw << " " << ads2SusRaw << " 0]\n";
    out << "   time [" << map100To16383(p.params[kAds2Attack]) << " "
        << map100To16383(p.params[kAds2Decay]) << " 0 0]\n";
    out << "  ]\n";
    out << " ]\n";

    if (p.params[kMode] != 0)
        out << " mono []\n";
    if (p.params[kPortamento] > 0)
    {
        out << " porta_on []\n";
        out << " porta_time [" << map100To16383(p.params[kPortamento]) << "]\n";
    }
    if (p.params[kArpOnOff] != 0)
        out << " arpeggiator_on []\n";
    if (p.params[kArpHold] != 0)
        out << " arpeggiator_hold_on []\n";

    if (p.params[kDelayMix] > 0)
    {
        out << " dry_wet [" << map100To16383(p.params[kDelayMix]) << "]\n";
        out << " dsp [\n";
        out << "  dsp [0]\n";
        out << "  algo [\"PRESET DSP ALGO BANK\" 0]\n";
        out << "  parameter [0 " << map100To16383(p.params[kDelay1]) << "]\n";
        out << "  parameter [1 " << map100To16383(p.params[kDelay2]) << "]\n";
        out << "  parameter [2 " << map100To16383(p.params[kDelayFeedback]) << "]\n";
        out << " ]\n";
    }

    out << "]\n";
    return out.str();
}

bool SetupFileIO::savePresetFile(const std::filesystem::path& path, const PresetData& preset)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return false;
    out << serializePresetText(preset);
    return true;
}

bool SetupFileIO::parseTuningText(const std::string& text, const std::string& nameHint, TuningScale& outScale)
{
    outScale.resetTo12TET();
    if (!nameHint.empty())
        outScale.name = nameHint;

    std::istringstream ss(text);
    std::string line;
    int parsedCount = 0;

    while (std::getline(ss, line))
    {
        // Trim leading whitespace
        size_t start = 0;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start])))
            ++start;
        if (start >= line.size() || line[start] == ';' || line[start] == '#' || line[start] == '[')
            continue;

        // Match "note <N>=<cents>" (case-insensitive for "note")
        std::string sub = line.substr(start);
        if (sub.size() >= 5 &&
            (sub.compare(0, 5, "note ") == 0 || sub.compare(0, 5, "Note ") == 0 || sub.compare(0, 5, "NOTE ") == 0))
        {
            size_t eqPos = sub.find('=', 5);
            if (eqPos != std::string::npos)
            {
                std::string idxStr = sub.substr(5, eqPos - 5);
                std::string valStr = sub.substr(eqPos + 1);
                char* endIdx = nullptr;
                long noteIdx = std::strtol(idxStr.c_str(), &endIdx, 10);
                char* endVal = nullptr;
                float centsVal = std::strtof(valStr.c_str(), &endVal);
                if (endIdx != idxStr.c_str() && endVal != valStr.c_str() && noteIdx >= 0 && noteIdx < 128)
                {
                    outScale.noteCents[static_cast<size_t>(noteIdx)] = centsVal;
                    ++parsedCount;
                }
            }
        }
    }

    return parsedCount > 0;
}

bool SetupFileIO::loadTuningFile(const std::filesystem::path& path, TuningScale& outScale)
{
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();

    std::string stem = path.stem().string();
    // Strip trailing ".scl" if filename was "*.scl.tun"
    if (stem.size() > 4 && stem.substr(stem.size() - 4) == ".scl")
        stem = stem.substr(0, stem.size() - 4);

    if (!parseTuningText(ss.str(), stem, outScale))
        return false;

    outScale.filePath = path.string();
    return true;
}

} // namespace abakos

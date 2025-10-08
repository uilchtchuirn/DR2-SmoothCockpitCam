#include "stdafx.h"
#include "Config.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <optional>
#include <vector>

#include "MessageHandler.h"

namespace fs = std::filesystem;

namespace IGCS
{
    // ---------------- string helpers ----------------
    static inline void ltrim(std::string& s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](unsigned char ch) { return !std::isspace(ch); }));
    }
    static inline void rtrim(std::string& s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(),
            [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    }
    static inline void trim(std::string& s) { ltrim(s); rtrim(s); }
    static inline std::string toLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::string narrow(const std::wstring& w)
    {
        if (w.empty()) return {};
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
            static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
            result.data(), sizeNeeded, nullptr, nullptr);
        return result;
    }

    // ---------------- parsing helpers ----------------

    static bool tryParseUInt16(const std::string& s, uint16_t& out)
    {
        try
        {
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
            {
                out = static_cast<uint16_t>(std::stoul(s, nullptr, 16));
            }
            else
            {
                out = static_cast<uint16_t>(std::stoul(s, nullptr, 10));
            }
            return true;
        }
        catch (...) { return false; }
    }

    static std::optional<uint16_t> parseGamepadButton(const std::string& raw)
    {
        std::string v = toLower(raw);
        trim(v);
        if (v == "lb") v = "leftshoulder";
        if (v == "rb") v = "rightshoulder";
        if (v == "ls") v = "leftthumb";
        if (v == "rs") v = "rightthumb";
        if (v == "up") v = "dpadup";
        if (v == "down") v = "dpaddown";
        if (v == "left") v = "dpadleft";
        if (v == "right") v = "dpadright";

        if (v == "dpadup" || v == "dpad_up") return XINPUT_GAMEPAD_DPAD_UP;
        if (v == "dpaddown" || v == "dpad_down") return XINPUT_GAMEPAD_DPAD_DOWN;
        if (v == "dpadleft" || v == "dpad_left") return XINPUT_GAMEPAD_DPAD_LEFT;
        if (v == "dpadright" || v == "dpad_right") return XINPUT_GAMEPAD_DPAD_RIGHT;
        if (v == "start") return XINPUT_GAMEPAD_START;
        if (v == "back") return XINPUT_GAMEPAD_BACK;
        if (v == "leftthumb" || v == "leftstick") return XINPUT_GAMEPAD_LEFT_THUMB;
        if (v == "rightthumb" || v == "rightstick") return XINPUT_GAMEPAD_RIGHT_THUMB;
        if (v == "leftshoulder") return XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (v == "rightshoulder") return XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (v == "a") return XINPUT_GAMEPAD_A;
        if (v == "b") return XINPUT_GAMEPAD_B;
        if (v == "x") return XINPUT_GAMEPAD_X;
        if (v == "y") return XINPUT_GAMEPAD_Y;

        uint16_t numeric = 0;
        if (tryParseUInt16(v, numeric))
        {
            return numeric;
        }
        return std::nullopt;
    }

    static const char* maskToName(uint16_t m)
    {
        switch (m)
        {
        case XINPUT_GAMEPAD_DPAD_UP: return "DPadUp";
        case XINPUT_GAMEPAD_DPAD_DOWN: return "DPadDown";
        case XINPUT_GAMEPAD_DPAD_LEFT: return "DPadLeft";
        case XINPUT_GAMEPAD_DPAD_RIGHT: return "DPadRight";
        case XINPUT_GAMEPAD_START: return "Start";
        case XINPUT_GAMEPAD_BACK: return "Back";
        case XINPUT_GAMEPAD_LEFT_THUMB: return "LeftThumb";
        case XINPUT_GAMEPAD_RIGHT_THUMB: return "RightThumb";
        case XINPUT_GAMEPAD_LEFT_SHOULDER: return "LeftShoulder";
        case XINPUT_GAMEPAD_RIGHT_SHOULDER: return "RightShoulder";
        case XINPUT_GAMEPAD_A: return "A";
        case XINPUT_GAMEPAD_B: return "B";
        case XINPUT_GAMEPAD_X: return "X";
        case XINPUT_GAMEPAD_Y: return "Y";
        default: return nullptr;
        }
    }

    // Valid single-bit XInput buttons we allow (prevents multi-bit masks)
    static bool isAllowedSingleButton(uint16_t mask)
    {
        static const uint16_t allowed[] = {
            XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN,
            XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
            XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK,
            XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
            XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
            XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y
        };
        for (auto a : allowed) if (a == mask) return true;
        return false;
    }

    static bool isSingleBit(uint16_t v) { return v && ((v & (v - 1)) == 0); }

    // -------------------------------------------------

    const Settings& Config::get()
    {
        static Settings s = load();
        return s;
    }

    std::wstring Config::findConfigPath()
    {
        HMODULE hMod = nullptr;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&Config::get),
            &hMod
        );

        wchar_t modulePath[MAX_PATH] = {};
        if (hMod && GetModuleFileNameW(hMod, modulePath, static_cast<DWORD>(std::size(modulePath))) > 0)
        {
            fs::path p(modulePath);
            return (p.parent_path() / L"dr2tools.cfg").wstring();
        }
        return (fs::current_path() / L"dr2tools.cfg").wstring();
    }

    Settings Config::load()
    {
        Settings result; // starts with compile-time defaults
        bool blendFromIni = false;
        bool gamepadFromIni = false;
        bool diToggleFromIni = false;

        const std::wstring cfgPath = findConfigPath();
        const std::string cfgPathUtf8 = narrow(cfgPath);
        MessageHandler::logLine("Config: loading dr2tools.cfg from '%s'", cfgPathUtf8.c_str());

        std::ifstream in(cfgPath);
        if (!in.is_open())
        {
            MessageHandler::logLine("Config: file not found. Using built-in defaults.");
            MessageHandler::logLine("Config: blend=%.6f (default)", result.blend);
            {
                const uint16_t m = result.cameraEnableGamepadMask;
                const char* name = maskToName(m);
                if (name) MessageHandler::logLine("Config: camera_enable_gamepad=%s (0x%04X) (default)", name, m);
                else MessageHandler::logLine("Config: camera_enable_gamepad=0x%04X (default)", m);
            }
            MessageHandler::logLine("Config: direct_input_toggle_button=%d (default)", result.directInputToggleButtonIndex);
            return result;
        }

        std::string line;
        while (std::getline(in, line))
        {
            auto commentPos = line.find_first_of("#;");
            if (commentPos != std::string::npos) line.erase(commentPos);
            trim(line);
            if (line.empty()) continue;
            if (line.front() == '[' && line.back() == ']') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            trim(key); trim(val);
            std::string keyLower = toLower(key);

            if (keyLower == "consoleenabled")
            {
                result.ConsoleEnabled = (val == "true" || val == "1");

            }
            else if (keyLower == "blend")
            {
                try
                {
                    float parsed = std::stof(val);
                    result.blend = parsed;
                    blendFromIni = true;
                    MessageHandler::logLine("Config: read blend=%.6f from ini", parsed);
                }
                catch (...)
                {
                    MessageHandler::logError("Config: invalid value for 'blend' ('%s'). Using default %.6f.", val.c_str(), result.blend);
                }
            }
            else if (keyLower == "camera_enable_gamepad")
            {
                auto parsed = parseGamepadButton(val);
                if (!parsed.has_value())
                {
                    MessageHandler::logError("Config: invalid value for 'camera_enable_gamepad' ('%s'). Keeping default (%s).",
                        val.c_str(), maskToName(result.cameraEnableGamepadMask) ? maskToName(result.cameraEnableGamepadMask) : "unknown");
                    continue;
                }
                uint16_t candidate = parsed.value();
                if (!(isSingleBit(candidate) && isAllowedSingleButton(candidate)))
                {
                    MessageHandler::logError("Config: camera_enable_gamepad value '%s' (0x%04X) is not a supported single button. Keeping default (%s).",
                        val.c_str(), candidate, maskToName(result.cameraEnableGamepadMask) ? maskToName(result.cameraEnableGamepadMask) : "unknown");
                    continue;
                }
                result.cameraEnableGamepadMask = candidate;
                gamepadFromIni = true;
                if (const char* nm = maskToName(candidate))
                    MessageHandler::logLine("Config: read camera_enable_gamepad=%s (0x%04X) from ini", nm, candidate);
                else
                    MessageHandler::logLine("Config: read camera_enable_gamepad=0x%04X from ini", candidate);
            }
            else if (keyLower == "direct_input_toggle_button")
            {
                try
                {
                    int parsed = std::stoi(val);
                    if (parsed < 0 || parsed > 127)
                    {
                        MessageHandler::logError(
                            "Config: direct_input_toggle_button value '%s' out of range (0..127). Keeping default (%d).",
                            val.c_str(), result.directInputToggleButtonIndex);
                    }
                    else
                    {
                        result.directInputToggleButtonIndex = parsed;
                        diToggleFromIni = true;
                        MessageHandler::logLine("Config: read direct_input_toggle_button=%d from ini", parsed);
                    }
                }
                catch (...)
                {
                    MessageHandler::logError(
                        "Config: invalid value for 'direct_input_toggle_button' ('%s'). Keeping default (%d).",
                        val.c_str(), result.directInputToggleButtonIndex);
                }
            }
        }

        if (!blendFromIni)
        {
            MessageHandler::logLine("Config: blend not specified. Using default %.6f.", result.blend);
        }
        if (!gamepadFromIni)
        {
            auto m = result.cameraEnableGamepadMask;
            if (const char* nm = maskToName(m))
                MessageHandler::logLine("Config: camera_enable_gamepad not specified. Using default %s (0x%04X).", nm, m);
            else
                MessageHandler::logLine("Config: camera_enable_gamepad not specified. Using default 0x%04X.", m);
        }
        if (!diToggleFromIni)
        {
            MessageHandler::logLine("Config: direct_input_toggle_button not specified. Using default %d.",
                result.directInputToggleButtonIndex);
        }

        return result;
    }
}
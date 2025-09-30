#pragma once
#include <string>
#include <cstdint>
#include <windows.h>
#include <Xinput.h>

namespace IGCS
{
    struct Settings
    {
        // Centralized, compile-time defaults
        static constexpr float   kDefaultBlend = 0.12f;
        static constexpr uint16_t kDefaultCameraEnableGamepadMask = XINPUT_GAMEPAD_RIGHT_THUMB;
        static constexpr int      kDefaultDirectInputToggleButtonIndex = 12;
        static constexpr bool     kDefaultConsoleEnabled = true;
        static constexpr bool     kDefaultAutoRestoreWndw = false;
        static constexpr int      kDefaultAltTabWait = 2000;

        // Initialized with defaults. If the INI omits a value or parsing fails,
        // these stay as-is and we log that the default was used.
        float    blend = kDefaultBlend;
        bool     ConsoleEnabled = kDefaultConsoleEnabled;
        bool     AutoRestoreWndw = kDefaultAutoRestoreWndw;
        uint16_t cameraEnableGamepadMask = kDefaultCameraEnableGamepadMask;
        int      directInputToggleButtonIndex = kDefaultDirectInputToggleButtonIndex;
        int      AltTabWait = kDefaultAltTabWait;
    };

    class Config
    {
    public:
        static const Settings& get();

    private:
        static Settings load();
        static std::wstring findConfigPath();
    };
}
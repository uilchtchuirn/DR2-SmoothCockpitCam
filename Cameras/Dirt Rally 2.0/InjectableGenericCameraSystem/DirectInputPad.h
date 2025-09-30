#pragma once
#include "stdafx.h"
#include <dinput.h>
#include <array>

namespace IGCS
{
    // Minimal DirectInput poller for one wheel/controller (buttons only)
    class DirectInputPad
    {
    public:
        DirectInputPad() = default;
        // IMPORTANT: Do NOT release DirectInput resources in the destructor.
        // Destructor may run during DLL_PROCESS_DETACH under loader lock.
        ~DirectInputPad() = default;

        // Initialize and acquire first attached game controller (e.g., wheel)
        bool initialize(HWND hwnd);
        bool isInitialized() const { return _initialized; }

        // Poll current state; call once per frame
        void update();

        // Explicit shutdown to release resources from a SAFE context (not DllMain).
        void shutdown();

        // Button helpers: 0..127 indices. True only on the frame the button transitions to down.
        bool isButtonJustPressed(int index) const
        {
            if (index < 0 || index >= 128) return false;
            return _buttonJustPressed[index];
        }

    private:
        static BOOL CALLBACK enumDevicesCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext);

        bool acquireDevice();
        void releaseDevice();
        void computeTransitions();

    private:
        bool _initialized = false;
        HWND _hwnd = nullptr;

        IDirectInput8* _di = nullptr;
        IDirectInputDevice8* _device = nullptr;

        DIJOYSTATE2 _state{};

        std::array<bool, 128> _prevBtn{};
        std::array<bool, 128> _curBtn{};
        std::array<bool, 128> _buttonJustPressed{};
    };
}
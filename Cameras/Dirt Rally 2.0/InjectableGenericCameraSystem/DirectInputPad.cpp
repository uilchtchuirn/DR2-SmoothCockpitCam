#include "stdafx.h"
#include "DirectInputPad.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#ifndef GET_DIDEVICE_TYPE
#define GET_DIDEVICE_TYPE(dwDevType) (LOBYTE(HIWORD(dwDevType)))
#endif

namespace IGCS
{
    // NOTE: No cleanup in destructor. Use shutdown() if you need explicit teardown.
    // See comments in header about loader lock and DllMain.

    BOOL CALLBACK DirectInputPad::enumDevicesCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext)
    {
        auto self = reinterpret_cast<DirectInputPad*>(pContext);
        if (!self || !self->_di) return DIENUM_STOP;

        const auto type = GET_DIDEVICE_TYPE(pdidInstance->dwDevType);
        const bool isDriving = (type == DI8DEVTYPE_DRIVING);

        if (SUCCEEDED(self->_di->CreateDevice(pdidInstance->guidInstance, &self->_device, nullptr)))
        {
            if (isDriving) return DIENUM_STOP; // prefer wheels
            // Keep device, continue enumeration to prefer a wheel; if none found, we’ll keep this one.
            return DIENUM_CONTINUE;
        }
        return DIENUM_CONTINUE;
    }

    bool DirectInputPad::initialize(HWND hwnd)
    {
        if (_initialized) return true;
        _hwnd = hwnd;

        if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8,
            reinterpret_cast<void**>(&_di), nullptr)))
        {
            return false;
        }

        if (FAILED(_di->EnumDevices(DI8DEVCLASS_GAMECTRL, enumDevicesCallback, this, DIEDFL_ATTACHEDONLY)))
        {
            return false;
        }

        if (!_device)
        {
            // No controller found
            return false;
        }

        if (FAILED(_device->SetDataFormat(&c_dfDIJoystick2)))
        {
            releaseDevice();
            return false;
        }

        if (FAILED(_device->SetCooperativeLevel(_hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
        {
            releaseDevice();
            return false;
        }

        if (!acquireDevice())
        {
            releaseDevice();
            return false;
        }

        ZeroMemory(&_state, sizeof(_state));
        _prevBtn.fill(false);
        _curBtn.fill(false);
        _buttonJustPressed.fill(false);

        _initialized = true;
        return true;
    }

    bool DirectInputPad::acquireDevice()
    {
        if (!_device) return false;
        const HRESULT hr = _device->Acquire();
        return SUCCEEDED(hr);
    }

    void DirectInputPad::releaseDevice()
    {
        if (_device)
        {
            _device->Unacquire();
            _device->Release();
            _device = nullptr;
        }
        if (_di)
        {
            _di->Release();
            _di = nullptr;
        }
        _initialized = false;
    }

    void DirectInputPad::computeTransitions()
    {
        for (int i = 0; i < 128; ++i)
        {
            _buttonJustPressed[i] = (!_prevBtn[i] && _curBtn[i]);
            _prevBtn[i] = _curBtn[i];
        }
    }

    void DirectInputPad::update()
    {
        if (!_initialized)
        {
            // Lazy init if needed
            if (!initialize(_hwnd)) return;
        }
        if (!_device) return;

        HRESULT hr = _device->Poll();
        if (FAILED(hr))
        {
            // During shutdown/alt-tab/etc., reacquire may fail; just bail.
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
            {
                if (!acquireDevice()) return;
            }
        }

        ZeroMemory(&_state, sizeof(_state));
        hr = _device->GetDeviceState(sizeof(DIJOYSTATE2), &_state);
        if (FAILED(hr))
        {
            // Don’t try hard during teardown; just stop updating.
            return;
        }

        for (int i = 0; i < 128; ++i)
        {
            _curBtn[i] = (_state.rgbButtons[i] & 0x80) != 0;
        }

        computeTransitions();
    }

    void DirectInputPad::shutdown()
    {
        // Explicit, safe-context teardown. Do NOT call from DllMain.
        releaseDevice();
        _prevBtn.fill(false);
        _curBtn.fill(false);
        _buttonJustPressed.fill(false);
        ZeroMemory(&_state, sizeof(_state));
    }
}
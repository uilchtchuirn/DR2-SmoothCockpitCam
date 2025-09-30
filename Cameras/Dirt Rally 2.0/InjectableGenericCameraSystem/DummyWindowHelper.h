#pragma once
#include <Windows.h>

class DummyWindowHelper {
public:
    DummyWindowHelper();
    ~DummyWindowHelper();
    HWND hwnd() const { return _hwnd; }
private:
    HWND _hwnd = nullptr;
    ATOM _wndClass = 0;
    static constexpr const wchar_t* kClassName = L"D3DHookDummyWindow";
};
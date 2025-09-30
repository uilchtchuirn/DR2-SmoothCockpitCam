#include "stdafx.h"
#include "DummyWindowHelper.h"

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DummyWindowHelper::DummyWindowHelper() {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kClassName;

    _wndClass = RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(
        0,
        kClassName,
        L"D3DHook Hidden Dummy Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    // The window is hidden by default (not shown).
}

DummyWindowHelper::~DummyWindowHelper() {
    if (_hwnd) {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    if (_wndClass) {
        UnregisterClass(kClassName, GetModuleHandle(nullptr));
        _wndClass = 0;
    }
}
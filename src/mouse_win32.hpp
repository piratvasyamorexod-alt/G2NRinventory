#pragma once

#include <Windows.h>
#include <commctrl.h>
#include <array>
#include <optional>
#include <utility>

#include "inventory_layout.hpp"

#pragma comment(lib, "comctl32.lib")

namespace InventoryMouse {

inline HWND WindowRoot(HWND window) {
    return window ? GetAncestor(window, GA_ROOT) : nullptr;
}

inline bool HasFocus(HWND game) {
    return game && WindowRoot(GetForegroundWindow()) == WindowRoot(game);
}

inline bool ClientBounds(HWND window, RECT& bounds) {
    RECT client{};
    if (!GetClientRect(window, &client)) return false;
    POINT topLeft{client.left, client.top}, bottomRight{client.right, client.bottom};
    if (!ClientToScreen(window, &topLeft) || !ClientToScreen(window, &bottomRight)) return false;
    bounds = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    return bounds.right > bounds.left && bounds.bottom > bounds.top;
}

class MouseEvents {
public:
    struct State {
        std::optional<POINT> leftClick, leftRelease, rightClick, wheelPosition;
        std::optional<InventoryLayout::PointerMotion> leftMotion;
        int wheel = 0;
        bool leftShift = false;
    };

private:
    static inline thread_local MouseEvents* activeWindow = nullptr;
    HWND window = nullptr;
    HHOOK mouseHook = nullptr, windowHook = nullptr;
    bool lostFocus = false;
    struct MouseEvent { POINT at; DWORD time; int delta; };
    std::array<std::optional<MouseEvent>, 4> lastLowLevel;
    std::optional<InventoryLayout::PointerMotion> leftMotion;
    State pending;
    int wheelDelta = 0;

    bool Watches(UINT message) const {
        return message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_RBUTTONDOWN
            || message == WM_MOUSEWHEEL || (message == WM_MOUSEMOVE && leftMotion.has_value());
    }

    void Record(UINT message, POINT at, int delta, DWORD time, bool lowLevel) {
        if (message == WM_MOUSEMOVE) {
            if (leftMotion) leftMotion->Extend(at.x, at.y);
            return;
        }
        const int index = message == WM_LBUTTONDOWN ? 0 : message == WM_LBUTTONUP ? 1 : message == WM_RBUTTONDOWN ? 2 : 3;
        auto& previous = lastLowLevel[index];
        if (lowLevel) {
            previous = MouseEvent{at, time, delta};
        } else if (previous && previous->at.x == at.x && previous->at.y == at.y && previous->delta == delta
                   && (time - previous->time <= 25 || previous->time - time <= 25)) {
            previous.reset();
            return;
        }
        switch (message) {
        case WM_LBUTTONDOWN:
            pending.leftClick = at;
            pending.leftRelease.reset();
            pending.leftMotion.reset();
            leftMotion.emplace(at.x, at.y);
            pending.leftShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            break;
        case WM_LBUTTONUP:
            if (leftMotion) {
                leftMotion->Extend(at.x, at.y);
                pending.leftMotion = std::exchange(leftMotion, std::nullopt);
                pending.leftRelease = at;
            }
            break;
        case WM_RBUTTONDOWN:
            pending.rightClick = at;
            break;
        case WM_MOUSEWHEEL:
            pending.wheelPosition = at;
            wheelDelta += delta;
            break;
        }
    }

    static LRESULT CALLBACK Observe(int code, WPARAM message, LPARAM data) {
        if (code == HC_ACTION && activeWindow && activeWindow->Watches(static_cast<UINT>(message))
            && HasFocus(activeWindow->window)) {
            auto& mouse = *activeWindow;
            const auto& input = *reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
            mouse.Record(static_cast<UINT>(message), input.pt,
                         static_cast<short>(HIWORD(input.mouseData)), input.time, true);
        }
        return CallNextHookEx(nullptr, code, message, data);
    }

    static LRESULT CALLBACK ObserveWindow(int code, WPARAM sent, LPARAM data) {
        if (code == HC_ACTION && activeWindow) {
            const auto& message = *reinterpret_cast<const CWPSTRUCT*>(data);
            auto& mouse = *activeWindow;
            if (message.hwnd == mouse.window) {
                if ((message.message == WM_ACTIVATEAPP && !message.wParam)
                    || (message.message == WM_ACTIVATE && LOWORD(message.wParam) == WA_INACTIVE)) {
                    mouse.lostFocus = true;
                    mouse.ClearPending();
                } else if (mouse.Watches(message.message) && HasFocus(mouse.window)) {
                    // Some wrappers deliver mouse input directly to the window.
                    POINT at{static_cast<short>(LOWORD(message.lParam)),
                             static_cast<short>(HIWORD(message.lParam))};
                    if (message.message != WM_MOUSEWHEEL && !ClientToScreen(message.hwnd, &at)) {
                        return CallNextHookEx(nullptr, code, sent, data);
                    }
                    const int delta = message.message == WM_MOUSEWHEEL ? GET_WHEEL_DELTA_WPARAM(message.wParam) : 0;
                    mouse.Record(message.message, at, delta, static_cast<DWORD>(GetMessageTime()), false);
                }
            }
        }
        return CallNextHookEx(nullptr, code, sent, data);
    }

    void ClearPending() {
        pending = {};
        leftMotion.reset();
        wheelDelta = 0;
        lastLowLevel = {};
    }

    void StopHooks() {
        if (activeWindow == this) activeWindow = nullptr;
        if (mouseHook) UnhookWindowsHookEx(std::exchange(mouseHook, nullptr));
        if (windowHook) UnhookWindowsHookEx(std::exchange(windowHook, nullptr));
    }

public:
    MouseEvents() = default;
    MouseEvents(const MouseEvents&) = delete;
    MouseEvents& operator=(const MouseEvents&) = delete;
    ~MouseEvents() { Stop(); }

    bool Start(HWND game) {
        if (!game || activeWindow) return false;
        window = game;
        // Windows dispatches low-level events on the installing thread. The
        // game already pumps this queue, so no worker has to exit with the DLL.
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&Observe), &module)) {
            mouseHook = SetWindowsHookExW(WH_MOUSE_LL, Observe, module, 0);
        }
        if (GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId()) {
            windowHook = SetWindowsHookExW(WH_CALLWNDPROC, ObserveWindow, nullptr, GetCurrentThreadId());
        }
        if (!mouseHook && !windowHook) return false;
        activeWindow = this;
        return true;
    }

    bool Restart(HWND game) {
        // Windows may have silently removed the low-level hook while the game
        // was inactive. Keep messages already captured after focus returned.
        StopHooks();
        return Start(game);
    }

    void Stop() {
        StopHooks();
        Discard();
        lostFocus = false;
    }

    void Discard() {
        ClearPending();
    }

    bool TakeFocusLoss() {
        return std::exchange(lostFocus, false);
    }

    State Take() {
        pending.wheel = wheelDelta / WHEEL_DELTA;
        wheelDelta -= pending.wheel * WHEEL_DELTA;
        if (leftMotion) pending.leftMotion = leftMotion;
        return std::exchange(pending, State{});
    }
};

class InventoryPointer {
    HWND cursorWindow = nullptr;
    HCURSOR cursor = nullptr;
    int showCalls = 0;
    std::optional<RECT> originalClip, ownedClip;

    static bool SameRect(const RECT& a, const RECT& b) {
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
    }

    static HCURSOR LoadInventoryCursor() {
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&LoadInventoryCursor), &module)) {
            if (auto resource = LoadCursorW(module, L"G2NR_INVENTORY_CURSOR")) return resource;
        }
        return LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    }

    static LRESULT CALLBACK KeepInventoryCursor(HWND window, UINT message, WPARAM wp, LPARAM lp,
                                              UINT_PTR id, DWORD_PTR context) {
        auto& pointer = *reinterpret_cast<InventoryPointer*>(context);
        if (message == WM_SETCURSOR && LOWORD(lp) == HTCLIENT && HasFocus(window)) {
            SetCursor(pointer.cursor);
            return TRUE;
        }
        if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(window, KeepInventoryCursor, id);
            pointer.cursorWindow = nullptr;
        }
        return DefSubclassProc(window, message, wp, lp);
    }

public:
    InventoryPointer() = default;
    InventoryPointer(const InventoryPointer&) = delete;
    InventoryPointer& operator=(const InventoryPointer&) = delete;
    ~InventoryPointer() { Leave(); }

    void Enter(HWND window, const RECT& client, bool center) {
        RECT previous{};
        if (GetClipCursor(&previous) && !SameRect(client, previous) && ClipCursor(&client)) {
            originalClip = previous;
            ownedClip = client;
        }
        cursor = LoadInventoryCursor();
        if (cursor && GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId()
            && SetWindowSubclass(window, KeepInventoryCursor, 0, reinterpret_cast<DWORD_PTR>(this))) {
            cursorWindow = window;
        }
        do {
            ++showCalls;
        } while (ShowCursor(TRUE) < 0);
        if (cursor) SetCursor(cursor);
        if (center) SetCursorPos(client.left + (client.right - client.left) / 2,
                                client.top + (client.bottom - client.top) / 2);
    }

    void Leave() {
        if (cursorWindow) RemoveWindowSubclass(std::exchange(cursorWindow, nullptr), KeepInventoryCursor, 0);
        RECT current{};
        if (originalClip && ownedClip && GetClipCursor(&current) && SameRect(*ownedClip, current)) {
            ClipCursor(&*originalClip);
        }
        originalClip.reset();
        ownedClip.reset();
        while (showCalls > 0) {
            ShowCursor(FALSE);
            --showCalls;
        }
    }
};

} // namespace InventoryMouse

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <wrl.h>
#include <WebView2.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shlwapi.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {
constexpr wchar_t kClassName[] = L"PrivaLayerMainWindow";
constexpr wchar_t kAppName[] = L"PrivaLayer";
constexpr UINT WM_TRAY_CALLBACK = WM_APP + 41;
constexpr UINT ID_TRAY_OPEN = 9101;
constexpr UINT ID_TRAY_SETTINGS = 9102;
constexpr UINT ID_TRAY_EXIT = 9103;
constexpr UINT kTrayIconId = 1;
constexpr int HOTKEY_TOGGLE = 1;
constexpr int HOTKEY_INTERACTIVE = 2;
constexpr wchar_t kDefaultToggleHotkey[] = L"CTRL+ALT+SPACE";
constexpr wchar_t kDefaultInteractiveHotkey[] = L"CTRL+ALT+I";

HWND g_hwnd = nullptr;
HICON g_appIcon = nullptr;
NOTIFYICONDATAW g_trayData{};
bool g_trayIconAdded = false;
UINT g_taskbarCreatedMessage = 0;
bool g_isTopmost = true;
bool g_isPassthrough = false;
bool g_sidebarCollapsed = false;
int g_opacityValue = 96;
std::wstring g_toggleHotkey = kDefaultToggleHotkey;
std::wstring g_interactiveHotkey = kDefaultInteractiveHotkey;
std::wstring g_lastCapturePath;
ULONG_PTR g_gdiplusToken = 0;

ComPtr<ICoreWebView2Environment> g_webEnvironment;
ComPtr<ICoreWebView2Controller> g_uiController;
ComPtr<ICoreWebView2> g_uiView;
ComPtr<ICoreWebView2Controller> g_browserController;
ComPtr<ICoreWebView2> g_browserView;
bool g_browserVisible = false;

std::wstring GetKnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw)) || !raw) return L".";
    std::wstring value(raw);
    CoTaskMemFree(raw);
    return value;
}

fs::path AppDataPath() {
    fs::path path = fs::path(GetKnownFolder(FOLDERID_LocalAppData)) / L"PrivaLayer";
    std::error_code ec;
    fs::create_directories(path, ec);
    return path;
}

fs::path SettingsPath() { return AppDataPath() / L"settings.ini"; }
fs::path BrowserDataPath() {
    fs::path path = AppDataPath() / L"WebView2Data";
    std::error_code ec;
    fs::create_directories(path, ec);
    return path;
}
fs::path CaptureFolder() {
    fs::path path = fs::path(GetKnownFolder(FOLDERID_Pictures)) / L"PrivaLayer";
    std::error_code ec;
    fs::create_directories(path, ec);
    return path;
}
fs::path LegacyNotesPath() { return AppDataPath() / L"notes.txt"; }

std::wstring ReadLegacyNotes() {
    HANDLE file = CreateFileW(LegacyNotesPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 2 || size.QuadPart > 8 * 1024 * 1024) {
        CloseHandle(file);
        return {};
    }
    std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    bytes.resize(read);
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        size_t chars = (bytes.size() - 2) / sizeof(wchar_t);
        std::wstring result(chars, L'\0');
        if (chars) memcpy(result.data(), bytes.data() + 2, chars * sizeof(wchar_t));
        return result;
    }
    return {};
}

fs::path ExecutableDirectory() {
    std::vector<wchar_t> buffer(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) return fs::current_path();
    return fs::path(std::wstring(buffer.data(), length)).parent_path();
}

void LoadSettings() {
    const std::wstring path = SettingsPath().wstring();
    g_opacityValue = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Appearance", L"Opacity", 96, path.c_str())), 45, 100);
    g_isTopmost = GetPrivateProfileIntW(L"Window", L"Topmost", 1, path.c_str()) != 0;
    g_isPassthrough = GetPrivateProfileIntW(L"Window", L"PassThrough", 0, path.c_str()) != 0;
    wchar_t value[128]{};
    GetPrivateProfileStringW(L"Hotkeys", L"Toggle", kDefaultToggleHotkey, value, 128, path.c_str());
    g_toggleHotkey = value;
    GetPrivateProfileStringW(L"Hotkeys", L"Interactive", kDefaultInteractiveHotkey, value, 128, path.c_str());
    g_interactiveHotkey = value;
}

void SaveInteger(const wchar_t* section, const wchar_t* key, int value) {
    wchar_t buffer[24]{};
    swprintf_s(buffer, L"%d", value);
    WritePrivateProfileStringW(section, key, buffer, SettingsPath().c_str());
}

void SetLayerOpacity(int value) {
    g_opacityValue = std::clamp(value, 45, 100);
    if (g_hwnd) SetLayeredWindowAttributes(g_hwnd, 0, static_cast<BYTE>((g_opacityValue * 255) / 100), LWA_ALPHA);
    SaveInteger(L"Appearance", L"Opacity", g_opacityValue);
}

void SetTopmost(bool enabled) {
    g_isTopmost = enabled;
    if (g_hwnd) SetWindowPos(g_hwnd, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SaveInteger(L"Window", L"Topmost", enabled ? 1 : 0);
}

void SetPassThrough(bool enabled) {
    g_isPassthrough = enabled;
    if (g_hwnd) {
        LONG_PTR style = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
        if (enabled) style |= WS_EX_TRANSPARENT;
        else style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, style);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    SaveInteger(L"Window", L"PassThrough", enabled ? 1 : 0);
}

std::vector<std::wstring> Split(const std::wstring& value, wchar_t separator) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= value.size()) {
        size_t end = value.find(separator, start);
        if (end == std::wstring::npos) end = value.size();
        parts.emplace_back(value.substr(start, end - start));
        if (end == value.size()) break;
        start = end + 1;
    }
    return parts;
}

bool ParseHotkey(const std::wstring& specification, UINT& modifiers, UINT& key) {
    modifiers = MOD_NOREPEAT;
    key = 0;
    bool hasModifier = false;
    bool hasKey = false;
    for (std::wstring token : Split(specification, L'+')) {
        token.erase(std::remove_if(token.begin(), token.end(), [](wchar_t ch) { return std::iswspace(ch) != 0; }), token.end());
        std::transform(token.begin(), token.end(), token.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towupper(ch)); });
        if (token == L"CTRL" || token == L"CONTROL") { modifiers |= MOD_CONTROL; hasModifier = true; }
        else if (token == L"ALT") { modifiers |= MOD_ALT; hasModifier = true; }
        else if (token == L"SHIFT") { modifiers |= MOD_SHIFT; hasModifier = true; }
        else if (token == L"WIN" || token == L"WINDOWS") { modifiers |= MOD_WIN; hasModifier = true; }
        else if (token == L"SPACE") { key = VK_SPACE; hasKey = true; }
        else if (token == L"TAB") { key = VK_TAB; hasKey = true; }
        else if (token == L"ENTER") { key = VK_RETURN; hasKey = true; }
        else if (token == L"ESC" || token == L"ESCAPE") { key = VK_ESCAPE; hasKey = true; }
        else if (token.size() == 1 && token[0] >= L'A' && token[0] <= L'Z') { key = static_cast<UINT>(token[0]); hasKey = true; }
        else if (token.size() == 1 && token[0] >= L'0' && token[0] <= L'9') { key = static_cast<UINT>(token[0]); hasKey = true; }
        else if (token.size() >= 2 && token[0] == L'F') {
            wchar_t* end = nullptr;
            long number = wcstol(token.c_str() + 1, &end, 10);
            if (end && *end == L'\0' && number >= 1 && number <= 12) { key = VK_F1 + static_cast<UINT>(number - 1); hasKey = true; }
        }
        if (hasKey && key) continue;
    }
    return hasModifier && hasKey && key != 0;
}

bool ApplyHotkeys(const std::wstring& toggle, const std::wstring& interactive, bool notify) {
    UINT toggleMods = 0, toggleKey = 0, interactiveMods = 0, interactiveKey = 0;
    bool valid = ParseHotkey(toggle, toggleMods, toggleKey) && ParseHotkey(interactive, interactiveMods, interactiveKey);
    UnregisterHotKey(g_hwnd, HOTKEY_TOGGLE);
    UnregisterHotKey(g_hwnd, HOTKEY_INTERACTIVE);
    bool registered = valid &&
        RegisterHotKey(g_hwnd, HOTKEY_TOGGLE, toggleMods, toggleKey) &&
        RegisterHotKey(g_hwnd, HOTKEY_INTERACTIVE, interactiveMods, interactiveKey);
    if (!registered) {
        UnregisterHotKey(g_hwnd, HOTKEY_TOGGLE);
        UnregisterHotKey(g_hwnd, HOTKEY_INTERACTIVE);
        UINT dm = 0, dk = 0, im = 0, ik = 0;
        ParseHotkey(kDefaultToggleHotkey, dm, dk);
        ParseHotkey(kDefaultInteractiveHotkey, im, ik);
        RegisterHotKey(g_hwnd, HOTKEY_TOGGLE, dm, dk);
        RegisterHotKey(g_hwnd, HOTKEY_INTERACTIVE, im, ik);
        g_toggleHotkey = kDefaultToggleHotkey;
        g_interactiveHotkey = kDefaultInteractiveHotkey;
        if (notify && g_uiView) g_uiView->PostWebMessageAsString(L"hotkeys-result|error|CTRL+ALT+SPACE|CTRL+ALT+I");
        return false;
    }
    g_toggleHotkey = toggle;
    g_interactiveHotkey = interactive;
    WritePrivateProfileStringW(L"Hotkeys", L"Toggle", toggle.c_str(), SettingsPath().c_str());
    WritePrivateProfileStringW(L"Hotkeys", L"Interactive", interactive.c_str(), SettingsPath().c_str());
    if (notify && g_uiView) g_uiView->PostWebMessageAsString(L"hotkeys-result|ok");
    return true;
}

void PostToUi(const std::wstring& message) {
    if (g_uiView) g_uiView->PostWebMessageAsString(message.c_str());
}

void AddTrayIcon(HWND hwnd) {
    if (!hwnd || g_trayIconAdded) return;
    g_trayData = {};
    g_trayData.cbSize = sizeof(NOTIFYICONDATAW);
    g_trayData.hWnd = hwnd;
    g_trayData.uID = kTrayIconId;
    g_trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayData.uCallbackMessage = WM_TRAY_CALLBACK;
    g_trayData.hIcon = g_appIcon ? g_appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_trayData.szTip, L"PrivaLayer — Personal Workspace");
    g_trayIconAdded = Shell_NotifyIconW(NIM_ADD, &g_trayData) == TRUE;
}

void RemoveTrayIcon() {
    if (g_trayIconAdded) Shell_NotifyIconW(NIM_DELETE, &g_trayData);
    g_trayIconAdded = false;
}

void ShowMainWindow() {
    if (!g_hwnd) return;
    ShowWindow(g_hwnd, SW_RESTORE);
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
}

void ToggleMainWindow() {
    if (!g_hwnd) return;
    if (IsWindowVisible(g_hwnd)) ShowWindow(g_hwnd, SW_HIDE);
    else ShowMainWindow();
}

void ShowTrayContextMenu(HWND hwnd) {
    POINT point{};
    GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, IsWindowVisible(hwnd) ? L"Hide PrivaLayer" : L"Open PrivaLayer");
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

void LayoutBrowser() {
    if (!g_browserController || !g_hwnd) return;
    RECT client{};
    GetClientRect(g_hwnd, &client);
    RECT bounds{ g_sidebarCollapsed ? 76L : 248L, 160, std::max(g_sidebarCollapsed ? 96L : 270L, client.right - 22), std::max(180L, client.bottom - 22) };
    g_browserController->put_Bounds(bounds);
}

void SetBrowserVisible(bool visible) {
    g_browserVisible = visible;
    if (g_browserController) {
        LayoutBrowser();
        g_browserController->put_IsVisible(visible ? TRUE : FALSE);
    }
}

void NavigateBrowser(const std::wstring& url) {
    if (!g_browserView || url.empty()) return;
    std::wstring target = url;
    if (target.find(L"://") == std::wstring::npos) target = L"https://" + target;
    g_browserView->Navigate(target.c_str());
}

std::wstring MakeCaptureName() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t name[96]{};
    swprintf_s(name, L"Capture_%04u%02u%02u_%02u%02u%02u.png",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    return name;
}

int GetPngEncoderClsid(CLSID* clsid) {
    UINT count = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || !size) return -1;
    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, size, codecs) != Gdiplus::Ok) return -1;
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            *clsid = codecs[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool SaveClipboardBitmap(const fs::path& path) {
    if (!OpenClipboard(g_hwnd)) return false;
    HBITMAP bitmapHandle = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    bool ownsBitmap = false;
    HGLOBAL dibMemory = nullptr;
    void* dibData = nullptr;
    if (!bitmapHandle) {
        UINT format = IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5 : CF_DIB;
        dibMemory = static_cast<HGLOBAL>(GetClipboardData(format));
        if (dibMemory) {
            dibData = GlobalLock(dibMemory);
            if (dibData) {
                auto* header = static_cast<BITMAPINFOHEADER*>(dibData);
                BYTE* pixels = static_cast<BYTE*>(dibData) + header->biSize;
                if (header->biCompression == BI_BITFIELDS && header->biSize == sizeof(BITMAPINFOHEADER)) pixels += 3 * sizeof(DWORD);
                else if (header->biBitCount <= 8) {
                    DWORD colors = header->biClrUsed ? header->biClrUsed : (1u << header->biBitCount);
                    pixels += colors * sizeof(RGBQUAD);
                }
                HDC screen = GetDC(nullptr);
                if (screen) {
                    bitmapHandle = CreateDIBitmap(screen, header, CBM_INIT, pixels,
                        reinterpret_cast<BITMAPINFO*>(dibData), DIB_RGB_COLORS);
                    ReleaseDC(nullptr, screen);
                    ownsBitmap = bitmapHandle != nullptr;
                }
            }
        }
    }
    bool saved = false;
    if (bitmapHandle) {
        CLSID png{};
        Gdiplus::Bitmap bitmap(bitmapHandle, nullptr);
        saved = GetPngEncoderClsid(&png) >= 0 && bitmap.GetLastStatus() == Gdiplus::Ok &&
            bitmap.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
    }
    if (ownsBitmap && bitmapHandle) DeleteObject(bitmapHandle);
    if (dibData) GlobalUnlock(dibMemory);
    CloseClipboard();
    return saved;
}

void StartWindowsSnip() {
    // Uses the official Windows snipping UI; the user selects the region manually.
    ShowWindow(g_hwnd, SW_HIDE);
    INPUT input[6]{};
    input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = VK_LWIN;
    input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = VK_SHIFT;
    input[2].type = INPUT_KEYBOARD; input[2].ki.wVk = 'S';
    input[3].type = INPUT_KEYBOARD; input[3].ki.wVk = 'S'; input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    input[4].type = INPUT_KEYBOARD; input[4].ki.wVk = VK_SHIFT; input[4].ki.dwFlags = KEYEVENTF_KEYUP;
    input[5].type = INPUT_KEYBOARD; input[5].ki.wVk = VK_LWIN; input[5].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(6, input, sizeof(INPUT));
    PostToUi(L"capture-started");
}

void ImportCapture() {
    fs::path path = CaptureFolder() / MakeCaptureName();
    bool available = IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5);
    if (!available || !SaveClipboardBitmap(path)) {
        PostToUi(L"capture-error|No image found in the clipboard. Finish the Windows snip, reopen PrivaLayer and import again.");
        return;
    }
    g_lastCapturePath = path.wstring();
    PostToUi(std::wstring(L"capture-saved|") + path.filename().wstring());
}

void OpenCaptureFolder() {
    ShellExecuteW(nullptr, L"open", CaptureFolder().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CopyToClipboard(const std::wstring& value) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    SIZE_T bytes = (value.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* target = GlobalLock(memory);
        if (target) {
            memcpy(target, value.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        } else GlobalFree(memory);
    }
    CloseClipboard();
}

void HandleWebMessage(const std::wstring& message) {
    std::vector<std::wstring> parts = Split(message, L'|');
    if (parts.empty()) return;
    const std::wstring& action = parts[0];
    if (action == L"opacity" && parts.size() > 1) SetLayerOpacity(_wtoi(parts[1].c_str()));
    else if (action == L"topmost" && parts.size() > 1) SetTopmost(parts[1] == L"1");
    else if (action == L"passthrough" && parts.size() > 1) SetPassThrough(parts[1] == L"1");
    else if (action == L"hotkeys" && parts.size() > 2) ApplyHotkeys(parts[1], parts[2], true);
    else if (action == L"capture-start") StartWindowsSnip();
    else if (action == L"capture-import") ImportCapture();
    else if (action == L"capture-folder") OpenCaptureFolder();
    else if (action == L"sidebar-collapsed" && parts.size() > 1) { g_sidebarCollapsed = parts[1] == L"1"; LayoutBrowser(); }
    else if (action == L"browser-show") SetBrowserVisible(true);
    else if (action == L"browser-hide") SetBrowserVisible(false);
    else if (action == L"browser-url" && parts.size() > 1) NavigateBrowser(parts[1]);
    else if (action == L"browser-back" && g_browserView) g_browserView->GoBack();
    else if (action == L"browser-forward" && g_browserView) g_browserView->GoForward();
    else if (action == L"browser-aistudio") {
        SetBrowserVisible(true);
        NavigateBrowser(L"https://aistudio.google.com/");
        PostToUi(L"navigate-page|browser");
    }
    else if (action == L"window-hide") ShowWindow(g_hwnd, SW_HIDE);
    else if (action == L"window-minimize") ShowWindow(g_hwnd, SW_MINIMIZE);
    else if (action == L"window-exit") DestroyWindow(g_hwnd);
    else if (action == L"copy-text" && parts.size() > 1) CopyToClipboard(parts[1]);
}

void EnsureWebViews() {
    if (!g_hwnd || g_webEnvironment) return;
    std::wstring userData = BrowserDataPath().wstring();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment) {
                    MessageBoxW(g_hwnd, L"PrivaLayer could not initialize WebView2. Install the Microsoft Edge WebView2 Runtime and restart the app.", kAppName, MB_ICONWARNING);
                    return S_OK;
                }
                g_webEnvironment = environment;
                g_webEnvironment->CreateCoreWebView2Controller(g_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || !controller) {
                                MessageBoxW(g_hwnd, L"PrivaLayer could not create its workspace surface.", kAppName, MB_ICONWARNING);
                                return S_OK;
                            }
                            g_uiController = controller;
                            g_uiController->get_CoreWebView2(&g_uiView);
                            if (g_uiView) {
                                g_uiView->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR raw = nullptr;
                                            if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                                                HandleWebMessage(raw);
                                                CoTaskMemFree(raw);
                                            }
                                            return S_OK;
                                        }).Get(), nullptr);
                                fs::path page = ExecutableDirectory() / L"assets" / L"ui" / L"index.html";
                                std::vector<wchar_t> url(32768, L'\0');
                                DWORD length = static_cast<DWORD>(url.size());
                                if (SUCCEEDED(UrlCreateFromPathW(page.c_str(), url.data(), &length, 0))) {
                                    g_uiView->add_NavigationCompleted(
                                        Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                            [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                                wchar_t state[512]{};
                                                swprintf_s(state, L"native-settings|%d|%d|%d|%s|%s",
                                                    g_opacityValue, g_isTopmost ? 1 : 0, g_isPassthrough ? 1 : 0,
                                                    g_toggleHotkey.c_str(), g_interactiveHotkey.c_str());
                                                PostToUi(state);
                                                std::wstring oldNote = ReadLegacyNotes();
                                                if (!oldNote.empty()) PostToUi(std::wstring(L"legacy-note|") + oldNote);
                                                return S_OK;
                                            }).Get(), nullptr);
                                    g_uiView->Navigate(url.data());
                                } else {
                                    MessageBoxW(g_hwnd, L"PrivaLayer UI files are missing. Reinstall the complete application folder.", kAppName, MB_ICONERROR);
                                }
                            }
                            g_webEnvironment->CreateCoreWebView2Controller(g_hwnd,
                                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                    [](HRESULT browserResult, ICoreWebView2Controller* browserController) -> HRESULT {
                                        if (FAILED(browserResult) || !browserController) return S_OK;
                                        g_browserController = browserController;
                                        g_browserController->get_CoreWebView2(&g_browserView);
                                        LayoutBrowser();
                                        g_browserController->put_IsVisible(g_browserVisible ? TRUE : FALSE);
                                        if (g_browserView) g_browserView->Navigate(L"https://aistudio.google.com/");
                                        return S_OK;
                                    }).Get());
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
    if (FAILED(hr)) MessageBoxW(g_hwnd, L"Unable to start the embedded browser engine.", kAppName, MB_ICONWARNING);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreatedMessage && message == g_taskbarCreatedMessage) {
        g_trayIconAdded = false;
        AddTrayIcon(hwnd);
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        EnsureWebViews();
        ApplyHotkeys(g_toggleHotkey, g_interactiveHotkey, false);
        SetLayerOpacity(g_opacityValue);
        SetTopmost(g_isTopmost);
        SetPassThrough(g_isPassthrough);
        return 0;
    }
    case WM_SIZE:
        if (g_uiController) {
            RECT client{};
            GetClientRect(hwnd, &client);
            g_uiController->put_Bounds(client);
        }
        LayoutBrowser();
        return 0;
    case WM_DPICHANGED: {
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 1050;
        info->ptMinTrackSize.y = 680;
        return 0;
    }
    case WM_TRAY_CALLBACK:
        if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) ToggleMainWindow();
        else if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) ShowTrayContextMenu(hwnd);
        return 0;
    case WM_HOTKEY:
        if (wParam == HOTKEY_TOGGLE) ToggleMainWindow();
        else if (wParam == HOTKEY_INTERACTIVE) {
            SetPassThrough(!g_isPassthrough);
            PostToUi(g_isPassthrough ? L"native-passthrough|1" : L"native-passthrough|0");
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN: ToggleMainWindow(); break;
        case ID_TRAY_SETTINGS:
            ShowMainWindow();
            PostToUi(L"navigate-page|settings");
            break;
        case ID_TRAY_EXIT: DestroyWindow(hwnd); break;
        }
        return 0;
    case WM_CLOSE:
        if (g_trayIconAdded) ShowWindow(hwnd, SW_HIDE);
        else ShowWindow(hwnd, SW_MINIMIZE);
        return 0;
    case WM_DESTROY:
        RemoveTrayIcon();
        UnregisterHotKey(hwnd, HOTKEY_TOGGLE);
        UnregisterHotKey(hwnd, HOTKEY_INTERACTIVE);
        if (g_browserController) g_browserController->Close();
        if (g_uiController) g_uiController->Close();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    LoadSettings();
    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) return 1;

    Gdiplus::GdiplusStartupInput gdiplusInput;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) != Gdiplus::Ok) return 2;

    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    g_appIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIcon = g_appIcon;
    wc.hIconSm = g_appIcon;
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    if (!RegisterClassExW(&wc)) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        CoUninitialize();
        return 3;
    }

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED, kClassName,
        L"PrivaLayer — Personal Workspace", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1460, 920, nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        CoUninitialize();
        return 4;
    }
    g_hwnd = hwnd;
    AddTrayIcon(hwnd);
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_appIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_appIcon));
    SetLayerOpacity(g_opacityValue);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    g_browserView.Reset();
    g_browserController.Reset();
    g_uiView.Reset();
    g_uiController.Reset();
    g_webEnvironment.Reset();
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
    CoUninitialize();
    return static_cast<int>(message.wParam);
}

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <wrl.h>
#include <WebView2.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>

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
constexpr int kSidebarWidth = 206;
constexpr int kHeaderHeight = 78;
constexpr int kContentLeft = 232;
constexpr int kContentTop = 112;
constexpr int kControlWidth = 790;

enum Page { PAGE_HOME = 0, PAGE_NOTES, PAGE_BROWSER, PAGE_AI, PAGE_WORKSPACE, PAGE_SETTINGS };
enum ControlId {
    ID_NAV_HOME = 100, ID_NAV_NOTES, ID_NAV_BROWSER, ID_NAV_AI, ID_NAV_WORKSPACE, ID_NAV_SETTINGS,
    ID_NOTES_EDIT = 200, ID_NOTES_SAVE,
    ID_URL_EDIT = 300, ID_BROWSER_GO, ID_BROWSER_BACK, ID_BROWSER_FORWARD,
    ID_AI_PROMPT = 400, ID_AI_COPY_PROMPT, ID_AI_START_SNIP, ID_AI_IMPORT_CAPTURE, ID_AI_OPEN_STUDIO,
    ID_SETTINGS_OPACITY = 500, ID_SETTINGS_TOPMOST, ID_SETTINGS_PASSTHROUGH, ID_SETTINGS_THEME,
    ID_WORK_OPEN_NOTES = 600, ID_WORK_OPEN_BROWSER, ID_WORK_OPEN_AI,
    ID_CAPTURE_OPEN_FOLDER = 700
};
constexpr int HOTKEY_TOGGLE = 1;
constexpr int HOTKEY_INTERACTIVE = 2;
constexpr int HOTKEY_SNIP = 3;

struct Palette {
    COLORREF bg;
    COLORREF sidebar;
    COLORREF panel;
    COLORREF panel2;
    COLORREF text;
    COLORREF muted;
    COLORREF accent;
    COLORREF accent2;
    COLORREF border;
};
const Palette kDark{ RGB(12,16,28), RGB(15,21,36), RGB(21,29,48), RGB(29,39,62), RGB(239,244,255), RGB(151,165,190), RGB(44,155,240), RGB(67,211,220), RGB(44,60,88) };
const Palette kPurple{ RGB(17,14,31), RGB(22,18,40), RGB(31,25,54), RGB(43,34,72), RGB(245,241,255), RGB(174,163,202), RGB(139,105,255), RGB(53,190,219), RGB(66,53,96) };
const Palette kLight{ RGB(242,246,252), RGB(230,237,248), RGB(255,255,255), RGB(236,242,250), RGB(25,34,51), RGB(93,108,132), RGB(20,112,204), RGB(20,155,163), RGB(205,216,232) };

HWND g_hwnd = nullptr;
HWND g_notesEdit = nullptr;
HWND g_notesSave = nullptr;
HWND g_urlEdit = nullptr;
HWND g_browserGo = nullptr;
HWND g_browserBack = nullptr;
HWND g_browserForward = nullptr;
HWND g_aiPrompt = nullptr;
HWND g_aiCopy = nullptr;
HWND g_aiSnip = nullptr;
HWND g_aiImport = nullptr;
HWND g_aiOpenStudio = nullptr;
HWND g_opacity = nullptr;
HWND g_topmost = nullptr;
HWND g_passthrough = nullptr;
HWND g_theme = nullptr;
HWND g_workNotes = nullptr;
HWND g_workBrowser = nullptr;
HWND g_workAI = nullptr;
HWND g_openCaptureFolder = nullptr;
std::vector<HWND> g_navButtons;
Page g_page = PAGE_HOME;
int g_opacityValue = 94;
int g_themeIndex = 0;
bool g_isPassthrough = false;
bool g_isTopmost = true;
ULONG_PTR g_gdiplusToken = 0;
HICON g_appIcon = nullptr;
Gdiplus::Image* g_logo = nullptr;
ComPtr<ICoreWebView2Environment> g_webEnvironment;
ComPtr<ICoreWebView2Controller> g_webController;
ComPtr<ICoreWebView2> g_webView;
bool g_webInitStarted = false;
std::wstring g_lastCapturePath;

const Palette& CurrentPalette() {
    if (g_themeIndex == 1) return kPurple;
    if (g_themeIndex == 2) return kLight;
    return kDark;
}

std::wstring GetKnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw)) || !raw) return L".";
    std::wstring result(raw);
    CoTaskMemFree(raw);
    return result;
}

fs::path ExecutableDirectory() {
    std::vector<wchar_t> buffer(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return fs::current_path();
    return fs::path(std::wstring(buffer.data(), length)).parent_path();
}

fs::path AppDataPath() {
    fs::path p = fs::path(GetKnownFolder(FOLDERID_LocalAppData)) / L"PrivaLayer";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

fs::path NotesPath() { return AppDataPath() / L"notes.txt"; }
fs::path SettingsPath() { return AppDataPath() / L"settings.ini"; }

void LoadSettings() {
    const std::wstring path = SettingsPath().wstring();
    g_opacityValue = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Appearance", L"Opacity", 94, path.c_str())), 40, 100);
    g_themeIndex = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Appearance", L"Theme", 0, path.c_str())), 0, 2);
    g_isTopmost = GetPrivateProfileIntW(L"Window", L"Topmost", 1, path.c_str()) != 0;
    g_isPassthrough = GetPrivateProfileIntW(L"Window", L"PassThrough", 0, path.c_str()) != 0;
}

void SaveSettings() {
    const std::wstring path = SettingsPath().wstring();
    wchar_t value[16]{};
    swprintf_s(value, L"%d", g_opacityValue);
    WritePrivateProfileStringW(L"Appearance", L"Opacity", value, path.c_str());
    swprintf_s(value, L"%d", g_themeIndex);
    WritePrivateProfileStringW(L"Appearance", L"Theme", value, path.c_str());
    WritePrivateProfileStringW(L"Window", L"Topmost", g_isTopmost ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Window", L"PassThrough", g_isPassthrough ? L"1" : L"0", path.c_str());
}
fs::path BrowserDataPath() {
    fs::path p = AppDataPath() / L"WebView2Data";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}
fs::path CaptureFolder() {
    fs::path p = fs::path(GetKnownFolder(FOLDERID_Pictures)) / L"PrivaLayer";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

HFONT MakeFont(int points, bool bold = false) {
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable");
}

HFONT g_font = nullptr;
HFONT g_fontBold = nullptr;

void SetControlFont(HWND control, HFONT font = nullptr) {
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : g_font), TRUE);
}

HWND MakeChild(DWORD exStyle, const wchar_t* cls, const wchar_t* text, DWORD style,
               int x, int y, int w, int h, int id, HWND parent = nullptr) {
    HWND control = CreateWindowExW(exStyle, cls, text, style, x, y, w, h,
        parent ? parent : g_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (control) SetControlFont(control);
    return control;
}

void SaveNotes() {
    if (!g_notesEdit) return;
    int len = GetWindowTextLengthW(g_notesEdit);
    std::wstring text(static_cast<size_t>(std::max(0, len)) + 1, L'\0');
    if (len > 0) GetWindowTextW(g_notesEdit, text.data(), len + 1);
    text.resize(static_cast<size_t>(std::max(0, len)));
    HANDLE file = CreateFileW(NotesPath().c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const WORD bom = 0xFEFF;
    DWORD written = 0;
    WriteFile(file, &bom, sizeof(bom), &written, nullptr);
    if (!text.empty()) WriteFile(file, text.data(), static_cast<DWORD>(text.size() * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(file);
}

void LoadNotes() {
    if (!g_notesEdit) return;
    HANDLE file = CreateFileW(NotesPath().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 8 * 1024 * 1024) { CloseHandle(file); return; }
    std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!bytes.empty()) ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    bytes.resize(read);
    size_t offset = (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) ? 2 : 0;
    size_t chars = (bytes.size() - offset) / sizeof(wchar_t);
    std::wstring text(chars, L'\0');
    if (chars) memcpy(text.data(), bytes.data() + offset, chars * sizeof(wchar_t));
    SetWindowTextW(g_notesEdit, text.c_str());
}

void SetLayerOpacity() {
    if (!g_hwnd) return;
    BYTE alpha = static_cast<BYTE>((std::clamp(g_opacityValue, 40, 100) * 255) / 100);
    SetLayeredWindowAttributes(g_hwnd, 0, alpha, LWA_ALPHA);
    SaveSettings();
}

void SetPassThrough(bool enabled) {
    g_isPassthrough = enabled;
    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    if (enabled) ex |= WS_EX_TRANSPARENT;
    else ex &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
    if (g_passthrough) SendMessageW(g_passthrough, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SaveSettings();
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

void SetTopmost(bool enabled) {
    g_isTopmost = enabled;
    SetWindowPos(g_hwnd, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (g_topmost) SendMessageW(g_topmost, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SaveSettings();
}

void ShowPageControl(HWND h, bool show) {
    if (h) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
}

void LayoutBrowser() {
    if (!g_webController || !g_hwnd) return;
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    RECT bounds{ kContentLeft, 190, rc.right - 28, rc.bottom - 28 };
    g_webController->put_Bounds(bounds);
}

void EnsureWebView() {
    if (g_webInitStarted || !g_hwnd) return;
    g_webInitStarted = true;
    std::wstring userData = BrowserDataPath().wstring();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment) {
                    MessageBoxW(g_hwnd, L"WebView2 could not be initialized. Install the Microsoft Edge WebView2 Runtime and restart PrivaLayer.", kAppName, MB_ICONWARNING);
                    return S_OK;
                }
                g_webEnvironment = environment;
                return g_webEnvironment->CreateCoreWebView2Controller(g_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || !controller) {
                                MessageBoxW(g_hwnd, L"The embedded browser could not be created.", kAppName, MB_ICONWARNING);
                                return S_OK;
                            }
                            g_webController = controller;
                            g_webController->get_CoreWebView2(&g_webView);
                            LayoutBrowser();
                            g_webController->put_IsVisible(g_page == PAGE_BROWSER ? TRUE : FALSE);
                            if (g_webView) g_webView->Navigate(L"https://aistudio.google.com/");
                            return S_OK;
                        }).Get());
            }).Get());
    if (FAILED(hr)) {
        MessageBoxW(g_hwnd, L"Unable to start WebView2. Check the WebView2 Runtime installation.", kAppName, MB_ICONWARNING);
    }
}

void NavigateBrowser() {
    if (!g_webView || !g_urlEdit) return;
    int n = GetWindowTextLengthW(g_urlEdit);
    std::wstring url(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(g_urlEdit, url.data(), n + 1);
    url.resize(static_cast<size_t>(n));
    if (url.empty()) return;
    if (url.find(L"://") == std::wstring::npos) url = L"https://" + url;
    g_webView->Navigate(url.c_str());
}

void SetPage(Page page) {
    SaveNotes();
    g_page = page;
    const bool notes = page == PAGE_NOTES;
    const bool browser = page == PAGE_BROWSER;
    const bool ai = page == PAGE_AI;
    const bool settings = page == PAGE_SETTINGS;
    const bool workspace = page == PAGE_WORKSPACE;

    ShowPageControl(g_notesEdit, notes);
    ShowPageControl(g_notesSave, notes);
    ShowPageControl(g_urlEdit, browser);
    ShowPageControl(g_browserGo, browser);
    ShowPageControl(g_browserBack, browser);
    ShowPageControl(g_browserForward, browser);
    ShowPageControl(g_aiPrompt, ai);
    ShowPageControl(g_aiCopy, ai);
    ShowPageControl(g_aiSnip, ai);
    ShowPageControl(g_aiImport, ai);
    ShowPageControl(g_aiOpenStudio, ai);
    ShowPageControl(g_opacity, settings);
    ShowPageControl(g_topmost, settings);
    ShowPageControl(g_passthrough, settings);
    ShowPageControl(g_theme, settings);
    ShowPageControl(g_workNotes, workspace);
    ShowPageControl(g_workBrowser, workspace);
    ShowPageControl(g_workAI, workspace);
    ShowPageControl(g_openCaptureFolder, ai);

    for (size_t i = 0; i < g_navButtons.size(); ++i) {
        if (g_navButtons[i]) InvalidateRect(g_navButtons[i], nullptr, TRUE);
    }
    if (g_webController) g_webController->put_IsVisible(browser ? TRUE : FALSE);
    if (browser) EnsureWebView();
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

void OpenEdgeStudio() {
    std::vector<fs::path> candidates;
    wchar_t* env = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&env, &len, L"PROGRAMFILES(X86)") == 0 && env) {
        candidates.emplace_back(fs::path(env) / L"Microsoft/Edge/Application/msedge.exe");
        free(env);
    }
    if (_wdupenv_s(&env, &len, L"PROGRAMFILES") == 0 && env) {
        candidates.emplace_back(fs::path(env) / L"Microsoft/Edge/Application/msedge.exe");
        free(env);
    }
    candidates.emplace_back(fs::path(GetKnownFolder(FOLDERID_LocalAppData)) / L"Microsoft/Edge/Application/msedge.exe");
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            ShellExecuteW(nullptr, L"open", p.c_str(), L"https://aistudio.google.com/", nullptr, SW_SHOWNORMAL);
            return;
        }
    }
    ShellExecuteW(nullptr, L"open", L"https://aistudio.google.com/", nullptr, nullptr, SW_SHOWNORMAL);
}

void CopyTextToClipboard(const std::wstring& text) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem) {
        void* ptr = GlobalLock(mem);
        if (ptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        } else GlobalFree(mem);
    }
    CloseClipboard();
}

int GetPngEncoderClsid(CLSID* clsid) {
    UINT count = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0) return -1;
    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, size, codecs) != Gdiplus::Ok) return -1;
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) { *clsid = codecs[i].Clsid; return static_cast<int>(i); }
    }
    return -1;
}

std::wstring MakeCaptureName() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t name[100]{};
    swprintf_s(name, L"Capture_%04u%02u%02u_%02u%02u%02u.png", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return name;
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

    bool ok = false;
    if (bitmapHandle) {
        CLSID png{};
        Gdiplus::Bitmap bitmap(bitmapHandle, nullptr);
        ok = GetPngEncoderClsid(&png) >= 0 && bitmap.GetLastStatus() == Gdiplus::Ok &&
             bitmap.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
    }
    if (ownsBitmap && bitmapHandle) DeleteObject(bitmapHandle);
    if (dibData) GlobalUnlock(dibMemory);
    CloseClipboard();
    return ok;
}

void StartWindowsSnip() {
    // Explicitly launches the Windows Snipping Tool UI. The user chooses what to capture.
    INPUT downs[3]{};
    downs[0].type = INPUT_KEYBOARD; downs[0].ki.wVk = VK_LWIN;
    downs[1].type = INPUT_KEYBOARD; downs[1].ki.wVk = VK_SHIFT;
    downs[2].type = INPUT_KEYBOARD; downs[2].ki.wVk = 'S';
    SendInput(3, downs, sizeof(INPUT));
    INPUT ups[3]{};
    ups[0].type = INPUT_KEYBOARD; ups[0].ki.wVk = 'S'; ups[0].ki.dwFlags = KEYEVENTF_KEYUP;
    ups[1].type = INPUT_KEYBOARD; ups[1].ki.wVk = VK_SHIFT; ups[1].ki.dwFlags = KEYEVENTF_KEYUP;
    ups[2].type = INPUT_KEYBOARD; ups[2].ki.wVk = VK_LWIN; ups[2].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(3, ups, sizeof(INPUT));
}

void ImportCapture() {
    fs::path path = CaptureFolder() / MakeCaptureName();
    bool hasImage = IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5);
    if (!hasImage || !SaveClipboardBitmap(path)) {
        MessageBoxW(g_hwnd, L"No clipboard image was found. Take a snip with Win + Shift + S, finish the selection, then try Import again.", kAppName, MB_ICONWARNING);
        return;
    }
    g_lastCapturePath = path.wstring();
    std::wstring message = L"Capture saved to:\n" + g_lastCapturePath + L"\n\nThe image remains on your clipboard. Open AI Studio and press Ctrl+V to attach it.";
    MessageBoxW(g_hwnd, message.c_str(), L"Capture ready", MB_ICONINFORMATION);
}

void OpenCaptureFolder() {
    ShellExecuteW(nullptr, L"open", CaptureFolder().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ApplyTheme() {
    if (g_theme) g_themeIndex = static_cast<int>(SendMessageW(g_theme, CB_GETCURSEL, 0, 0));
    SaveSettings();
    InvalidateRect(g_hwnd, nullptr, TRUE);
    for (HWND h : { g_notesEdit, g_urlEdit, g_aiPrompt }) if (h) InvalidateRect(h, nullptr, TRUE);
}

void CreateControls(HWND hwnd) {
    g_hwnd = hwnd;
    const DWORD childVisible = WS_CHILD | WS_VISIBLE;

    g_navButtons = {
        MakeChild(0, L"BUTTON", L"Overview", childVisible | BS_OWNERDRAW, 12, 108, 180, 42, ID_NAV_HOME),
        MakeChild(0, L"BUTTON", L"Notes", childVisible | BS_OWNERDRAW, 12, 156, 180, 42, ID_NAV_NOTES),
        MakeChild(0, L"BUTTON", L"Browser", childVisible | BS_OWNERDRAW, 12, 204, 180, 42, ID_NAV_BROWSER),
        MakeChild(0, L"BUTTON", L"AI Assistant", childVisible | BS_OWNERDRAW, 12, 252, 180, 42, ID_NAV_AI),
        MakeChild(0, L"BUTTON", L"Workspace", childVisible | BS_OWNERDRAW, 12, 300, 180, 42, ID_NAV_WORKSPACE),
        MakeChild(0, L"BUTTON", L"Settings", childVisible | BS_OWNERDRAW, 12, 348, 180, 42, ID_NAV_SETTINGS)
    };

    g_notesEdit = MakeChild(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
        kContentLeft, 190, kControlWidth, 470, ID_NOTES_EDIT);
    g_notesSave = MakeChild(0, L"BUTTON", L"Save notes", childVisible | BS_OWNERDRAW, kContentLeft, 676, 140, 42, ID_NOTES_SAVE);

    g_urlEdit = MakeChild(WS_EX_CLIENTEDGE, L"EDIT", L"https://aistudio.google.com/", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
        kContentLeft, 150, 610, 34, ID_URL_EDIT);
    g_browserBack = MakeChild(0, L"BUTTON", L"Back", WS_CHILD | BS_OWNERDRAW, kContentLeft + 620, 150, 72, 34, ID_BROWSER_BACK);
    g_browserForward = MakeChild(0, L"BUTTON", L"Next", WS_CHILD | BS_OWNERDRAW, kContentLeft + 698, 150, 72, 34, ID_BROWSER_FORWARD);
    g_browserGo = MakeChild(0, L"BUTTON", L"Go", WS_CHILD | BS_OWNERDRAW, kContentLeft + 776, 150, 70, 34, ID_BROWSER_GO);

    g_aiPrompt = MakeChild(WS_EX_CLIENTEDGE, L"EDIT", L"Select text or describe what you need help with...", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
        kContentLeft, 190, 790, 210, ID_AI_PROMPT);
    g_aiCopy = MakeChild(0, L"BUTTON", L"Copy prompt", childVisible | BS_OWNERDRAW, kContentLeft, 420, 130, 42, ID_AI_COPY_PROMPT);
    g_aiSnip = MakeChild(0, L"BUTTON", L"Start screen snip", childVisible | BS_OWNERDRAW, kContentLeft + 142, 420, 170, 42, ID_AI_START_SNIP);
    g_aiImport = MakeChild(0, L"BUTTON", L"Import clipboard image", childVisible | BS_OWNERDRAW, kContentLeft + 324, 420, 190, 42, ID_AI_IMPORT_CAPTURE);
    g_aiOpenStudio = MakeChild(0, L"BUTTON", L"Open AI Studio in Edge", childVisible | BS_OWNERDRAW, kContentLeft, 478, 210, 42, ID_AI_OPEN_STUDIO);
    g_openCaptureFolder = MakeChild(0, L"BUTTON", L"Open capture folder", childVisible | BS_OWNERDRAW, kContentLeft + 222, 478, 180, 42, ID_CAPTURE_OPEN_FOLDER);

    g_workNotes = MakeChild(0, L"BUTTON", L"Open Notes", WS_CHILD | BS_OWNERDRAW, kContentLeft, 192, 180, 62, ID_WORK_OPEN_NOTES);
    g_workBrowser = MakeChild(0, L"BUTTON", L"Open Browser", WS_CHILD | BS_OWNERDRAW, kContentLeft + 194, 192, 180, 62, ID_WORK_OPEN_BROWSER);
    g_workAI = MakeChild(0, L"BUTTON", L"Open AI Assistant", WS_CHILD | BS_OWNERDRAW, kContentLeft + 388, 192, 200, 62, ID_WORK_OPEN_AI);

    g_opacity = MakeChild(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_HORZ, kContentLeft, 210, 360, 42, ID_SETTINGS_OPACITY);
    SendMessageW(g_opacity, TBM_SETRANGE, TRUE, MAKELONG(40, 100));
    SendMessageW(g_opacity, TBM_SETPOS, TRUE, g_opacityValue);
    g_topmost = MakeChild(0, L"BUTTON", L"Keep PrivaLayer above other windows", WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP, kContentLeft, 286, 330, 32, ID_SETTINGS_TOPMOST);
    g_passthrough = MakeChild(0, L"BUTTON", L"Pass mouse clicks through the overlay", WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP, kContentLeft, 330, 330, 32, ID_SETTINGS_PASSTHROUGH);
    g_theme = MakeChild(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, kContentLeft, 410, 250, 250, ID_SETTINGS_THEME);
    SendMessageW(g_theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Midnight Blue"));
    SendMessageW(g_theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Violet"));
    SendMessageW(g_theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
    SendMessageW(g_theme, CB_SETCURSEL, g_themeIndex, 0);
    SendMessageW(g_topmost, BM_SETCHECK, g_isTopmost ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_passthrough, BM_SETCHECK, g_isPassthrough ? BST_CHECKED : BST_UNCHECKED, 0);

    for (HWND h : { g_notesEdit, g_notesSave, g_urlEdit, g_browserGo, g_browserBack, g_browserForward,
                    g_aiPrompt, g_aiCopy, g_aiSnip, g_aiImport, g_aiOpenStudio, g_openCaptureFolder,
                    g_workNotes, g_workBrowser, g_workAI, g_opacity, g_topmost, g_passthrough, g_theme }) {
        ShowWindow(h, SW_HIDE);
    }
    SetControlFont(g_notesEdit, g_font);
    SetControlFont(g_aiPrompt, g_font);
    LoadNotes();
    SetPage(PAGE_HOME);
}

void DrawLabel(HDC dc, const wchar_t* text, int x, int y, COLORREF color, int size = 10, bool bold = false) {
    HFONT font = MakeFont(size, bold);
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text, static_cast<int>(wcslen(text)));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void DrawRoundedPanel(HDC dc, RECT rc, COLORREF color, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 14, 14);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void PaintMain(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc{}; GetClientRect(hwnd, &rc);
    const Palette& p = CurrentPalette();
    HBRUSH bg = CreateSolidBrush(p.bg);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    RECT side{ 0, 0, kSidebarWidth, rc.bottom };
    HBRUSH sideBrush = CreateSolidBrush(p.sidebar);
    FillRect(dc, &side, sideBrush);
    DeleteObject(sideBrush);
    HPEN line = CreatePen(PS_SOLID, 1, p.border);
    HGDIOBJ oldPen = SelectObject(dc, line);
    MoveToEx(dc, kSidebarWidth, 0, nullptr); LineTo(dc, kSidebarWidth, rc.bottom);
    MoveToEx(dc, kSidebarWidth, kHeaderHeight, nullptr); LineTo(dc, rc.right, kHeaderHeight);
    SelectObject(dc, oldPen); DeleteObject(line);

    if (g_logo && g_logo->GetLastStatus() == Gdiplus::Ok) {
        Gdiplus::Graphics graphics(dc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(g_logo, 20, 22, 44, 44);
    }
    DrawLabel(dc, L"PrivaLayer", 72, 29, p.text, 17, true);
    DrawLabel(dc, L"PERSONAL WORKSPACE", 73, 51, p.muted, 7, true);
    DrawLabel(dc, L"PRIVATE DESKTOP SPACE", 18, 82, p.muted, 7, true);

    const wchar_t* title = L"Overview";
    const wchar_t* subtitle = L"A focused workspace for notes, web research and AI assistance.";
    switch (g_page) {
        case PAGE_NOTES: title = L"Notes"; subtitle = L"Your notes are saved locally on this PC."; break;
        case PAGE_BROWSER: title = L"Browser"; subtitle = L"WebView2 browser. Website sign-in policies still apply."; break;
        case PAGE_AI: title = L"AI Assistant"; subtitle = L"Prepare a prompt, capture a region, then paste the image into AI Studio."; break;
        case PAGE_WORKSPACE: title = L"Workspace"; subtitle = L"Quick access to the four tools in PrivaLayer."; break;
        case PAGE_SETTINGS: title = L"Settings"; subtitle = L"Customize appearance and interaction."; break;
    }
    DrawLabel(dc, title, kContentLeft, 30, p.text, 19, true);
    DrawLabel(dc, subtitle, kContentLeft, 56, p.muted, 9, false);

    if (g_page == PAGE_HOME) {
        RECT card1{ kContentLeft, 150, kContentLeft + 280, 280 };
        RECT card2{ kContentLeft + 300, 150, kContentLeft + 580, 280 };
        RECT card3{ kContentLeft, 300, kContentLeft + 280, 430 };
        RECT card4{ kContentLeft + 300, 300, kContentLeft + 580, 430 };
        DrawRoundedPanel(dc, card1, p.panel, p.border); DrawRoundedPanel(dc, card2, p.panel, p.border);
        DrawRoundedPanel(dc, card3, p.panel, p.border); DrawRoundedPanel(dc, card4, p.panel, p.border);
        DrawLabel(dc, L"01  NOTES", card1.left + 18, card1.top + 18, p.accent2, 9, true);
        DrawLabel(dc, L"Keep key points close", card1.left + 18, card1.top + 52, p.text, 12, true);
        DrawLabel(dc, L"Quick notes saved locally.", card1.left + 18, card1.top + 82, p.muted, 9);
        DrawLabel(dc, L"02  BROWSER", card2.left + 18, card2.top + 18, p.accent2, 9, true);
        DrawLabel(dc, L"Research without switching", card2.left + 18, card2.top + 52, p.text, 11, true);
        DrawLabel(dc, L"Powered by Microsoft WebView2.", card2.left + 18, card2.top + 82, p.muted, 9);
        DrawLabel(dc, L"03  AI ASSISTANT", card3.left + 18, card3.top + 18, p.accent2, 9, true);
        DrawLabel(dc, L"Prepare prompts and captures", card3.left + 18, card3.top + 52, p.text, 11, true);
        DrawLabel(dc, L"Copy prompts and images to AI Studio.", card3.left + 18, card3.top + 82, p.muted, 9);
        DrawLabel(dc, L"04  WORKSPACE", card4.left + 18, card4.top + 18, p.accent2, 9, true);
        DrawLabel(dc, L"One place for your tools", card4.left + 18, card4.top + 52, p.text, 12, true);
        DrawLabel(dc, L"Quick access and appearance controls.", card4.left + 18, card4.top + 82, p.muted, 9);
        DrawLabel(dc, L"HOTKEYS", kContentLeft, 482, p.muted, 8, true);
        DrawLabel(dc, L"Ctrl + Alt + Space   Show / hide window", kContentLeft, 510, p.text, 10);
        DrawLabel(dc, L"Ctrl + Alt + I           Toggle click-through mode", kContentLeft, 537, p.text, 10);
        DrawLabel(dc, L"Ctrl + Alt + 1          Start Windows screen snip", kContentLeft, 564, p.text, 10);
        DrawLabel(dc, L"Note: PrivaLayer remains visible in normal screen sharing and recording.", kContentLeft, 620, p.muted, 8);
    }
    else if (g_page == PAGE_AI) {
        DrawLabel(dc, L"Prompt", kContentLeft, 166, p.muted, 9, true);
        DrawLabel(dc, L"Capture workflow", kContentLeft, 548, p.muted, 9, true);
        DrawLabel(dc, L"1. Start screen snip and select the area in Windows.", kContentLeft, 578, p.text, 9);
        DrawLabel(dc, L"2. Import the clipboard image. It is saved locally and stays on the clipboard.", kContentLeft, 602, p.text, 9);
        DrawLabel(dc, L"3. Open AI Studio in Edge and press Ctrl+V to attach it yourself.", kContentLeft, 626, p.text, 9);
        DrawLabel(dc, L"No background capture or automatic upload is performed.", kContentLeft, 658, p.muted, 8);
    }
    else if (g_page == PAGE_SETTINGS) {
        DrawLabel(dc, L"Window opacity", kContentLeft, 178, p.text, 10, true);
        wchar_t opacityText[32]{}; swprintf_s(opacityText, L"%d%%", g_opacityValue);
        DrawLabel(dc, opacityText, kContentLeft + 390, 220, p.accent2, 10, true);
        DrawLabel(dc, L"Interaction", kContentLeft, 252, p.text, 10, true);
        DrawLabel(dc, L"Appearance", kContentLeft, 382, p.text, 10, true);
        DrawLabel(dc, L"Pass-through mode leaves the overlay visible but sends mouse clicks to windows behind it.", kContentLeft, 370, p.muted, 8);
    }
    else if (g_page == PAGE_WORKSPACE) {
        DrawRoundedPanel(dc, RECT{ kContentLeft, 154, kContentLeft + 600, 300 }, p.panel, p.border);
        DrawLabel(dc, L"Your workspace", kContentLeft + 22, 174, p.text, 13, true);
        DrawLabel(dc, L"Choose a tool to open it. Notes and browser data remain on this computer.", kContentLeft + 22, 208, p.muted, 9);
    }
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        SetPassThrough(g_isPassthrough);
        RegisterHotKey(hwnd, HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT, VK_SPACE);
        RegisterHotKey(hwnd, HOTKEY_INTERACTIVE, MOD_CONTROL | MOD_ALT, 'I');
        RegisterHotKey(hwnd, HOTKEY_SNIP, MOD_CONTROL | MOD_ALT, '1');
        SetTopmost(g_isTopmost);
        return 0;
    case WM_PAINT:
        PaintMain(hwnd);
        return 0;
    case WM_SIZE:
        LayoutBrowser();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 1160;
        info->ptMinTrackSize.y = 780;
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == HOTKEY_TOGGLE) {
            ShowWindow(hwnd, IsWindowVisible(hwnd) ? SW_HIDE : SW_SHOW);
            if (IsWindowVisible(hwnd)) { SetForegroundWindow(hwnd); }
        } else if (wParam == HOTKEY_INTERACTIVE) {
            SetPassThrough(!g_isPassthrough);
        } else if (wParam == HOTKEY_SNIP) {
            SetPage(PAGE_AI);
            StartWindowsSnip();
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
            case ID_NAV_HOME: SetPage(PAGE_HOME); break;
            case ID_NAV_NOTES: SetPage(PAGE_NOTES); break;
            case ID_NAV_BROWSER: SetPage(PAGE_BROWSER); break;
            case ID_NAV_AI: SetPage(PAGE_AI); break;
            case ID_NAV_WORKSPACE: SetPage(PAGE_WORKSPACE); break;
            case ID_NAV_SETTINGS: SetPage(PAGE_SETTINGS); break;
            case ID_NOTES_SAVE: SaveNotes(); MessageBoxW(hwnd, L"Notes saved locally.", kAppName, MB_ICONINFORMATION); break;
            case ID_BROWSER_GO: NavigateBrowser(); break;
            case ID_BROWSER_BACK: if (g_webView) g_webView->GoBack(); break;
            case ID_BROWSER_FORWARD: if (g_webView) g_webView->GoForward(); break;
            case ID_AI_COPY_PROMPT: {
                int n = GetWindowTextLengthW(g_aiPrompt);
                std::wstring value(static_cast<size_t>(n) + 1, L'\0');
                GetWindowTextW(g_aiPrompt, value.data(), n + 1); value.resize(static_cast<size_t>(n));
                CopyTextToClipboard(value);
                MessageBoxW(hwnd, L"Prompt copied. Paste it into your AI chat.", kAppName, MB_ICONINFORMATION);
                break;
            }
            case ID_AI_START_SNIP: StartWindowsSnip(); break;
            case ID_AI_IMPORT_CAPTURE: ImportCapture(); break;
            case ID_AI_OPEN_STUDIO: OpenEdgeStudio(); break;
            case ID_CAPTURE_OPEN_FOLDER: OpenCaptureFolder(); break;
            case ID_WORK_OPEN_NOTES: SetPage(PAGE_NOTES); break;
            case ID_WORK_OPEN_BROWSER: SetPage(PAGE_BROWSER); break;
            case ID_WORK_OPEN_AI: SetPage(PAGE_AI); break;
            case ID_SETTINGS_TOPMOST:
                SetTopmost(SendMessageW(g_topmost, BM_GETCHECK, 0, 0) == BST_CHECKED); break;
            case ID_SETTINGS_PASSTHROUGH:
                SetPassThrough(SendMessageW(g_passthrough, BM_GETCHECK, 0, 0) == BST_CHECKED); break;
            case ID_SETTINGS_THEME: if (HIWORD(wParam) == CBN_SELCHANGE) ApplyTheme(); break;
        }
        if (HIWORD(wParam) == EN_CHANGE && id == ID_NOTES_EDIT) SaveNotes();
        if (HIWORD(wParam) == EN_KILLFOCUS && id == ID_URL_EDIT) NavigateBrowser();
        return 0;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == g_opacity) {
            g_opacityValue = static_cast<int>(SendMessageW(g_opacity, TBM_GETPOS, 0, 0));
            SetLayerOpacity(); InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis) return FALSE;
        const Palette& p = CurrentPalette();
        int id = static_cast<int>(dis->CtlID);
        bool nav = id >= ID_NAV_HOME && id <= ID_NAV_SETTINGS;
        bool active = (id == ID_NAV_HOME && g_page == PAGE_HOME) || (id == ID_NAV_NOTES && g_page == PAGE_NOTES) ||
                      (id == ID_NAV_BROWSER && g_page == PAGE_BROWSER) || (id == ID_NAV_AI && g_page == PAGE_AI) ||
                      (id == ID_NAV_WORKSPACE && g_page == PAGE_WORKSPACE) || (id == ID_NAV_SETTINGS && g_page == PAGE_SETTINGS);
        COLORREF fill = active ? p.panel2 : (dis->itemState & ODS_SELECTED ? p.panel2 : p.panel);
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(dis->hDC, &dis->rcItem, brush); DeleteObject(brush);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, active ? p.accent2 : p.text);
        SelectObject(dis->hDC, g_font);
        wchar_t label[128]{}; GetWindowTextW(dis->hwndItem, label, 127);
        RECT r = dis->rcItem; r.left += nav ? 18 : 12;
        DrawTextW(dis->hDC, label, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (active) {
            HBRUSH marker = CreateSolidBrush(p.accent);
            RECT m{ dis->rcItem.left, dis->rcItem.top + 8, dis->rcItem.left + 3, dis->rcItem.bottom - 8 };
            FillRect(dis->hDC, &m, marker); DeleteObject(marker);
        }
        return TRUE;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam); const Palette& p = CurrentPalette();
        SetTextColor(dc, p.text); SetBkColor(dc, p.panel2);
        static HBRUSH editBrush = nullptr; if (editBrush) DeleteObject(editBrush); editBrush = CreateSolidBrush(p.panel2);
        return reinterpret_cast<LRESULT>(editBrush);
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam); const Palette& p = CurrentPalette();
        SetTextColor(dc, p.text); SetBkColor(dc, p.bg);
        static HBRUSH staticBrush = nullptr; if (staticBrush) DeleteObject(staticBrush); staticBrush = CreateSolidBrush(p.bg);
        return reinterpret_cast<LRESULT>(staticBrush);
    }
    case WM_CLOSE:
        SaveNotes(); DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_TOGGLE); UnregisterHotKey(hwnd, HOTKEY_INTERACTIVE); UnregisterHotKey(hwnd, HOTKEY_SNIP);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    LoadSettings();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gdipInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdipInput, nullptr);
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_BAR_CLASSES };
    InitCommonControlsEx(&controls);

    g_font = MakeFont(9, false);
    g_fontBold = MakeFont(10, true);
    fs::path logoPath = ExecutableDirectory() / L"assets" / L"privalayer.png";
    if (fs::exists(logoPath)) g_logo = Gdiplus::Image::FromFile(logoPath.c_str());

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(kDark.bg);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return 1;

    DWORD exStyle = WS_EX_APPWINDOW | WS_EX_LAYERED;
    HWND hwnd = CreateWindowExW(exStyle, kClassName, L"PrivaLayer — Personal Workspace",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 790,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 2;
    g_hwnd = hwnd;
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIcon));
    SetLayerOpacity();
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_webController) g_webController->Close();
    g_webView.Reset(); g_webController.Reset(); g_webEnvironment.Reset();
    if (g_logo) delete g_logo;
    if (g_font) DeleteObject(g_font);
    if (g_fontBold) DeleteObject(g_fontBold);
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

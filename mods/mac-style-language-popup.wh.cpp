// ==WindhawkMod==
// @id              mac-style-language-popup
// @name            Mac-style Language Popup
// @description     Shows the input language list next to the text cursor whenever the input language changes, like macOS
// @version         0.3
// @author          Gasased
// @include         explorer.exe
// @compilerOptions -ld2d1 -ldwrite -ldwmapi -loleacc -lole32 -loleaut32 -luuid -lshcore -lshlwapi -lgdi32 -limm32
// @license         MIT
// @github          https://github.com/Gasased
// @homepage        https://github.com/Gasased/windhawk-language-selector
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Mac-style Language Popup

**The language switcher, right where you type.** Press Alt+Shift (or
Win+Space) and a small panel shows your languages next to the text cursor,
not in the corner of the screen.

![Typing in three languages with the panel at the cursor](https://raw.githubusercontent.com/Gasased/windhawk-language-selector/main/assets/demo.gif)

- **At the text cursor** in Notepad, browsers, Electron apps, Telegram,
  the Settings app and most other apps.
- **Out of the way.** Hides a moment after you let go of the keys, or as
  soon as you type.
- **One panel only.** Windows' own Win+Space switcher is hidden and this
  panel follows it.
- **Looks like Windows 11.** Accent color, light and dark theme.

## Styles

List or a single entry, vertical or horizontal, full names or just
ENG / УКР / 日本語. Change it on the **Settings** tab.

![All display styles in light and dark theme](https://raw.githubusercontent.com/Gasased/windhawk-language-selector/main/assets/styles.png)

## No text cursor?

On the desktop or in an app that doesn't report its cursor, Windows' own
switcher shows up as usual. To get this panel at the mouse pointer instead,
turn on *If there's no text cursor, show this panel at the mouse pointer*.

## Zed and other apps that draw their own cursor

Some apps (Zed, for example) draw the text cursor themselves, and the mod can
only find it from inside the app. To add such an app:

1. Open the **Advanced** tab of this mod.
2. Under **Custom process inclusion list**, add the app's file name, e.g.
   `Zed.exe` (one per line).
3. Click **Save** and restart the app.

## More

All settings, with animated examples:
[github.com/Gasased/windhawk-language-selector](https://github.com/Gasased/windhawk-language-selector)
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- windowsSwitcher: hide
  $name: Windows' own language switcher (Win+Space, Alt held + Shift)
  $options:
  - hide: Hide it, this mod's panel follows its selection
  - caret: Show it too, moved next to the text cursor
  - default: Show it too, where Windows puts it
- onlyNewLanguage: false
  $name: Show only the new language (like macOS)
  $description: Off = the whole language list with the new one highlighted

- orientation: vertical
  $name: List direction
  $options:
  - vertical: Vertical
  - horizontal: Horizontal
- reverseOrder: false
  $name: Reverse language order
- placement: below
  $name: Placement relative to the text cursor
  $description: Flips to the opposite side automatically when there is no room
  $options:
  - below: Below
  - above: Above
  - right: Right
  - left: Left
- offsetX: 0
  $name: Horizontal offset (px)
- offsetY: 4
  $name: Vertical offset (px)
- noCursorClassic: true
  $name: If there's no text cursor, keep Windows' usual behavior
  $description: Windows' own switcher appears in its usual place for Win+Space or Alt held + Shift, and a quick Alt+Shift shows nothing. Games and full-screen apps usually have no text cursor, so they get this too
- noCursorMouse: false
  $name: If there's no text cursor, show this panel at the mouse pointer
  $description: Takes priority over the option above
- useUIAutomation: true
  $name: Use UI Automation to find the text cursor
  $description: Needed for WinUI/UWP/WPF apps. Can make Chromium apps turn on their accessibility tree.
- rememberCaret: false
  $name: Remember the text cursor position in each window
  $description: For apps that hide their text cursor until you click or type after switching to them (Telegram and other Qt apps). The panel goes where the cursor last was, as long as the window hasn't moved
- appearDelay: 0
  $name: Delay before appearing (ms)
- holdDelay: 100
  $name: Show while the language hotkey is held, after (ms)
  $description: A quicker tap shows the panel when you let go. Lower = earlier, but shortcuts like Shift+Alt+F can flash it briefly
- hideDelay: 300
  $name: Time visible after the keys are released (ms)
  $description: Hovering the panel keeps it open
- hideOnTyping: true
  $name: Hide as soon as you start typing
- animation: slide
  $name: Animation
  $options:
  - slide: Fade and slide
  - zoom: Fade and zoom
  - fade: Fade
  - none: None
- fadeInDuration: 80
  $name: Appear animation duration (ms)
- fadeOutDuration: 220
  $name: Disappear animation duration (ms)
- slideDistance: 8
  $name: Slide distance (px)
- scale: 100
  $name: Size (%)
- opacity: 97
  $name: Opacity (%)
- cornerRadius: 8
  $name: Corner radius (px)
- shadow: true
  $name: Drop shadow
- showAbbreviation: true
  $name: Show language code (ENG, УКР)
- showLanguageName: true
  $name: Show language name (English (United States))
  $description: Turn off the name and the layout for compact boxes with just the code (ENG | УКР | РУС)
- showLayoutName: true
  $name: Show keyboard layout (US)
- panelPadding: 4
  $name: Panel padding (px)
  $description: Space between the panel edge and the language boxes
- itemPaddingX: 16
  $name: Box padding, left and right (px)
- itemPaddingY: 8
  $name: Box padding, top and bottom (px)
- columnGap: 12
  $name: Gap between the code and the name (px)
- itemSpacing: 2
  $name: Space between language boxes (px)
- nativeNames: false
  $name: Language names in their own language
  $description: For example Українська instead of Ukrainian
- theme: system
  $name: Theme
  $options:
  - system: Follow Windows
  - dark: Dark
  - light: Light
- accentColor: ""
  $name: Accent color
  $description: "#RRGGBB, empty = Windows accent color"
- fontFamily: Segoe UI Variable Text
  $name: Font
- clickToSwitch: false
  $name: Click a language to switch to it
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <imm.h>
#include <oleacc.h>
#include <shellscalingapi.h>
#include <shlwapi.h>
#include <uiautomation.h>

#include <math.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

using std::max;
using std::min;

// ---------------------------------------------------------------- settings

enum class Placement { Below, Above, Right, Left };
enum class Anim { Slide, Zoom, Fade, None };
enum class Theme { System, Dark, Light };
enum class NativePanel { Caret, Hide, Default };

struct {
    NativePanel nativePanel;
    bool onlyNew, codesOnly, horizontal, reverseOrder;
    Placement placement;
    int offsetX, offsetY;
    bool noCursorClassic, noCursorMouse;
    bool useUIA, rememberCaret;
    int appearDelay, holdDelay, showDuration, fadeIn, fadeOut;
    bool hideOnTyping;
    Anim anim;
    int slideDistance, scale, opacity, cornerRadius;
    bool shadow, showAbbr, showName, showLayout, nativeNames;
    float pad, padX, padY, gap, itemGap;  // spacing, DIPs
    Theme theme;
    std::wstring accent, font;
    bool clickToSwitch;
} g_s;

static std::wstring StrSetting(PCWSTR name) {
    PCWSTR v = Wh_GetStringSetting(name);
    std::wstring s = v ? v : L"";
    Wh_FreeStringSetting(v);
    return s;
}

static void LoadSettings() {
    auto is = [](PCWSTR name, PCWSTR val) { return StrSetting(name) == val; };
    g_s.nativePanel = is(L"windowsSwitcher", L"hide")      ? NativePanel::Hide
                      : is(L"windowsSwitcher", L"default") ? NativePanel::Default
                                                       : NativePanel::Caret;
    g_s.onlyNew = Wh_GetIntSetting(L"onlyNewLanguage");
    g_s.horizontal = is(L"orientation", L"horizontal");
    g_s.reverseOrder = Wh_GetIntSetting(L"reverseOrder");
    std::wstring p = StrSetting(L"placement");
    g_s.placement = p == L"above" ? Placement::Above
                    : p == L"right" ? Placement::Right
                    : p == L"left"  ? Placement::Left
                                    : Placement::Below;
    g_s.offsetX = Wh_GetIntSetting(L"offsetX");
    g_s.offsetY = Wh_GetIntSetting(L"offsetY");
    g_s.noCursorClassic = Wh_GetIntSetting(L"noCursorClassic");
    g_s.noCursorMouse = Wh_GetIntSetting(L"noCursorMouse");
    g_s.useUIA = Wh_GetIntSetting(L"useUIAutomation");
    g_s.rememberCaret = Wh_GetIntSetting(L"rememberCaret");
    g_s.appearDelay = max(0, Wh_GetIntSetting(L"appearDelay"));
    g_s.holdDelay = max(0, Wh_GetIntSetting(L"holdDelay"));
    g_s.showDuration = max(0, Wh_GetIntSetting(L"hideDelay"));
    g_s.hideOnTyping = Wh_GetIntSetting(L"hideOnTyping");
    g_s.fadeIn = max(0, Wh_GetIntSetting(L"fadeInDuration"));
    g_s.fadeOut = max(0, Wh_GetIntSetting(L"fadeOutDuration"));
    std::wstring a = StrSetting(L"animation");
    g_s.anim = a == L"zoom"   ? Anim::Zoom
               : a == L"fade" ? Anim::Fade
               : a == L"none" ? Anim::None
                              : Anim::Slide;
    g_s.slideDistance = Wh_GetIntSetting(L"slideDistance");
    g_s.scale = min(400, max(50, Wh_GetIntSetting(L"scale")));
    g_s.opacity = min(100, max(10, Wh_GetIntSetting(L"opacity")));
    g_s.cornerRadius = max(0, Wh_GetIntSetting(L"cornerRadius"));
    g_s.shadow = Wh_GetIntSetting(L"shadow");
    g_s.showAbbr = Wh_GetIntSetting(L"showAbbreviation");
    g_s.showName = Wh_GetIntSetting(L"showLanguageName");
    g_s.showLayout = Wh_GetIntSetting(L"showLayoutName");
    if (!g_s.showAbbr && !g_s.showName && !g_s.showLayout) g_s.showAbbr = true;  // something has to show
    g_s.codesOnly = g_s.showAbbr && !g_s.showName && !g_s.showLayout;      // compact cells
    auto px = [](PCWSTR name) { return (float)min(64, max(0, Wh_GetIntSetting(name))); };
    g_s.pad = px(L"panelPadding");
    g_s.padX = px(L"itemPaddingX");
    g_s.padY = px(L"itemPaddingY");
    g_s.gap = px(L"columnGap");
    g_s.itemGap = px(L"itemSpacing");
    g_s.nativeNames = Wh_GetIntSetting(L"nativeNames");
    std::wstring t = StrSetting(L"theme");
    g_s.theme = t == L"dark" ? Theme::Dark : t == L"light" ? Theme::Light : Theme::System;
    g_s.accent = StrSetting(L"accentColor");
    g_s.font = StrSetting(L"fontFamily");
    if (g_s.font.empty()) g_s.font = L"Segoe UI Variable Text";
    g_s.clickToSwitch = Wh_GetIntSetting(L"clickToSwitch");
}

// ---------------------------------------------------------------- language info

struct LangItem {
    HKL hkl;
    std::wstring abbr, name, layout;
    int order;           // position in the Settings language list
    float abbrW, textW, codeW;  // measured, DIPs (codeW = code in the name font, codes-only mode)
};

static std::wstring LocaleStr(PCWSTR locale, LCTYPE type) {
    wchar_t buf[256];
    return GetLocaleInfoEx(locale, type, buf, ARRAYSIZE(buf)) ? buf : L"";
}

static std::wstring RegStr(HKEY root, const std::wstring& key, PCWSTR value) {
    wchar_t buf[512];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key.c_str(), value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return L"";
    return buf;
}

// Language tag the user picked in Settings ("en-US", "uk") for an HKL's LANGID,
// so names match the Settings list ("Ukrainian", not "Ukrainian (Ukraine)").
// *order = position in that list, which is the order Windows shows them in.
static std::wstring UserLanguageTag(LANGID lang, int* order) {
    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    std::wstring primaryMatch;
    *order = 1000;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\International\\User Profile", L"Languages",
                     RRF_RT_REG_MULTI_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
        int i = 0;
        for (PCWSTR tag = buf; *tag; tag += wcslen(tag) + 1, i++) {
            LANGID l = LANGIDFROMLCID(LocaleNameToLCID(tag, LOCALE_ALLOW_NEUTRAL_NAMES));
            if (l == lang) return *order = i, tag;
            if (primaryMatch.empty() && PRIMARYLANGID(l) == PRIMARYLANGID(lang)) primaryMatch = tag, *order = i;
        }
    }
    if (!primaryMatch.empty()) return primaryMatch;
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    return LCIDToLocaleName(MAKELCID(lang, SORT_DEFAULT), name, ARRAYSIZE(name), 0) ? name : L"";
}

static std::wstring LayoutDisplayName(HKL hkl) {
    UINT_PTR v = (UINT_PTR)hkl;
    WORD device = HIWORD(v);
    wchar_t klid[KL_NAMELENGTH] = L"";
    if ((v & 0xF0000000) == 0xE0000000) {
        swprintf_s(klid, L"%08X", (DWORD)v);  // IME: KLID is the HKL itself
    } else if ((device & 0xF000) == 0xF000) {
        // Layout variant: find the KLID whose "Layout Id" matches.
        HKEY key;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts", 0,
                          KEY_READ, &key) == ERROR_SUCCESS) {
            wchar_t want[8];
            swprintf_s(want, L"%04x", device & 0x0FFF);
            wchar_t sub[KL_NAMELENGTH];
            DWORD n = ARRAYSIZE(sub);
            for (DWORD i = 0; RegEnumKeyExW(key, i, sub, &n, 0, 0, 0, 0) == ERROR_SUCCESS; i++, n = ARRAYSIZE(sub)) {
                if (!_wcsicmp(RegStr(key, sub, L"Layout Id").c_str(), want)) {
                    wcscpy_s(klid, sub);
                    break;
                }
            }
            RegCloseKey(key);
        }
    } else {
        swprintf_s(klid, L"%08X", (DWORD)device);
    }
    if (!*klid) return L"";
    std::wstring key = std::wstring(L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\") + klid;
    std::wstring indirect = RegStr(HKEY_LOCAL_MACHINE, key, L"Layout Display Name");
    wchar_t out[256];
    if (!indirect.empty() && SUCCEEDED(SHLoadIndirectString(indirect.c_str(), out, ARRAYSIZE(out), nullptr)))
        return out;
    return RegStr(HKEY_LOCAL_MACHINE, key, L"Layout Text");
}

static LangItem MakeItem(HKL hkl) {
    LangItem it{hkl};
    std::wstring tag = UserLanguageTag(LOWORD((UINT_PTR)hkl), &it.order);
    // Windows' indicator uses the first 3 letters of the language's own name:
    // English -> ENG, українська -> УКР, русский -> РУС, Deutsch -> DEU.
    std::wstring native = LocaleStr(tag.c_str(), LOCALE_SNATIVELANGUAGENAME).substr(0, 3);
    wchar_t up[16] = L"";
    LCMapStringEx(tag.c_str(), LCMAP_UPPERCASE, native.c_str(), -1, up, ARRAYSIZE(up), nullptr, nullptr, 0);
    it.abbr = up;
    it.name = LocaleStr(tag.c_str(), g_s.nativeNames ? LOCALE_SNATIVEDISPLAYNAME : LOCALE_SLOCALIZEDDISPLAYNAME);
    if (g_s.nativeNames && !it.name.empty()) {
        // native names come lowercase in some languages ("українська")
        LCMapStringEx(tag.c_str(), LCMAP_UPPERCASE, it.name.c_str(), 1, &it.name[0], 1, nullptr, nullptr, 0);
    }
    it.layout = LayoutDisplayName(hkl);
    if (it.name.empty()) it.name = it.layout;
    return it;
}

// ---------------------------------------------------------------- caret location

static IUIAutomation* g_uia;

// Popup thread only. Also called on settings reload, so turning the setting on works without a restart.
static void EnsureUIA() {
    if (!g_s.useUIA || g_uia) return;
    // bounded, so an app (or explorer itself) that doesn't answer can't hang this thread
    IUIAutomation2* uia = nullptr;
    CoCreateInstance(__uuidof(CUIAutomation8), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia));
    if (uia) uia->put_ConnectionTimeout(1500), uia->put_TransactionTimeout(1500), g_uia = uia;
    else CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_uia));
}

static bool RangeRect(IUIAutomationTextRange* r, RECT* out) {
    auto firstRect = [out](IUIAutomationTextRange* x, bool rightEdge) {
        SAFEARRAY* sa = nullptr;
        if (FAILED(x->GetBoundingRectangles(&sa)) || !sa) return false;
        LONG ub = -1;
        SafeArrayGetUBound(sa, 1, &ub);
        double* d;
        bool ok = ub >= 3 && SUCCEEDED(SafeArrayAccessData(sa, (void**)&d));
        if (ok) {
            LONG x0 = (LONG)(rightEdge ? d[0] + d[2] : d[0]);
            *out = {x0, (LONG)d[1], x0 + 1, (LONG)(d[1] + d[3])};
            SafeArrayUnaccessData(sa);
            ok = d[3] > 0;
        }
        SafeArrayDestroy(sa);
        return ok;
    };
    if (firstRect(r, false)) return true;
    // Degenerate (empty) caret range has no rectangle: measure the next
    // character's left edge, or at end of text the previous one's right edge.
    IUIAutomationTextRange* c = nullptr;
    bool ok = false;
    if (SUCCEEDED(r->Clone(&c)) && c) {
        c->ExpandToEnclosingUnit(TextUnit_Character);
        ok = firstRect(c, false);
        c->Release();
    }
    if (!ok && SUCCEEDED(r->Clone(&c)) && c) {
        int moved = 0;
        c->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, -1, &moved);
        ok = moved && firstRect(c, true);
        c->Release();
    }
    return ok;
}

static bool CaretFromUIA(RECT* out) {
    if (!g_uia) return false;
    IUIAutomationElement* el = nullptr;
    if (FAILED(g_uia->GetFocusedElement(&el)) || !el) return false;
    IUIAutomationTextRange* range = nullptr;
    IUIAutomationTextPattern2* tp2 = nullptr;
    if (SUCCEEDED(el->GetCurrentPatternAs(UIA_TextPattern2Id, IID_PPV_ARGS(&tp2))) && tp2) {
        BOOL active;
        tp2->GetCaretRange(&active, &range);
        tp2->Release();
    }
    if (!range) {
        IUIAutomationTextPattern* tp = nullptr;
        if (SUCCEEDED(el->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(&tp))) && tp) {
            IUIAutomationTextRangeArray* arr = nullptr;
            if (SUCCEEDED(tp->GetSelection(&arr)) && arr) {
                int n = 0;
                arr->get_Length(&n);
                if (n > 0) arr->GetElement(0, &range);
                arr->Release();
            }
            tp->Release();
        }
    }
    bool ok = range && RangeRect(range, out);
    if (range) range->Release();
    el->Release();
    return ok;
}

static bool CaretFromMSAA(HWND focus, RECT* out) {
    IAccessible* acc = nullptr;
    if (FAILED(AccessibleObjectFromWindow(focus, OBJID_CARET, IID_PPV_ARGS(&acc))) || !acc) return false;
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    long x, y, w, h;
    bool ok = SUCCEEDED(acc->accLocation(&x, &y, &w, &h, self)) && h > 0 && (x || y);
    if (ok) *out = {x, y, x + max(w, 1L), y + h};
    acc->Release();
    return ok;
}

// Client rect in the window's own DPI space -> physical screen pixels.
static void ClientToPhysical(HWND hwnd, const RECT& rc, RECT* out) {
    DPI_AWARENESS_CONTEXT prev = SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(hwnd));
    POINT pts[2] = {{rc.left, rc.top}, {rc.right, rc.bottom}};
    ClientToScreen(hwnd, &pts[0]);
    ClientToScreen(hwnd, &pts[1]);
    SetThreadDpiAwarenessContext(prev);
    LogicalToPhysicalPointForPerMonitorDPI(hwnd, &pts[0]);
    LogicalToPhysicalPointForPerMonitorDPI(hwnd, &pts[1]);
    *out = {pts[0].x, pts[0].y, max(pts[1].x, pts[0].x + 1), pts[1].y};
}

static bool CaretFromWin32(const GUITHREADINFO& gti, RECT* out) {
    if (!gti.hwndCaret || IsRectEmpty(&gti.rcCaret)) return false;
    RECT rc = gti.rcCaret;
    // Qt's caret is an invisible 2x2 dot at the top of the text line: assume a ~20 DIP line
    int line = MulDiv(20, GetDpiForWindow(gti.hwndCaret), 96);
    if (rc.bottom - rc.top < line / 3) rc.bottom = rc.top + line;
    ClientToPhysical(gti.hwndCaret, rc, out);
    return true;
}

// Apps that draw their own caret and expose it nowhere (Zed) still tell the IME
// where it is. Inside those apps the mod hooks the IMM calls and leaves the
// point on the window as a property, which works across processes.
constexpr PCWSTR CARET_PROP = L"MacStyleLanguagePopup.Caret";

static HANDLE PackPoint(POINT p) {
    return (HANDLE)((1ull << 40) | (WORD)(short)p.x | ((UINT_PTR)(WORD)(short)p.y << 16));  // bit 40: never NULL
}

static bool CaretFromImeProp(HWND hwnd, RECT* out) {
    UINT_PTR v = hwnd ? (UINT_PTR)GetPropW(hwnd, CARET_PROP) : 0;
    if (!v) return false;
    int x = (short)(v & 0xFFFF), y = (short)((v >> 16) & 0xFFFF);
    // the point is mid-line (Zed's convention); the line height isn't passed, assume ~20 DIPs
    int half = MulDiv(10, GetDpiForWindow(hwnd), 96);
    ClientToPhysical(hwnd, {x, y - half, x + 1, y + half}, out);
    return true;
}

using ImmSetCompositionWindow_t = BOOL(WINAPI*)(HIMC, LPCOMPOSITIONFORM);
using ImmSetCandidateWindow_t = BOOL(WINAPI*)(HIMC, LPCANDIDATEFORM);
static ImmSetCompositionWindow_t ImmSetCompositionWindow_Original;
static ImmSetCandidateWindow_t ImmSetCandidateWindow_Original;

// Both run on the app's UI thread, so GetFocus() is the window the point is relative to.
static void SaveImePoint(POINT p) {
    if (HWND f = GetFocus()) SetPropW(f, CARET_PROP, PackPoint(p));
}

static BOOL WINAPI ImmSetCompositionWindow_Hook(HIMC himc, LPCOMPOSITIONFORM cf) {
    if (cf && cf->dwStyle & (CFS_POINT | CFS_FORCE_POSITION)) SaveImePoint(cf->ptCurrentPos);
    return ImmSetCompositionWindow_Original(himc, cf);
}

static BOOL WINAPI ImmSetCandidateWindow_Hook(HIMC himc, LPCANDIDATEFORM cf) {
    if (cf && cf->dwStyle == CFS_CANDIDATEPOS) SaveImePoint(cf->ptCurrentPos);
    return ImmSetCandidateWindow_Original(himc, cf);
}

// The window whose thread really receives keyboard input.
static HWND InputWindow(HWND fg) {
    wchar_t cls[64];
    if (GetClassNameW(fg, cls, ARRAYSIZE(cls)) && !wcscmp(cls, L"ApplicationFrameWindow")) {
        if (HWND core = FindWindowExW(fg, nullptr, L"Windows.UI.Core.CoreWindow", nullptr)) return core;
    }
    return fg;
}

// Last Win32 caret seen per top-level window (Poll samples the foreground one).
// Qt apps (Telegram) destroy their caret when the window loses focus and only
// create it again on the first click or key press, so right after Alt+Tab there
// is none (and their UIA caret is off or missing). Same window, not moved
// since: the text box is still where it was.
struct SeenCaret { RECT caret, win; };
static std::unordered_map<HWND, SeenCaret> g_seenCarets;

static void RememberCaret(HWND fg, const GUITHREADINFO& gti) {
    SeenCaret s;
    if (!CaretFromWin32(gti, &s.caret) || !GetWindowRect(fg, &s.win)) return;
    if (g_seenCarets.size() > 64) g_seenCarets.clear();  // ponytail: crude bound, closed windows pile up otherwise
    g_seenCarets[fg] = s;
}

static bool RememberedCaret(HWND fg, RECT* out) {
    auto it = g_seenCarets.find(fg);
    RECT win;
    if (it == g_seenCarets.end() || !GetWindowRect(fg, &win) || !EqualRect(&win, &it->second.win)) return false;
    Wh_Log(L"caret from memory");
    *out = it->second.caret;
    return true;
}

static bool FindCaret(RECT* out) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    GUITHREADINFO gti{sizeof(gti)};
    GetGUIThreadInfo(GetWindowThreadProcessId(InputWindow(fg), nullptr), &gti);
    if (CaretFromWin32(gti, out)) return true;
    if (CaretFromImeProp(gti.hwndFocus, out)) return true;
    if (g_s.rememberCaret && RememberedCaret(fg, out)) return true;  // ahead of the accessibility guesses
    if (gti.hwndFocus && CaretFromMSAA(gti.hwndFocus, out)) return true;
    return g_s.useUIA && CaretFromUIA(out);
}

// ---------------------------------------------------------------- popup

enum { TIMER_POLL = 1, TIMER_APPEAR, TIMER_HIDE, TIMER_CHORD };
enum { WM_APP_RELOAD = WM_APP + 1, WM_APP_PLACE_NATIVE, WM_APP_FLYOUT_SEL, WM_APP_FLYOUT_SHOWN, WM_APP_HAS_ANCHOR,
       WM_APP_FRAME };
enum class State { Hidden, In, Shown, Out };

static HWND g_hwnd;
static HANDLE g_thread;
static HANDLE g_mutex;

static ID2D1Factory* g_d2d;
static IDWriteFactory* g_dw;
static ID2D1DCRenderTarget* g_rt;
static HDC g_memDC;
static HBITMAP g_dib;
static SIZE g_dibSize;

static std::deque<LangItem> g_cache;  // per-HKL info, rebuilt on settings change; push_back keeps pointers valid
static std::vector<LangItem*> g_items;  // what the popup currently shows
static HKL g_current;
static State g_state = State::Hidden;
static double g_animStart;  // NowMs()
static float g_dpiScale = 1;        // physical px per DIP
static Placement g_side;            // placement after flipping
static POINT g_pos;                  // panel's final window position, physical
static float g_W, g_H, g_rowH, g_abbrCol, g_codeCol;  // panel size in DIPs

// last observed input state
static HWND g_lastFg;
static HKL g_lastHkl;
static ULONGLONG g_fgChangedAt;

constexpr float LINE1 = 20, LINE2 = 16, LINE_GAP = 2;  // text line heights (name, layout), DIPs

static float Margin() { return g_s.shadow ? 18.f : 1.f; }

static D2D1_COLOR_F Rgba(UINT rgb, float a) {
    return {((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, a};
}

static bool IsDark() {
    if (g_s.theme != Theme::System) return g_s.theme == Theme::Dark;
    DWORD light = 0, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return !light;
}

static UINT AccentRgb(bool dark) {
    if (g_s.accent.size() == 7 && g_s.accent[0] == L'#' &&
        std::all_of(g_s.accent.begin() + 1, g_s.accent.end(), [](wchar_t c) { return iswxdigit(c); }))
        return wcstoul(g_s.accent.c_str() + 1, nullptr, 16);
    // AccentPalette: 8 RGBA entries, light3..light1, accent, dark1..dark3. The
    // flyouts use a light shade on dark theme and a dark shade on light theme.
    BYTE pal[32];
    DWORD size = sizeof(pal);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
                     L"AccentPalette", RRF_RT_REG_BINARY, nullptr, pal, &size) == ERROR_SUCCESS) {
        BYTE* c = pal + (dark ? 1 : 4) * 4;
        return (c[0] << 16) | (c[1] << 8) | c[2];
    }
    return dark ? 0x60CDFF : 0x005FB8;
}

static IDWriteTextFormat* MakeFormat(float size, DWRITE_FONT_WEIGHT weight) {
    IDWriteTextFormat* f = nullptr;
    g_dw->CreateTextFormat(g_s.font.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                           size, L"", &f);
    if (f) {
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return f;
}

static IDWriteTextFormat *g_fAbbr, *g_fName, *g_fSub;

static void ReleaseFormats() {
    for (auto f : {&g_fAbbr, &g_fName, &g_fSub})
        if (*f) (*f)->Release(), *f = nullptr;
}

static float TextWidth(const std::wstring& s, IDWriteTextFormat* f) {
    if (s.empty() || !f) return 0;
    IDWriteTextLayout* l = nullptr;
    if (FAILED(g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, 4000, 100, &l))) return 0;
    DWRITE_TEXT_METRICS m;
    l->GetMetrics(&m);
    l->Release();
    return ceilf(m.widthIncludingTrailingWhitespace);
}

static LangItem* CachedItem(HKL hkl) {
    for (auto& it : g_cache)
        if (it.hkl == hkl) return &it;
    LangItem it = MakeItem(hkl);
    it.abbrW = TextWidth(it.abbr, g_fAbbr);
    it.codeW = TextWidth(it.abbr, g_fName);
    // a layout shown without the name takes the name's place and font
    it.textW = max(g_s.showName ? TextWidth(it.name, g_fName) : 0.f,
                   g_s.showLayout ? TextWidth(it.layout, g_s.showName ? g_fSub : g_fName) : 0.f);
    g_cache.push_back(it);
    return &g_cache.back();
}

static float ItemWidth(const LangItem* it) {
    if (g_s.codesOnly) return g_s.padX * 2 + g_codeCol;  // equal cells, whatever the code
    bool text = g_s.showName || g_s.showLayout;
    return g_s.padX * 2 + (g_s.showAbbr ? g_abbrCol + (text ? g_s.gap : 0) : 0) + it->textW;
}

static void EnsureFormats() {
    if (g_fName) return;
    g_fAbbr = MakeFormat(12, DWRITE_FONT_WEIGHT_NORMAL);
    g_fName = MakeFormat(14, DWRITE_FONT_WEIGHT_NORMAL);
    g_fSub = MakeFormat(12, DWRITE_FONT_WEIGHT_NORMAL);
}

// Installed layouts in the Settings language order (= the order Windows' switcher uses).
static std::vector<HKL> OrderedLayouts() {
    EnsureFormats();
    HKL list[64];
    int n = GetKeyboardLayoutList(ARRAYSIZE(list), list);
    std::vector<LangItem*> items;
    for (int i = 0; i < n; i++) items.push_back(CachedItem(list[i]));
    std::stable_sort(items.begin(), items.end(), [](auto a, auto b) { return a->order < b->order; });
    std::vector<HKL> out;
    for (auto it : items) out.push_back(it->hkl);
    return out;
}

static void BuildItems() {
    g_items.clear();
    EnsureFormats();
    if (g_s.onlyNew) {
        g_items.push_back(CachedItem(g_current));
    } else {
        for (HKL h : OrderedLayouts()) g_items.push_back(CachedItem(h));
        if (g_s.reverseOrder) std::reverse(g_items.begin(), g_items.end());
    }
    // Only-new-language mode is sized for the widest language, so the panel keeps
    // its size and spot while you press through them.
    std::vector<LangItem*> sizing = g_items;
    if (g_s.onlyNew)
        for (HKL h : OrderedLayouts()) sizing.push_back(CachedItem(h));
    g_abbrCol = 0;
    g_codeCol = 0;
    for (auto it : sizing) g_abbrCol = max(g_abbrCol, it->abbrW), g_codeCol = max(g_codeCol, it->codeW);
    g_rowH = g_s.padY * 2 + (g_s.showName && g_s.showLayout ? LINE1 + LINE_GAP + LINE2 : LINE1);
    float maxW = 0, sumW = 0;
    for (auto it : sizing) maxW = max(maxW, ItemWidth(it));
    for (auto it : g_items) sumW += ItemWidth(it);
    size_t n = g_items.size();
    if (g_s.horizontal && !g_s.onlyNew) {
        g_W = g_s.pad * 2 + sumW + g_s.itemGap * (n - 1);
        g_H = g_s.pad * 2 + g_rowH;
    } else {
        g_W = g_s.pad * 2 + maxW;
        g_H = g_s.pad * 2 + g_rowH * n + g_s.itemGap * (n - 1);
    }
}

// Item rect in panel DIPs (panel origin = top-left of the visible panel).
static D2D1_RECT_F ItemRect(size_t i) {
    if (g_s.horizontal && !g_s.onlyNew) {
        float x = g_s.pad;
        for (size_t k = 0; k < i; k++) x += ItemWidth(g_items[k]) + g_s.itemGap;
        return {x, g_s.pad, x + ItemWidth(g_items[i]), g_s.pad + g_rowH};
    }
    float y = g_s.pad + i * (g_rowH + g_s.itemGap);
    return {g_s.pad, y, g_W - g_s.pad, y + g_rowH};
}

static float EaseOut(float t) { return 1 - powf(1 - t, 3); }

static LONGLONG QpcFreq() {
    static LONGLONG f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x.QuadPart; }();
    return f;
}

static LONGLONG Qpc() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart;
}

// Animation clock. Not GetTickCount64: it moves in 15.6 ms steps, so some frames
// would repeat a position and others jump two.
static double NowMs() { return Qpc() * 1000.0 / QpcFreq(); }

// 0..1 visibility for the current animation frame.
static float Progress() {
    if (g_s.anim == Anim::None) return g_state == State::In || g_state == State::Shown ? 1 : 0;
    float e = (float)(NowMs() - g_animStart);
    if (g_state == State::In) return g_s.fadeIn ? min(1.f, e / g_s.fadeIn) : 1;
    if (g_state == State::Out) return g_s.fadeOut ? max(0.f, 1 - e / g_s.fadeOut) : 0;
    return g_state == State::Shown ? 1 : 0;
}

static bool g_dirty;  // the DIB doesn't hold the current panel picture

static bool EnsureSurface(SIZE sz) {
    if (g_dib && sz.cx == g_dibSize.cx && sz.cy == g_dibSize.cy) return true;
    if (g_dib) DeleteObject(g_dib), g_dib = nullptr;
    g_dirty = true;
    BITMAPINFO bi{{sizeof(BITMAPINFOHEADER), sz.cx, -sz.cy, 1, 32, BI_RGB}};
    void* bits;
    g_dib = CreateDIBSection(g_memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!g_dib) return false;
    SelectObject(g_memDC, g_dib);
    g_dibSize = sz;
    RECT rc{0, 0, sz.cx, sz.cy};
    return SUCCEEDED(g_rt->BindDC(g_memDC, &rc));
}

static bool g_dark;
static UINT g_accentRgb;

// Draws the panel into the DIB. Fade and slide only change the window's alpha and
// position, so that happens once per show; zoom redraws every frame.
static bool Draw(float e, float s, float m) {
    bool dark = g_dark;
    float op = g_s.opacity / 100.f, r = (float)g_s.cornerRadius;
    UINT accent = g_accentRgb;

    g_rt->BeginDraw();
    g_rt->Clear({0, 0, 0, 0});
    D2D1_MATRIX_3X2_F t = D2D1::Matrix3x2F::Translation(m, m) * D2D1::Matrix3x2F::Scale(s, s);
    if (g_s.anim == Anim::Zoom) {
        // grow out of the side facing the caret
        float z = 0.9f + 0.1f * e;
        D2D1_POINT_2F c = {(g_W / 2 + m) * s, (g_H / 2 + m) * s};
        if (g_side == Placement::Below) c.y = m * s;
        if (g_side == Placement::Above) c.y = (g_H + m) * s;
        if (g_side == Placement::Right) c.x = m * s;
        if (g_side == Placement::Left) c.x = (g_W + m) * s;
        t = t * D2D1::Matrix3x2F::Scale(z, z, c);
    }
    g_rt->SetTransform(t);

    ID2D1SolidColorBrush* b = nullptr;
    g_rt->CreateSolidColorBrush({0, 0, 0, 0}, &b);
    if (!b) {
        g_rt->EndDraw();
        return false;
    }
    D2D1_RECT_F panel{0, 0, g_W, g_H};
    if (g_s.shadow) {
        // stacked translucent rounded rects = cheap soft shadow, offset down a bit
        for (float k = m - 2; k >= 1; k -= 1) {
            b->SetColor({0, 0, 0, 0.022f * (1 - k / m) * (dark ? 1.6f : 1.f)});
            D2D1_RECT_F rr{-k, -k + 3, g_W + k, g_H + k + 3};
            g_rt->FillRoundedRectangle({rr, r + k, r + k}, b);
        }
    }
    b->SetColor(Rgba(dark ? 0x2C2C2C : 0xF9F9F9, op));
    g_rt->FillRoundedRectangle({panel, r, r}, b);
    b->SetColor(dark ? Rgba(0xFFFFFF, 0.08f) : Rgba(0x000000, 0.06f));
    g_rt->DrawRoundedRectangle({{0.5f, 0.5f, g_W - 0.5f, g_H - 0.5f}, r, r}, b, 1);

    UINT fg = dark ? 0xFFFFFF : 0x1A1A1A;
    for (size_t i = 0; i < g_items.size(); i++) {
        LangItem* it = g_items[i];
        D2D1_RECT_F rc = ItemRect(i);
        float ir = max(0.f, r - g_s.pad + 1);  // box corners follow the panel's
        if (it->hkl == g_current && !g_s.onlyNew) {
            b->SetColor(dark ? Rgba(0xFFFFFF, 0.07f) : Rgba(0x000000, 0.05f));
            g_rt->FillRoundedRectangle({rc, ir, ir}, b);
            // selection pill: on the left in a list, centered underneath in a row (like Windows 11 tabs)
            float cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
            D2D1_RECT_F pill = g_s.horizontal ? D2D1_RECT_F{cx - 8, rc.bottom - 4, cx + 8, rc.bottom - 1}
                                              : D2D1_RECT_F{rc.left, cy - 8, rc.left + 3, cy + 8};
            b->SetColor(Rgba(accent, 1));
            g_rt->FillRoundedRectangle({pill, 1.5f, 1.5f}, b);
        }
        if (g_s.codesOnly) {
            // centered code; in a row it sits 1 px higher to clear the accent mark under it
            float x = (rc.left + rc.right - it->codeW) / 2, lift = g_s.horizontal && !g_s.onlyNew ? 1.f : 0.f;
            b->SetColor(Rgba(fg, 1));
            g_rt->DrawText(it->abbr.c_str(), (UINT32)it->abbr.size(), g_fName,
                           {x, rc.top - lift, x + it->codeW + 4, rc.bottom - lift}, b);
            continue;
        }
        // first line: name (or the layout when the name is off); second line: layout under the name
        float x = rc.left + g_s.padX, top = rc.top + g_s.padY;
        D2D1_RECT_F line1{x, top, rc.right, top + LINE1}, line2{x, top + LINE1 + LINE_GAP, rc.right, top + LINE1 + LINE_GAP + LINE2};
        if (g_s.showAbbr) {
            b->SetColor(Rgba(fg, 0.9f));
            g_rt->DrawText(it->abbr.c_str(), (UINT32)it->abbr.size(), g_fAbbr, {x, line1.top, x + g_abbrCol + 4, line1.bottom}, b);
            line1.left = line2.left = x + g_abbrCol + g_s.gap;
        }
        if (g_s.showName) {
            b->SetColor(Rgba(fg, 1));
            g_rt->DrawText(it->name.c_str(), (UINT32)it->name.size(), g_fName, line1, b);
        }
        if (g_s.showLayout) {
            b->SetColor(Rgba(fg, g_s.showName ? 0.62f : 1));
            g_rt->DrawText(it->layout.c_str(), (UINT32)it->layout.size(), g_s.showName ? g_fSub : g_fName,
                           g_s.showName ? line2 : line1, b);
        }
    }
    b->Release();
    if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET) {
        g_rt->Release();
        g_rt = nullptr;
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        g_d2d->CreateDCRenderTarget(&props, &g_rt);
        if (g_dib) DeleteObject(g_dib), g_dib = nullptr;  // forces BindDC on the new target
        return false;
    }
    return true;
}

static void Render() {
    if (!g_rt) return;
    float p = Progress();
    // decelerate in, accelerate out
    float e = g_state == State::Out ? p * p : EaseOut(p);
    float s = g_dpiScale * g_s.scale / 100.f, m = Margin();
    SIZE sz{(LONG)ceilf((g_W + 2 * m) * s), (LONG)ceilf((g_H + 2 * m) * s)};
    if (!EnsureSurface(sz)) return;
    if (g_dirty || g_s.anim == Anim::Zoom) {
        if (!Draw(e, s, m)) return;
        g_dirty = false;
    }

    POINT pos = g_pos;
    if (g_s.anim == Anim::Slide) {
        // start nearer the caret and move outward
        int d = (int)((1 - e) * g_s.slideDistance * g_dpiScale);
        if (g_side == Placement::Below) pos.y -= d;
        if (g_side == Placement::Above) pos.y += d;
        if (g_side == Placement::Right) pos.x -= d;
        if (g_side == Placement::Left) pos.x += d;
    }
    POINT src{0, 0};
    BLENDFUNCTION bf{AC_SRC_OVER, 0, (BYTE)(255 * e), AC_SRC_ALPHA};
    UpdateLayeredWindow(g_hwnd, nullptr, &pos, &sz, g_memDC, &src, 0, &bf, ULW_ALPHA);
}

struct Anchor {
    RECT rc;      // caret (or the mouse pointer standing in), physical px
    float dpi;    // monitor scale, physical px per DIP
    RECT wa;      // monitor work area
};

// Where a panel should go: the caret, else (setting) the mouse pointer. false = no anchor:
// this mod shows nothing and Windows' own switcher keeps its usual place.
static bool FindAnchor(Anchor* a) {
    bool haveCaret = FindCaret(&a->rc);
    Wh_Log(L"caret %d: %ld,%ld %ldx%ld", haveCaret, a->rc.left, a->rc.top, a->rc.right - a->rc.left,
           a->rc.bottom - a->rc.top);
    if (!haveCaret) {
        if (!g_s.noCursorMouse) return false;
        POINT pt;
        GetCursorPos(&pt);
        a->rc = {pt.x, pt.y, pt.x + 1, pt.y + 20};  // pretend the pointer is a caret
    }
    HMONITOR mon = MonitorFromRect(&a->rc, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    a->dpi = dpiX / 96.f;
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    a->wa = mi.rcWork;
    return true;
}

// Top-left (physical px) for a w x h panel next to the anchor, flipped to the
// other side when it doesn't fit and clamped to the work area. `inset` = how far
// the panel's first text column is from its left edge, so text lines up with the caret.
static POINT PlaceNear(const Anchor& a, int w, int h, int inset, Placement* side) {
    const RECT& c = a.rc;
    const RECT& wa = a.wa;
    int ox = (int)(g_s.offsetX * a.dpi), oy = (int)(g_s.offsetY * a.dpi), gap = (int)(4 * a.dpi);
    int x, y;
    *side = g_s.placement;
    if (*side == Placement::Below && c.bottom + gap + oy + h > wa.bottom) *side = Placement::Above;
    else if (*side == Placement::Above && c.top - gap - oy - h < wa.top) *side = Placement::Below;
    else if (*side == Placement::Right && c.right + gap + ox + w > wa.right) *side = Placement::Left;
    else if (*side == Placement::Left && c.left - gap - ox - w < wa.left) *side = Placement::Right;
    int alignX = c.left - inset + ox;
    int midY = (c.top + c.bottom - h) / 2 + oy;
    switch (*side) {
        case Placement::Below: x = alignX, y = c.bottom + gap + oy; break;
        case Placement::Above: x = alignX, y = c.top - gap - oy - h; break;
        case Placement::Right: x = c.right + gap + ox, y = midY; break;
        case Placement::Left: x = c.left - gap - ox - w, y = midY; break;
    }
    int em = (int)(8 * a.dpi);  // keep off the screen edges like other flyouts
    return {max((int)wa.left + em, min(x, (int)wa.right - em - w)), max((int)wa.top + em, min(y, (int)wa.bottom - em - h))};
}

static Anchor g_anchor;  // where the visible panel was placed

// keepAnchor: the panel is already on screen, so repeated switches reuse its
// anchor instead of looking the caret up again. The panel then can't jump
// around between presses, even when an app reports the caret inconsistently.
static bool PlacePanel(bool keepAnchor) {
    if (!keepAnchor && !FindAnchor(&g_anchor)) return false;
    const Anchor& a = g_anchor;
    g_dpiScale = a.dpi;
    float s = g_dpiScale * g_s.scale / 100.f;
    int w = (int)ceilf(g_W * s), h = (int)ceilf(g_H * s), m = (int)ceilf(Margin() * s);
    POINT p = PlaceNear(a, w, h, (int)((g_s.pad + g_s.padX) * s), &g_side);
    g_pos = {p.x - m, p.y - m};
    Wh_Log(L"panel %dx%d at %ld,%ld (dpi scale %.2f)", w, h, p.x, p.y, g_dpiScale);
    return true;
}

static void StartHideTimer() { SetTimer(g_hwnd, TIMER_HIDE, g_s.showDuration, nullptr); }

// Animation frames are timed to the display: one per refresh, requested ~3 ms
// before DWM composes, so each lands on the next screen frame. Not SetTimer: its
// ~15.6 ms ticks beat against a 60 Hz display and skip a frame now and then.
static HANDLE g_frameThread, g_frameWake;
static std::atomic<bool> g_animating, g_framePending, g_frameQuit;

static DWORD WINAPI FrameThread(void*) {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);  // before Windows 10 1803
    while (WaitForSingleObject(g_frameWake, INFINITE) == WAIT_OBJECT_0 && !g_frameQuit) {
        while (g_animating && !g_frameQuit) {
            LONGLONG now = Qpc(), freq = QpcFreq(), lead = freq * 3 / 1000;
            LONGLONG vblank = now, period = freq / 60;
            DWM_TIMING_INFO ti{sizeof(ti)};
            if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &ti)) && ti.qpcRefreshPeriod > 0)
                vblank = ti.qpcVBlank, period = ti.qpcRefreshPeriod;
            // next vblank that is at least `lead` away, minus `lead`
            LONGLONG target = vblank + ((now + lead - vblank) / period + 1) * period - lead;
            LARGE_INTEGER due{.QuadPart = -max(1LL, (target - now) * 10000000 / freq)};  // relative, 100 ns
            if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                WaitForSingleObject(timer, 100);
            else
                Sleep(16);
            if (!g_framePending.exchange(true)) PostMessageW(g_hwnd, WM_APP_FRAME, 0, 0);  // no pile-up if we lag
        }
    }
    if (timer) CloseHandle(timer);
    return 0;
}

static void Animate() {
    g_animating = true;
    SetEvent(g_frameWake);
}

static void ShowPopup() {
    g_dark = IsDark();
    g_accentRgb = AccentRgb(g_dark);
    BuildItems();
    if (g_items.empty() || !PlacePanel(g_state != State::Hidden)) return;
    g_dirty = true;  // language, theme or list may have changed
    if (g_state == State::Out) {
        // Caught while fading out: fade back in from the current frame instead of
        // restarting, so it doesn't blink or slide again.
        float e = Progress();
        e *= e;  // what Render() showed for this frame
        float p = 1 - cbrtf(1 - e);  // the In progress that looks the same
        g_state = State::In;
        g_animStart = NowMs() - p * g_s.fadeIn;
        Animate();
    } else if (g_state == State::Hidden) {
        g_state = State::In;
        g_animStart = NowMs();
        Animate();
    }
    Render();
    // Win+Space's own flyout is topmost too; re-assert z-order every show.
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    StartHideTimer();
}

// ---------------------------------------------------------------- Windows' own panel

// InputSwitch.dll (in explorer) shows the Win+Space panel as a top-level
// Shell_InputSwitchTopLevelWindow on its own thread, and only ever moves it with
// SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE)
// before it's drawn. Rewriting x/y there moves the real panel.
static std::atomic<HWND> g_flyout;
static ULONGLONG g_flyoutSeenAt;
static std::atomic<bool> g_flyoutClassic;  // this time Windows' switcher shows itself (no text cursor)

static bool IsFlyout(HWND hwnd) {
    if (hwnd == g_flyout) return true;
    wchar_t cls[40];
    if (!GetClassNameW(hwnd, cls, ARRAYSIZE(cls)) || wcscmp(cls, L"Shell_InputSwitchTopLevelWindow")) return false;
    g_flyout = hwnd;
    return true;
}

static bool FlyoutShowing() {
    HWND f = g_flyout;
    BOOL cloaked = FALSE;
    return f && IsWindowVisible(f) &&
           !(SUCCEEDED(DwmGetWindowAttribute(f, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked);
}

using SetWindowPos_t = decltype(&SetWindowPos);
static SetWindowPos_t SetWindowPos_Original;

static BOOL WINAPI SetWindowPos_Hook(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags) {
    if (IsFlyout(hwnd)) Wh_Log(L"flyout SetWindowPos %d,%d %dx%d flags=%X", x, y, cx, cy, flags);
    if (!(flags & SWP_NOMOVE) && g_s.nativePanel != NativePanel::Default && IsFlyout(hwnd)) {
        if (g_s.nativePanel == NativePanel::Hide) {
            // No text cursor (and no mouse-pointer option) + "keep Windows' usual
            // behavior": leave Windows' switcher where it is and stay out of the way.
            // The caret check runs on the popup thread (see below); if it can't
            // answer in time, hide: two panels is the worse outcome.
            DWORD_PTR anchor = 1;
            if (g_s.noCursorClassic && !g_s.noCursorMouse && g_hwnd &&
                !SendMessageTimeoutW(g_hwnd, WM_APP_HAS_ANCHOR, 0, 0, SMTO_ABORTIFHUNG, 300, &anchor))
                anchor = 1;
            g_flyoutClassic = !anchor;
            if (anchor) {
                x = y = -32000;
                Wh_Log(L"native panel moved off-screen");
                if (g_hwnd) PostMessageW(g_hwnd, WM_APP_FLYOUT_SHOWN, 0, 0);
            } else {
                Wh_Log(L"native panel left in place: no text cursor");
            }
        } else if (g_hwnd) {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            int w = (flags & SWP_NOSIZE) ? rc.right - rc.left : cx;
            int h = (flags & SWP_NOSIZE) ? rc.bottom - rc.top : cy;
            // Caret lookup can be slow (UIA), so it runs on the popup thread,
            // never on the flyout's own UI thread; give up after 300 ms.
            POINT p;
            DWORD_PTR ok = 0;
            if (SendMessageTimeoutW(g_hwnd, WM_APP_PLACE_NATIVE, MAKEWPARAM(w, h), (LPARAM)&p,
                                    SMTO_ABORTIFHUNG, 300, &ok) && ok) {
                Wh_Log(L"native panel %dx%d: %d,%d -> %ld,%ld", w, h, x, y, p.x, p.y);
                x = p.x, y = p.y;
            } else {
                Wh_Log(L"native panel: no position (ok=%d, err=%u)", (int)ok, GetLastError());
            }
        }
    }
    return SetWindowPos_Original(hwnd, after, x, y, cx, cy, flags);
}

// Which entry Windows' switcher has selected right now (it moves on every
// Space / Shift press while Win / Alt is held; the switch itself only happens on
// release). Its list items are "Windows.Shell.InputApp.InputSwitcherItem_<n>",
// n = position in the Settings language order. -1 = unknown.
static int FlyoutSelection(IUIAutomation* uia) {
    HWND f = g_flyout;
    if (!uia || !f) return -1;
    IUIAutomationElement *root = nullptr, *sel = nullptr;
    IUIAutomationCondition* cond = nullptr;
    VARIANT v;
    v.vt = VT_BOOL;
    v.boolVal = VARIANT_TRUE;
    int idx = -1;
    if (SUCCEEDED(uia->ElementFromHandle(f, &root)) && root &&
        SUCCEEDED(uia->CreatePropertyCondition(UIA_SelectionItemIsSelectedPropertyId, v, &cond)) && cond &&
        SUCCEEDED(root->FindFirst(TreeScope_Descendants, cond, &sel)) && sel) {
        BSTR id = nullptr;
        sel->get_CurrentAutomationId(&id);
        PCWSTR n = id && wcsstr(id, L"InputSwitcherItem_") ? wcsrchr(id, L'_') + 1 : nullptr;
        if (n) idx = _wtoi(n);
        SysFreeString(id);
    }
    if (sel) sel->Release();
    if (cond) cond->Release();
    if (root) root->Release();
    return idx;
}

// The switcher lives in this process, and querying it from the popup thread can
// deadlock (its UI thread may be waiting on ours at that moment). So it's asked
// from a separate MTA thread, with UIA timeouts, which posts the answer back.
static HANDLE g_selThread, g_selWake;
static std::atomic<bool> g_selQuit;

static DWORD WINAPI SelectionThread(void*) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IUIAutomation2* uia = nullptr;
    CoCreateInstance(__uuidof(CUIAutomation8), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia));
    if (uia) uia->put_ConnectionTimeout(500), uia->put_TransactionTimeout(500);
    while (WaitForSingleObject(g_selWake, INFINITE) == WAIT_OBJECT_0 && !g_selQuit) {
        int i = FlyoutSelection(uia);
        if (i >= 0) PostMessageW(g_hwnd, WM_APP_FLYOUT_SEL, i, 0);
    }
    if (uia) uia->Release();
    CoUninitialize();
    return 0;
}

// Windows' switcher (open off-screen) has entry i selected: show that in this
// panel instead, and keep the panel up for as long as the switcher is open.
static void OnFlyoutSelection(int i) {
    if (g_flyoutClassic) return;
    if (GetTickCount64() - g_flyoutSeenAt > 200) return;  // stale: switcher already closed
    auto list = OrderedLayouts();
    if (i >= (int)list.size()) return;
    if (list[i] != g_current || g_state == State::Hidden || g_state == State::Out) {
        g_current = list[i];
        KillTimer(g_hwnd, TIMER_APPEAR);
        ShowPopup();
    } else {
        StartHideTimer();
    }
}

// The layout Windows switches to next from `from` (Settings order, wrapping).
static HKL NextLayout(HKL from, bool back) {
    auto list = OrderedLayouts();
    auto it = std::find(list.begin(), list.end(), from);
    if (list.size() < 2 || it == list.end()) return nullptr;
    size_t i = it - list.begin(), n = list.size();
    return list[back ? (i + n - 1) % n : (i + 1) % n];
}

// Show the language Windows is about to switch to, before the app has
// processed the switch (it lands a poll or two later and then just confirms).
static ULONGLONG g_predictedAt;  // a shown prediction not yet confirmed by a real switch

static void ShowPredicted(bool back) {
    HKL next = NextLayout(g_lastHkl, back);
    if (!next) return;
    g_current = next;
    g_predictedAt = GetTickCount64();
    KillTimer(g_hwnd, TIMER_APPEAR);
    ShowPopup();
}

static void FadeOut() {
    if (g_state == State::Hidden || g_state == State::Out) return;
    Wh_Log(L"fade out");
    KillTimer(g_hwnd, TIMER_HIDE);
    // from the current frame: a panel still fading in fades straight back out
    float shown = g_state == State::In ? EaseOut(Progress()) : 1;  // what Render() shows now
    g_state = State::Out;
    g_animStart = NowMs() - (1 - sqrtf(shown)) * g_s.fadeOut;
    Animate();
}

static bool Down(int vk) { return GetAsyncKeyState(vk) < 0; }

// The language hotkey (Settings > Typing > Advanced keyboard settings > Input
// language hot keys): 1 = Alt+Shift (default), 2 = Ctrl+Shift, 3 = none, 4 = grave.
static int g_hotkey = 1;

static void LoadHotkey() {
    std::wstring v = RegStr(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", L"Language Hotkey");
    if (v.empty()) v = RegStr(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", L"Hotkey");
    g_hotkey = v.empty() ? 1 : _wtoi(v.c_str());
}

// Windows switches when the Alt+Shift (Ctrl+Shift) chord is released without
// another key in between. Watching the chord lets the panel come up right at
// release instead of after the app catches up, and while the chord is held.
static ULONGLONG g_chordSince;  // 0 = chord not down
static bool g_chordSpoiled;      // another key went down during it
static bool g_chordShown;        // panel already shows its prediction

static void WatchChord(ULONGLONG now) {
    if (g_s.nativePanel != NativePanel::Hide || (g_hotkey != 1 && g_hotkey != 2)) return;
    bool alt = g_hotkey == 1;
    int mod = alt ? VK_MENU : VK_CONTROL, other = alt ? VK_CONTROL : VK_MENU;
    int modL = alt ? VK_LMENU : VK_LCONTROL, modR = alt ? VK_RMENU : VK_RCONTROL;
    bool chord = Down(VK_SHIFT) && Down(mod) && !Down(other) && !Down(VK_LWIN) && !Down(VK_RWIN);
    if (chord) {
        for (int vk = 0x08; vk <= 0xFE && !g_chordSpoiled; vk++) {  // from 0x08: mouse buttons don't count
            if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == mod || vk == modL || vk == modR) continue;
            if (Down(vk)) g_chordSpoiled = true;
        }
        if (!g_chordSince) {
            g_chordSince = now, g_chordShown = false;
            Wh_Log(L"chord down");
            // a short tap of another key would slip between 50 ms polls
            SetTimer(g_hwnd, TIMER_CHORD, 10, nullptr);
        }
        if (g_chordSpoiled) {
            // e.g. Shift+Alt+F in an editor: no switch is coming, take the preview back
            if (g_chordShown) g_chordShown = false, g_predictedAt = 0, FadeOut();
            return;
        }
        if (!g_chordShown && now - g_chordSince >= (ULONGLONG)g_s.holdDelay) {  // held: preview
            g_chordShown = true;
            ShowPredicted(false);
        } else if (g_chordShown && g_state != State::Hidden) {
            StartHideTimer();  // keep it up while held
        }
        return;
    }
    if (g_chordSince) Wh_Log(L"chord up, spoiled=%d", g_chordSpoiled);
    if (g_chordSince && !g_chordSpoiled && !g_chordShown) ShowPredicted(false);  // tapped
    if (g_chordSince && g_chordShown && g_predictedAt) g_predictedAt = now;  // held: the switch comes now, wait from here
    if (g_chordSince) KillTimer(g_hwnd, TIMER_CHORD);
    g_chordSince = 0;
    g_chordSpoiled = false;
}

// A key going down while the panel is up (outside the language hotkey and
// Windows' switcher) = typing: hide. Keys already down when the panel came up
// (Space still held after Win+Space) don't count, only new presses.
// ponytail: sampled every 50 ms, a tap shorter than that is missed; typing taps are longer.
static void WatchTyping() {
    static bool wasDown[256];
    bool armed = g_s.hideOnTyping && (g_state == State::In || g_state == State::Shown) && !g_chordSince &&
                 !FlyoutShowing() && !Down(VK_LWIN) && !Down(VK_RWIN);
    for (int vk = 0x08; vk <= 0xFE; vk++) {  // from 0x08: mouse buttons don't count
        bool down = Down(vk);
        bool modifier = (vk >= VK_SHIFT && vk <= VK_MENU) || (vk >= VK_LSHIFT && vk <= VK_RMENU) ||
                        vk == VK_LWIN || vk == VK_RWIN;
        if (armed && down && !wasDown[vk] && !modifier) {
            Wh_Log(L"typing: hide");  // no key code: the log must not record what is typed
            g_predictedAt = 0;
            FadeOut();
            armed = false;
        }
        wasDown[vk] = down;
    }
}

static void Poll() {
    ULONGLONG now = GetTickCount64();
    if (FlyoutShowing()) {
        g_flyoutSeenAt = now;
        if (g_s.nativePanel == NativePanel::Hide && !g_flyoutClassic && g_selWake) SetEvent(g_selWake);
    }
    WatchChord(now);
    WatchTyping();
    if (g_predictedAt && !g_chordSince && now - g_predictedAt > 400) {
        g_predictedAt = 0;  // predicted a switch that never came
        FadeOut();
    }
    HWND fg = GetForegroundWindow();
    if (!fg || fg == g_hwnd) return;
    HWND input = InputWindow(fg);
    DWORD tid = GetWindowThreadProcessId(input, nullptr);
    GUITHREADINFO gti{sizeof(gti)};
    if (GetGUIThreadInfo(tid, &gti) && gti.hwndFocus) tid = GetWindowThreadProcessId(gti.hwndFocus, nullptr);
    if (g_s.rememberCaret) RememberCaret(fg, gti);
    HKL hkl = GetKeyboardLayout(tid);
    if (fg != g_lastFg) {
        g_lastFg = fg;
        g_fgChangedAt = now;
    }
    if (hkl == g_lastHkl) return;
    bool firstPoll = !g_lastHkl;
    g_lastHkl = hkl;
    g_predictedAt = 0;
    // A different layout right after a focus change is the new window's own
    // layout (per-app input mode / activation sync), not a user switch.
    Wh_Log(L"layout %p -> fg %p, first=%d, sinceFocus=%llu ms", hkl, fg, firstPoll, now - g_fgChangedAt);
    if (firstPoll || now - g_fgChangedAt < 300) return;
    // Windows' panel was on screen for this switch (it commits on key release):
    // don't show a second one, unless that panel is hidden by this mod.
    if (g_s.nativePanel != NativePanel::Hide && now - g_flyoutSeenAt < 500) return;
    g_current = hkl;
    g_dirty = true;  // a panel still up shows the new highlight on its next frame
    if (g_s.appearDelay) SetTimer(g_hwnd, TIMER_APPEAR, g_s.appearDelay, nullptr);
    else ShowPopup();
}

static int HitItem(LPARAM lp) {
    float s = g_dpiScale * g_s.scale / 100.f, m = Margin();
    float x = GET_X_LPARAM(lp) / s - m, y = GET_Y_LPARAM(lp) / s - m;
    for (size_t i = 0; i < g_items.size(); i++) {
        D2D1_RECT_F r = ItemRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return (int)i;
    }
    return -1;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TIMER:
            switch (wp) {
                case TIMER_POLL: Poll(); return 0;
                case TIMER_CHORD: WatchChord(GetTickCount64()); return 0;
                case TIMER_APPEAR: KillTimer(hwnd, TIMER_APPEAR); ShowPopup(); return 0;
                case TIMER_HIDE: {
                    POINT pt;
                    RECT rc;
                    GetCursorPos(&pt);
                    GetWindowRect(hwnd, &rc);
                    if (PtInRect(&rc, pt)) return 0;  // hovered: keep it, timer fires again
                    // a key of the language hotkey still held (Shift down, tapping Alt
                    // to step through languages): keep it, hide after the release
                    if ((g_hotkey == 1 || g_hotkey == 2) &&
                        (Down(VK_SHIFT) || Down(g_hotkey == 1 ? VK_MENU : VK_CONTROL)))
                        return 0;
                    FadeOut();
                    return 0;
                }
            }
            break;
        case WM_APP_FRAME: {
            g_framePending = false;
            float p = Progress();
            if (g_state == State::In && p >= 1) g_state = State::Shown;
            if (g_state == State::Out && p <= 0) {
                g_state = State::Hidden;
                ShowWindow(hwnd, SW_HIDE);
            }
            if (g_state == State::Shown || g_state == State::Hidden) g_animating = false;
            if (g_state != State::Hidden) Render();
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONUP:
            if (g_s.clickToSwitch && !g_s.onlyNew) {
                int i = HitItem(lp);
                if (i >= 0) {
                    HWND fg = GetForegroundWindow();
                    PostMessageW(InputWindow(fg), WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)g_items[i]->hkl);
                }
            }
            return 0;
        case WM_APP_PLACE_NATIVE: {
            Anchor a;
            if (!FindAnchor(&a)) return 0;
            Placement side;
            // Windows' panel: text column starts ~16 DIPs in (4 margin + 12 padding)
            *(POINT*)lp = PlaceNear(a, LOWORD(wp), HIWORD(wp), (int)(16 * a.dpi), &side);
            return 1;
        }
        case WM_APP_HAS_ANCHOR: {
            if (g_state != State::Hidden) return 1;  // panel already up at its anchor
            RECT rc;
            return FindCaret(&rc);
        }
        case WM_APP_FLYOUT_SEL:
            OnFlyoutSelection((int)wp);
            return 0;
        case WM_APP_FLYOUT_SHOWN:
            // Windows' switcher just opened (off-screen). Its first selection is the
            // next language (previous with Shift held): show that right away, the
            // UIA read confirms or corrects it a moment later.
            g_flyoutSeenAt = GetTickCount64();
            if (g_state == State::Hidden || g_state == State::Out) ShowPredicted(Down(VK_SHIFT) && Down(VK_LWIN));
            g_predictedAt = 0;  // the switcher, not a hotkey, owns this one
            if (g_selWake) SetEvent(g_selWake);
            return 0;
        case WM_APP_RELOAD:
            LoadSettings();
            LoadHotkey();
            EnsureUIA();
            g_cache.clear();
            ReleaseFormats();
            return 0;
        case WM_SETTINGCHANGE:
            g_cache.clear();  // language list, names or hotkey may have changed
            LoadHotkey();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static DWORD WINAPI PopupThread(void*) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EnsureUIA();
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw);
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (g_d2d) g_d2d->CreateDCRenderTarget(&props, &g_rt);
    g_memDC = CreateCompatibleDC(nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.lpszClassName = L"MacStyleLanguagePopup";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName,
                             L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
    if (g_hwnd && g_rt && g_dw) {
        LoadHotkey();
        g_selWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_selThread = CreateThread(nullptr, 0, SelectionThread, nullptr, 0, nullptr);
        g_frameWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_frameThread = g_frameWake ? CreateThread(nullptr, 0, FrameThread, nullptr, 0, nullptr) : nullptr;
        if (g_frameThread) {  // without it no animation would ever finish: better no panel at all
            SetTimer(g_hwnd, TIMER_POLL, 50, nullptr);
            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0)) DispatchMessageW(&msg);
            g_frameQuit = true;
            SetEvent(g_frameWake);
            WaitForSingleObject(g_frameThread, INFINITE);  // waits at most one frame
            CloseHandle(g_frameThread);
        } else {
            Wh_Log(L"init failed: no frame thread");
            DestroyWindow(g_hwnd);
        }
        if (g_frameWake) CloseHandle(g_frameWake);
        g_frameThread = g_frameWake = nullptr;
        g_selQuit = true;
        SetEvent(g_selWake);
        if (g_selThread) WaitForSingleObject(g_selThread, INFINITE), CloseHandle(g_selThread);  // UIA timeouts bound this
        CloseHandle(g_selWake);
        g_selThread = g_selWake = nullptr;
    } else {
        Wh_Log(L"init failed: hwnd=%p rt=%p dw=%p", g_hwnd, g_rt, g_dw);
        if (g_hwnd) DestroyWindow(g_hwnd);
    }
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    ReleaseFormats();
    g_cache.clear();
    g_items.clear();
    if (g_dib) DeleteObject(g_dib);
    if (g_memDC) DeleteDC(g_memDC);
    if (g_rt) g_rt->Release();
    if (g_dw) g_dw->Release();
    if (g_d2d) g_d2d->Release();
    if (g_uia) g_uia->Release();
    CoUninitialize();
    return 0;
}

// ---------------------------------------------------------------- Windhawk

BOOL Wh_ModInit() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, ARRAYSIZE(exe));
    if (_wcsicmp(PathFindFileNameW(exe), L"explorer.exe")) {
        // an app without a readable caret: only report its IME position (see CaretFromImeProp)
        HMODULE imm = LoadLibraryW(L"imm32.dll");
        Wh_SetFunctionHook((void*)GetProcAddress(imm, "ImmSetCompositionWindow"), (void*)ImmSetCompositionWindow_Hook,
                           (void**)&ImmSetCompositionWindow_Original);
        Wh_SetFunctionHook((void*)GetProcAddress(imm, "ImmSetCandidateWindow"), (void*)ImmSetCandidateWindow_Hook,
                           (void**)&ImmSetCandidateWindow_Original);
        return TRUE;
    }
    // Explorer can run several processes; one popup is enough.
    g_mutex = CreateMutexW(nullptr, FALSE, L"Local\\MacStyleLanguagePopup");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return TRUE;
    }
    LoadSettings();
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original);
    g_thread = CreateThread(nullptr, 0, PopupThread, nullptr, 0, nullptr);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    if (g_hwnd) PostMessageW(g_hwnd, WM_APP_RELOAD, 0, 0);
}

void Wh_ModUninit() {
    if (g_thread) {
        // wait until the window exists so the close message isn't lost
        while (!g_hwnd && WaitForSingleObject(g_thread, 10) == WAIT_TIMEOUT) {}
        if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
    }
    if (g_mutex) CloseHandle(g_mutex);
}

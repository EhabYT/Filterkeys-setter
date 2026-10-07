// Theme.cpp : colour palette and theming helpers.

#include "pch.h"
#include "Theme.h"
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

namespace
{
	const TCHAR* const kSettingsKey = _T("Software\\FilterKeysSetter");
	const TCHAR* const kThemeValue  = _T("Theme");

	// Palette taken from the application icon.
	const ThemePalette kDarkPalette = {
		RGB(0x2A, 0x4A, 0x7B),  // back top      #2A4A7B
		RGB(0x1F, 0x3A, 0x61),  // back middle   #1F3A61
		RGB(0x14, 0x26, 0x3F),  // back bottom   #14263F
		RGB(0x1D, 0x51, 0x88),  // surface       #1D5188
		RGB(0x5D, 0x9C, 0xD6),  // accent        #5D9CD6
		RGB(0x37, 0x78, 0xB5),  // accent muted  #3778B5
		RGB(0xFF, 0xFF, 0xFF),  // text          #FFFFFF
		RGB(0xE0, 0xE0, 0xE0),  // secondary     #E0E0E0
		// Light enough to stay above 4.5:1 on the lightest band of the
		// gradient (#2A4A7B), where the old #8CA4C4 only reached 3.5:1.
		RGB(0xA8, 0xBB, 0xD6),  // disabled      #A8BBD6
	};

	// Light mode deliberately stays close to the system colours so the dialog
	// keeps looking like a native Windows utility.
	ThemePalette MakeLightPalette()
	{
		ThemePalette p;
		p.clrBackTop        = ::GetSysColor(COLOR_3DFACE);
		p.clrBackMid        = ::GetSysColor(COLOR_3DFACE);
		p.clrBackBottom     = ::GetSysColor(COLOR_3DFACE);
		p.clrSurface        = ::GetSysColor(COLOR_WINDOW);
		p.clrAccent         = RGB(0x1D, 0x51, 0x88);
		p.clrAccentMuted    = RGB(0x37, 0x78, 0xB5);
		p.clrText           = ::GetSysColor(COLOR_WINDOWTEXT);
		p.clrTextSecondary  = ::GetSysColor(COLOR_GRAYTEXT);
		p.clrTextDisabled   = ::GetSysColor(COLOR_GRAYTEXT);
		return p;
	}

	// DWMWA_USE_IMMERSIVE_DARK_MODE. The attribute was 19 in Windows 10 1809
	// and became 20 from 1903 onwards.
	const DWORD kDarkModeAttribute       = 20;
	const DWORD kDarkModeAttributeLegacy = 19;
}

CTheme::CTheme()
	: m_mode(ThemeMode::Dark)
	, m_brushBack(NULL)
	, m_brushSurface(NULL)
{
	m_palette = kDarkPalette;
	Rebuild();
}

CTheme::~CTheme()
{
	if (m_brushBack) ::DeleteObject(m_brushBack);
	if (m_brushSurface) ::DeleteObject(m_brushSurface);
}

ThemeMode CTheme::LoadPreference()
{
	DWORD dwValue = 1;  // dark by default, matching the application icon
	HKEY hKey = NULL;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD dwType = 0;
		DWORD dwSize = sizeof(dwValue);
		DWORD dwRead = 0;
		if (::RegQueryValueEx(hKey, kThemeValue, NULL, &dwType,
		                      reinterpret_cast<LPBYTE>(&dwRead), &dwSize) == ERROR_SUCCESS
		    && dwType == REG_DWORD) {
			dwValue = dwRead;
		}
		::RegCloseKey(hKey);
	}
	return (dwValue == 0) ? ThemeMode::Light : ThemeMode::Dark;
}

void CTheme::SavePreference(ThemeMode mode)
{
	HKEY hKey = NULL;
	DWORD dwDisposition = 0;
	if (::RegCreateKeyEx(HKEY_CURRENT_USER, kSettingsKey, 0, NULL, REG_OPTION_NON_VOLATILE,
	                     KEY_WRITE, NULL, &hKey, &dwDisposition) == ERROR_SUCCESS) {
		DWORD dwValue = (mode == ThemeMode::Dark) ? 1 : 0;
		::RegSetValueEx(hKey, kThemeValue, 0, REG_DWORD,
		                reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue));
		::RegCloseKey(hKey);
	}
}

bool CTheme::IsHighContrast()
{
	HIGHCONTRAST hc = { 0 };
	hc.cbSize = sizeof(hc);
	if (!::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
		return false;
	}
	return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

void CTheme::SetMode(ThemeMode mode)
{
	// High contrast always wins over the stored preference.
	if (IsHighContrast()) {
		mode = ThemeMode::Light;
	}

	m_mode = mode;
	m_palette = (mode == ThemeMode::Dark) ? kDarkPalette : MakeLightPalette();
	Rebuild();
}

void CTheme::Rebuild()
{
	if (m_brushBack) {
		::DeleteObject(m_brushBack);
	}
	if (m_brushSurface) {
		::DeleteObject(m_brushSurface);
	}
	m_brushBack = ::CreateSolidBrush(m_palette.clrBackTop);
	m_brushSurface = ::CreateSolidBrush(m_palette.clrSurface);
}

void CTheme::PaintBackground(CDC& dc, const CRect& rect) const
{
	PaintBackgroundSlice(dc, rect, rect);
}

void CTheme::PaintBackgroundSlice(CDC& dc, const CRect& full, const CRect& target) const
{
	if (!IsDark()) {
		dc.FillSolidRect(target, m_palette.clrBackTop);
		return;
	}

	// A plain banded gradient. GradientFill would pull in msimg32 for no real
	// gain at this size, and 128 bands are indistinguishable from smooth here.
	const int bands = 128;
	const int height = full.Height();
	if (height <= 0) {
		return;
	}

	// Three stops: top -> middle over the upper half, middle -> bottom over
	// the lower half.
	const COLORREF stops[3] = {
		m_palette.clrBackTop,
		m_palette.clrBackMid,
		m_palette.clrBackBottom,
	};

	const int half = bands / 2;

	for (int i = 0; i < bands; ++i) {
		CRect band(full.left,
		           full.top + MulDiv(height, i, bands),
		           full.right,
		           full.top + MulDiv(height, i + 1, bands));

		CRect clipped;
		if (!clipped.IntersectRect(band, target)) {
			continue;
		}

		// Which half of the gradient this band belongs to, and how far it
		// has travelled through it.
		const int segment = (i < half) ? 0 : 1;
		const int step    = (i < half) ? i : (i - half);
		const int steps   = (segment == 0) ? half : (bands - half);

		const COLORREF from = stops[segment];
		const COLORREF to   = stops[segment + 1];

		const int r1 = GetRValue(from), r2 = GetRValue(to);
		const int g1 = GetGValue(from), g2 = GetGValue(to);
		const int b1 = GetBValue(from), b2 = GetBValue(to);

		const COLORREF clr = RGB(r1 + (r2 - r1) * step / (steps - 1),
		                         g1 + (g2 - g1) * step / (steps - 1),
		                         b1 + (b2 - b1) * step / (steps - 1));
		dc.FillSolidRect(clipped, clr);
	}
}

void CTheme::ApplyToTitleBar(HWND hWnd) const
{
	typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

	HMODULE hDwm = ::LoadLibrary(_T("dwmapi.dll"));
	if (hDwm == NULL) {
		return;  // pre-Vista, nothing to do
	}

	PFN_DwmSetWindowAttribute pfn = reinterpret_cast<PFN_DwmSetWindowAttribute>(
		::GetProcAddress(hDwm, "DwmSetWindowAttribute"));
	if (pfn != NULL) {
		BOOL bDark = IsDark() ? TRUE : FALSE;
		if (FAILED(pfn(hWnd, kDarkModeAttribute, &bDark, sizeof(bDark)))) {
			pfn(hWnd, kDarkModeAttributeLegacy, &bDark, sizeof(bDark));
		}
	}
	::FreeLibrary(hDwm);
}

void CTheme::ApplyToControl(HWND hWndControl) const
{
	if (hWndControl == NULL) {
		return;
	}

	TCHAR szClass[64] = { 0 };
	::GetClassName(hWndControl, szClass, _countof(szClass));

	// Only BUTTON (check boxes, radio buttons, group boxes) and STATIC honour
	// the WM_CTLCOLORSTATIC text colour -- and only while they are not drawn by
	// the visual style engine. Push buttons keep their native look on purpose:
	// owner-drawing them is out of scope for this change.
	const bool isButton = (_tcsicmp(szClass, _T("Button")) == 0);
	const bool isStatic = (_tcsicmp(szClass, _T("Static")) == 0);
	if (!isButton && !isStatic) {
		return;
	}

	if (isButton) {
		const LONG style = ::GetWindowLong(hWndControl, GWL_STYLE);
		const LONG type = style & BS_TYPEMASK;
		const bool isPushButton = (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON);
		if (isPushButton) {
			return;
		}
	}

	// An empty theme name detaches the control from the visual style; passing
	// NULL puts it back under the style that matches its class.
	if (IsDark()) {
		::SetWindowTheme(hWndControl, L"", L"");
	}
	else {
		::SetWindowTheme(hWndControl, NULL, NULL);
	}
	::InvalidateRect(hWndControl, NULL, TRUE);
}

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
		RGB(0xD6, 0xE8, 0xF7),  // secondary     #D6E8F7
		// Light enough to stay above 4.5:1 on the lightest band of the
		// gradient (#2A4A7B), where the old #8CA4C4 only reached 3.5:1.
		RGB(0xA8, 0xBB, 0xD6),  // disabled      #A8BBD6
	};

	// Light mode is the luminous half of the same spectrum on a white canvas:
	// the background drifts from pure white down to the light outline tone,
	// surfaces stay white and the text is midnight navy.
	const ThemePalette kLightPalette = {
		RGB(0xFF, 0xFF, 0xFF),  // back top      #FFFFFF
		RGB(0xEA, 0xF4, 0xFB),  // back middle   #EAF4FB  (white -> #D6E8F7)
		RGB(0xD6, 0xE8, 0xF7),  // back bottom   #D6E8F7
		RGB(0xFF, 0xFF, 0xFF),  // surface       #FFFFFF
		RGB(0x37, 0x78, 0xB5),  // accent        #3778B5
		RGB(0x5D, 0x9C, 0xD6),  // accent muted  #5D9CD6
		RGB(0x14, 0x26, 0x3F),  // text          #14263F
		RGB(0x1D, 0x51, 0x88),  // secondary     #1D5188
		// The one tone chosen for being weaker rather than for contrast:
		// 4.65:1 on white and 3.71:1 on the lowest band of the background.
		// Disabled text is exempt from the contrast requirement, and a
		// disabled field has to read as disabled.
		RGB(0x37, 0x78, 0xB5),  // disabled      #3778B5
	};

	// High contrast: the system colours, used exactly as Windows gives them.
	ThemePalette MakeSystemPalette()
	{
		ThemePalette p;
		p.clrBackTop        = ::GetSysColor(COLOR_3DFACE);
		p.clrBackMid        = ::GetSysColor(COLOR_3DFACE);
		p.clrBackBottom     = ::GetSysColor(COLOR_3DFACE);
		p.clrSurface        = ::GetSysColor(COLOR_WINDOW);
		p.clrAccent         = ::GetSysColor(COLOR_HIGHLIGHT);
		p.clrAccentMuted    = ::GetSysColor(COLOR_HIGHLIGHT);
		p.clrText           = ::GetSysColor(COLOR_WINDOWTEXT);
		p.clrTextSecondary  = ::GetSysColor(COLOR_WINDOWTEXT);
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
	, m_bBranded(true)
	, m_brushBack(NULL)
	, m_brushSurface(NULL)
	, m_bOwnsBrushes(false)
{
	m_palette = kDarkPalette;
	Rebuild();
}

CTheme::~CTheme()
{
	// Stock objects must never be deleted, so only our own brushes are.
	if (m_bOwnsBrushes) {
		if (m_brushBack) ::DeleteObject(m_brushBack);
		if (m_brushSurface) ::DeleteObject(m_brushSurface);
	}
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
		// Deliberately unchecked: if the preference cannot be stored the
		// next start simply opens in the default theme, which is not worth
		// interrupting the user for.
		(void)::RegSetValueEx(hKey, kThemeValue, 0, REG_DWORD,
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
	// High contrast always wins over the stored preference, and it switches
	// the brand palette off rather than merely picking the lighter one.
	m_bBranded = !IsHighContrast();
	if (!m_bBranded) {
		mode = ThemeMode::Light;
	}

	m_mode = mode;
	if (!m_bBranded) {
		m_palette = MakeSystemPalette();
	}
	else {
		m_palette = (mode == ThemeMode::Dark) ? kDarkPalette : kLightPalette;
	}
	Rebuild();
}

void CTheme::Rebuild()
{
	if (m_bOwnsBrushes) {
		if (m_brushBack) {
			::DeleteObject(m_brushBack);
		}
		if (m_brushSurface) {
			::DeleteObject(m_brushSurface);
		}
	}
	m_brushBack = ::CreateSolidBrush(m_palette.clrBackTop);
	m_brushSurface = ::CreateSolidBrush(m_palette.clrSurface);

	// Returning NULL from WM_CTLCOLOR* is not allowed -- the control would
	// paint with whatever brush happens to be selected. Under GDI handle
	// exhaustion a stock brush is a poor but legal answer.
	m_bOwnsBrushes = (m_brushBack != NULL && m_brushSurface != NULL);
	if (!m_bOwnsBrushes) {
		if (m_brushBack != NULL) {
			::DeleteObject(m_brushBack);
		}
		if (m_brushSurface != NULL) {
			::DeleteObject(m_brushSurface);
		}
		// Stock objects, because creating a brush is what just failed.
		// Black under the dark theme and white under the light one keeps
		// the dialog legible and keeps grey out of the palette; they are
		// also the two stock brushes closest to what they replace.
		const int stock = IsDark() ? BLACK_BRUSH : WHITE_BRUSH;
		m_brushBack = static_cast<HBRUSH>(::GetStockObject(stock));
		m_brushSurface = static_cast<HBRUSH>(::GetStockObject(stock));
	}
}

void CTheme::PaintBackground(CDC& dc, const CRect& rect) const
{
	PaintBackgroundSlice(dc, rect, rect);
}

void CTheme::PaintBackgroundSlice(CDC& dc, const CRect& full, const CRect& target) const
{
	if (!IsBranded()) {
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

		// steps is 64 for both halves as long as bands stays even and above
		// two; the guard keeps a future edit of 'bands' from dividing by zero.
		const int span = (steps > 1) ? (steps - 1) : 1;

		const COLORREF from = stops[segment];
		const COLORREF to   = stops[segment + 1];

		const int r1 = GetRValue(from), r2 = GetRValue(to);
		const int g1 = GetGValue(from), g2 = GetGValue(to);
		const int b1 = GetBValue(from), b2 = GetBValue(to);

		const COLORREF clr = RGB(r1 + (r2 - r1) * step / span,
		                         g1 + (g2 - g1) * step / span,
		                         b1 + (b2 - b1) * step / span);
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

void CTheme::ApplyToToolTip(HWND hWndToolTip) const
{
	if (hWndToolTip == NULL) {
		return;
	}

	// TTM_SETTIPBKCOLOR and TTM_SETTIPTEXTCOLOR are ignored while the tooltip
	// is drawn by the visual style engine, which is why the control has to be
	// detached first. Unlike the dialog's own controls a tooltip loses nothing
	// by that: it is a rectangle with one run of text.
	if (IsBranded()) {
		::SetWindowTheme(hWndToolTip, L"", L"");
		::SendMessage(hWndToolTip, TTM_SETTIPBKCOLOR,
		              static_cast<WPARAM>(m_palette.clrSurface), 0);
		::SendMessage(hWndToolTip, TTM_SETTIPTEXTCOLOR,
		              static_cast<WPARAM>(m_palette.clrText), 0);
	}
	else {
		::SetWindowTheme(hWndToolTip, NULL, NULL);
		::SendMessage(hWndToolTip, TTM_SETTIPBKCOLOR,
		              static_cast<WPARAM>(::GetSysColor(COLOR_INFOBK)), 0);
		::SendMessage(hWndToolTip, TTM_SETTIPTEXTCOLOR,
		              static_cast<WPARAM>(::GetSysColor(COLOR_INFOTEXT)), 0);
	}
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
	// the visual style engine. Push buttons are deliberately left attached to
	// the style: an unthemed button sends no NM_CUSTOMDRAW, and that
	// notification is what CFilterKeysSetterDlg uses to paint them.
	const bool isButton = (_tcsicmp(szClass, _T("Button")) == 0);
	const bool isStatic = (_tcsicmp(szClass, _T("Static")) == 0);
	// Edit controls are detached as well. A themed edit paints its own
	// background from the visual style and ignores the brush returned by
	// WM_CTLCOLOREDIT, which left white boxes sitting in a dark dialog.
	const bool isEdit   = (_tcsicmp(szClass, _T("Edit")) == 0);
	if (!isButton && !isStatic && !isEdit) {
		return;
	}

	if (isButton) {
		const LONG style = ::GetWindowLong(hWndControl, GWL_STYLE);
		const LONG type = style & BS_TYPEMASK;
		const bool isPushButton = (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON);
		// Group boxes stay with the style for the same reason push buttons
		// do: only a themed BUTTON sends NM_CUSTOMDRAW, and that is where
		// the frame and its caption are drawn. Detached, the classic frame
		// runs straight through the caption text.
		const bool isGroupBox = (type == BS_GROUPBOX);
		// Check boxes and radio buttons are drawn by the dialog as well,
		// for the same reason: detached they fall back to the classic
		// white box with a black tick.
		const bool isCheck = (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX
		                   || type == BS_3STATE || type == BS_AUTO3STATE);
		const bool isRadio = (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON);
		if (isPushButton || isGroupBox || isCheck || isRadio) {
			return;
		}
	}

	// An empty theme name detaches the control from the visual style; passing
	// NULL puts it back under the style that matches its class. Both brand
	// themes detach: in light mode a themed edit would still paint its own
	// disabled background in the system grey, which is the one colour the
	// palette has no place for.
	if (IsBranded()) {
		::SetWindowTheme(hWndControl, L"", L"");
	}
	else {
		::SetWindowTheme(hWndControl, NULL, NULL);
	}

	if (isEdit) {
		SetEditFrame(hWndControl, IsBranded());
	}

	::InvalidateRect(hWndControl, NULL, TRUE);
}

void CTheme::SetEditFrame(HWND hWndEdit, bool bFlat)
{
	// A branded edit has no frame of its own. The sunken client edge is
	// drawn by the non-client code in the system 3D colours, which no brush
	// and no WM_CTLCOLOREDIT can reach -- a grey trench around a white or
	// navy field. Removing it leaves a flat rectangle, and the dialog draws
	// the one pixel outline behind it (see PaintEditFrames).
	const LONG_PTR exStyle = ::GetWindowLongPtr(hWndEdit, GWL_EXSTYLE);
	const LONG_PTR style   = ::GetWindowLongPtr(hWndEdit, GWL_STYLE);

	const LONG_PTR wantEx = bFlat ? (exStyle & ~WS_EX_CLIENTEDGE)
	                              : (exStyle | WS_EX_CLIENTEDGE);
	const LONG_PTR want   = bFlat ? (style & ~WS_BORDER)
	                              : style;

	if (wantEx == exStyle && want == style) {
		return;
	}

	// The client area has to come out of this the same size and in the same
	// place, or the text inside the field moves: a single line edit draws
	// its text against the top of its client rectangle, so two pixels of
	// frame turning into two pixels of client area would drop the baseline.
	// Measure the client rectangle in screen coordinates first.
	RECT rcClient = { 0, 0, 0, 0 };
	::GetClientRect(hWndEdit, &rcClient);
	::MapWindowPoints(hWndEdit, NULL, reinterpret_cast<LPPOINT>(&rcClient), 2);

	::SetWindowLongPtr(hWndEdit, GWL_EXSTYLE, wantEx);
	::SetWindowLongPtr(hWndEdit, GWL_STYLE, want);

	// Which window rectangle holds that client rectangle under the new
	// styles. AdjustWindowRectEx knows what each frame costs, so this does
	// not hard-code the two pixels of a client edge.
	RECT rcWindow = rcClient;
	if (!::AdjustWindowRectEx(&rcWindow,
	                          static_cast<DWORD>(want) & ~static_cast<DWORD>(WS_CHILD),
	                          FALSE, static_cast<DWORD>(wantEx))) {
		rcWindow = rcClient;
	}
	HWND hWndParent = ::GetParent(hWndEdit);
	if (hWndParent != NULL) {
		::MapWindowPoints(NULL, hWndParent, reinterpret_cast<LPPOINT>(&rcWindow), 2);
	}

	// SWP_FRAMECHANGED is what makes the window recalculate its non-client
	// area at all; without it the frame would still be there.
	::SetWindowPos(hWndEdit, NULL, rcWindow.left, rcWindow.top,
	               rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
	               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	// Without the client edge the text would start hard against the
	// outline, so give it back the two pixels the edge used to provide.
	const LRESULT margin = bFlat ? MAKELPARAM(2, 2) : MAKELPARAM(0, 0);
	::SendMessage(hWndEdit, EM_SETMARGINS,
	              EC_LEFTMARGIN | EC_RIGHTMARGIN, margin);
}

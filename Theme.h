// Theme.h : colour palette and theming helpers for the FilterKeys Setter dialog.
//
// The palette is derived from the application icon: a navy background that
// deepens towards the bottom in three steps, blue input surfaces and a light
// blue accent.

#pragma once

enum class ThemeMode
{
	Light = 0,
	Dark  = 1,
};

struct ThemePalette
{
	COLORREF clrBackTop;        // top of the dialog background gradient
	COLORREF clrBackMid;        // middle stop of that gradient
	COLORREF clrBackBottom;     // bottom of the dialog background gradient
	COLORREF clrSurface;        // edit controls and other input surfaces
	COLORREF clrAccent;         // slider thumb, focus, highlights
	COLORREF clrAccentMuted;    // quieter accent: slider channel, outlines
	COLORREF clrText;           // primary text
	COLORREF clrTextSecondary;  // secondary / computed text
	COLORREF clrTextDisabled;   // greyed-out controls
};

// Owns the brushes for the active theme. One instance lives in the dialog.
class CTheme
{
public:
	CTheme();
	~CTheme();

	// Reads the persisted choice from the registry (defaults to Dark).
	static ThemeMode LoadPreference();
	static void SavePreference(ThemeMode mode);

	// True while Windows runs one of the high contrast schemes. The custom
	// palette must then step aside: those colours are a deliberate
	// accessibility choice by the user and overriding them is harmful.
	static bool IsHighContrast();

	void SetMode(ThemeMode mode);
	ThemeMode GetMode() const { return m_mode; }
	bool IsDark() const { return m_mode == ThemeMode::Dark; }

	const ThemePalette& Palette() const { return m_palette; }

	HBRUSH BackBrush() const { return m_brushBack; }
	HBRUSH SurfaceBrush() const { return m_brushSurface; }

	// Paints the vertical background gradient into the whole client area.
	void PaintBackground(CDC& dc, const CRect& rect) const;

	// Paints only the part of that gradient covered by 'target'. Both rects
	// share one coordinate space, which lets a child control reproduce the
	// exact slice of the parent gradient sitting behind it.
	void PaintBackgroundSlice(CDC& dc, const CRect& full, const CRect& target) const;

	// Switches the window frame between the light and dark title bar.
	// Silently does nothing on Windows versions that do not support it.
	void ApplyToTitleBar(HWND hWnd) const;

	// In dark mode the common controls are detached from the visual style so
	// that WM_CTLCOLOR* actually governs their text colour; in light mode the
	// native theme is restored.
	void ApplyToControl(HWND hWndControl) const;

private:
	void Rebuild();

	ThemeMode m_mode;
	ThemePalette m_palette;
	HBRUSH m_brushBack;
	HBRUSH m_brushSurface;
};

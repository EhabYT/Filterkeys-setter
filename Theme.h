// Theme.h : colour palette and theming helpers for the FilterKeys Setter dialog.
//
// Both palettes come from the same brand spectrum, the one the application
// icon is built from: luminous sky blue (#5D9CD6) through the base blue
// (#3778B5) and the deep blue (#1D5188) down to midnight navy (#14263F),
// with #D6E8F7 as the light outline tone.
//
// Dark mode runs the deep half of that spectrum: a navy background that
// deepens towards the bottom in three steps, blue input surfaces, a sky blue
// accent. Light mode runs the luminous half on a white canvas: white to
// #D6E8F7 behind, white surfaces, midnight navy text.
//
// Windows high contrast is the exception. There the system colours are used
// unchanged and every painter stands down, because those colours are a
// deliberate accessibility choice by the user.

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

	// The class owns two GDI brushes, so a copy would hand the same two
	// handles to a second destructor. Nothing copies a CTheme today; this
	// makes sure a later refactor cannot start doing it by accident.
	CTheme(const CTheme&) = delete;
	CTheme& operator=(const CTheme&) = delete;

	// The theme to open in: this program's own stored choice if there is
	// one, otherwise whatever the user chose for Windows, otherwise Dark.
	static ThemeMode LoadPreference();

	// What Windows itself is set to -- Settings -> Personalisation ->
	// Colours -> "Choose your default app mode". True (dark) where the
	// setting does not exist, which is every version before Windows 10
	// 1809.
	static bool SystemPrefersDark();
	static void SavePreference(ThemeMode mode);

	// True while Windows runs one of the high contrast schemes. The custom
	// palette must then step aside: those colours are a deliberate
	// accessibility choice by the user and overriding them is harmful.
	static bool IsHighContrast();

	void SetMode(ThemeMode mode);
	ThemeMode GetMode() const { return m_mode; }
	bool IsDark() const { return m_mode == ThemeMode::Dark; }

	// True while the brand palette is in charge, which is what the painters
	// ask: both the dark and the light theme are drawn by the program, and
	// only high contrast is handed back to Windows untouched.
	bool IsBranded() const { return m_bBranded; }

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

	// Tooltips are top-level popups, so they are not reached by walking the
	// dialog's children and need to be handed over explicitly.
	void ApplyToToolTip(HWND hWndToolTip) const;

	// Adds or removes the sunken client edge of an edit control. The frame
	// is non-client and drawn in the system 3D colours, so a branded edit
	// has to do without it and let the dialog outline it instead.
	static void SetEditFrame(HWND hWndEdit, bool bFlat);

private:
	void Rebuild();

	ThemeMode m_mode;
	bool m_bBranded;
	ThemePalette m_palette;
	HBRUSH m_brushBack;
	HBRUSH m_brushSurface;
	// False when the two handles above are stock objects, which must not
	// be deleted.
	bool m_bOwnsBrushes;
};

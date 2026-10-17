// FilterKeysSetterDlg.cpp : implementation file
//

#include "pch.h"
#include "FilterKeysSetter.h"
#include "FilterKeysSetterDlg.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


namespace
{
	// Paints a push button in the dark palette. Lives here rather than in
	// either dialog so the About box cannot drift away from the main window.
	// Returns false when the caller should let the system draw the button.
	// Check boxes and radio buttons detached from the visual style fall back
	// to the classic look: a white box with a black tick, which is exactly
	// what a dark dialog should not contain. They keep the style so that
	// NM_CUSTOMDRAW arrives, and are drawn here instead.
	bool PaintThemedCheckBox(const CTheme& theme, LPNMCUSTOMDRAW pcd,
	                         const CRect& rcBehind)
	{
		if (!theme.IsBranded() || pcd == NULL || pcd->dwDrawStage != CDDS_PREPAINT) {
			return false;
		}

		CDC* pDC = CDC::FromHandle(pcd->hdc);
		CWnd* pBox = CWnd::FromHandle(pcd->hdr.hwndFrom);
		if (pDC == NULL || pBox == NULL) {
			return false;
		}

		const ThemePalette& pal = theme.Palette();
		const LONG style = ::GetWindowLong(pcd->hdr.hwndFrom, GWL_STYLE);
		const LONG type = style & BS_TYPEMASK;
		const bool isRadio = (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON);

		const bool enabled = (pcd->uItemState & CDIS_DISABLED) == 0;
		const bool focused = (pcd->uItemState & CDIS_FOCUS) != 0;
		const bool hot     = (pcd->uItemState & CDIS_HOT) != 0;
		const bool pressed = (pcd->uItemState & CDIS_SELECTED) != 0;
		const LRESULT check = pBox->SendMessage(BM_GETCHECK);
		const bool checked = (check == BST_CHECKED);
		const bool mixed   = (check == BST_INDETERMINATE);

		CRect rc(pcd->rc);
		theme.PaintBackgroundSlice(*pDC, rcBehind, rc);

		// The font is selected first, because both the size of the indicator
		// and the line it sits on depend on the font and the DPI -- not on
		// the height of the control, which grows with the number of text
		// lines. Deriving them from rc.Height() made the two-line radio draw
		// a 23 px box instead of 13 px, centred between its two lines rather
		// than on the first one.
		CFont* pFont = pBox->GetFont();
		CFont* pOldFont = (pFont != NULL) ? pDC->SelectObject(pFont) : NULL;

		TEXTMETRIC tm = { 0 };
		pDC->GetTextMetrics(&tm);
		const int line = tm.tmHeight + tm.tmExternalLeading;

		// A 13x13 pixel indicator is what Windows uses at 96 dpi.
		const int dpi = ::GetDeviceCaps(pDC->GetSafeHdc(), LOGPIXELSY);
		int side = ::MulDiv(13, dpi, 96);
		if (side < 9) side = 9;
		if (side > line) side = line;

		// Placed on the first text line, the way the native control does it.
		const int boxTop = rc.top + max(0, (line - side) / 2);
		CRect rcBox(rc.left, boxTop, rc.left + side, boxTop + side);

		// Pressed is darker than resting, hot is lighter: the same three
		// steps the push buttons use, so a click reads the same way
		// everywhere in the dialog.
		const COLORREF clrFace   = pressed ? pal.clrBackBottom
		                         : hot     ? pal.clrAccentMuted
		                                   : pal.clrSurface;
		const COLORREF clrBorder = (focused || hot) ? pal.clrAccent : pal.clrAccentMuted;
		const COLORREF clrMark   = enabled ? pal.clrText : pal.clrTextDisabled;

		if (isRadio) {
			CBrush brFace;
			CPen penBorder;
			if (brFace.CreateSolidBrush(clrFace) && penBorder.CreatePen(PS_SOLID, 1, clrBorder)) {
				CBrush* pOldBrush = pDC->SelectObject(&brFace);
				CPen* pOldPen = pDC->SelectObject(&penBorder);
				pDC->Ellipse(rcBox);
				if (checked || mixed) {
					CRect rcDot(rcBox);
					rcDot.DeflateRect(side / 4, side / 4);
					CBrush brMark;
					if (brMark.CreateSolidBrush(mixed ? pal.clrTextDisabled
					                                  : clrMark)) {
						CBrush* pPrev = pDC->SelectObject(&brMark);
						pDC->Ellipse(rcDot);
						pDC->SelectObject(pPrev);
					}
				}
				pDC->SelectObject(pOldPen);
				pDC->SelectObject(pOldBrush);
			}
		}
		else {
			pDC->FillSolidRect(rcBox, clrFace);
			pDC->Draw3dRect(rcBox, clrBorder, clrBorder);
			if (mixed) {
				// BST_INDETERMINATE: a filled square, the way the style
				// engine shows "some of the things below, not all".
				CRect rcMixed(rcBox);
				rcMixed.DeflateRect(side / 4, side / 4);
				pDC->FillSolidRect(rcMixed, pal.clrTextDisabled);
			}
			else if (checked) {
				CPen penMark;
				if (penMark.CreatePen(PS_SOLID, max(1, side / 7), clrMark)) {
					CPen* pOldPen = pDC->SelectObject(&penMark);
					// A tick, not a cross: down to the low point, up again.
					pDC->MoveTo(rcBox.left + side / 4, rcBox.top + side / 2);
					pDC->LineTo(rcBox.left + side / 2 - 1, rcBox.bottom - side / 4 - 1);
					pDC->LineTo(rcBox.right - side / 5, rcBox.top + side / 5);
					pDC->SelectObject(pOldPen);
				}
			}
		}

		CString strText;
		pBox->GetWindowText(strText);

		const int oldMode = pDC->SetBkMode(TRANSPARENT);
		const COLORREF oldText = pDC->SetTextColor(enabled ? pal.clrText
		                                                   : pal.clrTextDisabled);
		CRect rcText(rc);
		// The same DPI as the indicator, not the control height: a two-line
		// caption used to push its text twice as far right as a one-line one.
		rcText.left = rcBox.right + ::MulDiv(5, dpi, 96);

		// Same UI state rule as the push buttons: no underline until the
		// user has asked for one with Alt or the keyboard.
		UINT format = DT_LEFT | DT_WORDBREAK;
		const LRESULT uiState = ::SendMessage(pcd->hdr.hwndFrom,
			WM_QUERYUISTATE, 0, 0);
		if ((uiState & UISF_HIDEACCEL) != 0) {
			format |= DT_HIDEPREFIX;
		}

		// Single-line captions are centred; wrapped ones start at the top,
		// which is what the resource layout expects.
		//
		// The question "does this fit on one line" is about width, not
		// height, so it is measured without DT_WORDBREAK. Measuring with it
		// wraps first and then reports the wrapped height, which fits by
		// construction -- so the old height test never failed, and the two
		// multiline radio captions were drawn with DT_SINGLELINE and no
		// ellipsis, cut off mid-word at the edge of the control.
		CRect rcOne(rcText);
		pDC->DrawText(strText, rcOne,
		              (format | DT_CALCRECT | DT_SINGLELINE) & ~DT_WORDBREAK);
		if (rcOne.right <= rcText.right) {
			format |= DT_VCENTER | DT_SINGLELINE;
			format &= ~DT_WORDBREAK;
		}

		CRect rcMeasure(rcText);
		pDC->DrawText(strText, rcMeasure, format | DT_CALCRECT);
		pDC->DrawText(strText, rcText, format);

		if (focused) {
			CRect rcFocus(rcText);
			rcFocus.right = min(rcText.left + rcMeasure.Width() + 2, rc.right);
			rcFocus.bottom = min(rcText.top + rcMeasure.Height(), rc.bottom);
			pDC->DrawFocusRect(rcFocus);
		}

		if (pOldFont != NULL) {
			pDC->SelectObject(pOldFont);
		}
		pDC->SetTextColor(oldText);
		pDC->SetBkMode(oldMode);
		return true;
	}

	// The slice of the dialog gradient sitting behind a child control, in that
	// control's own coordinates. The dialog and the group boxes both need it.
	CRect BackgroundBehindChild(HWND hWndChild)
	{
		CWnd* pChild = CWnd::FromHandle(hWndChild);
		if (pChild == NULL) {
			return CRect(0, 0, 0, 0);
		}

		CWnd* pParent = pChild->GetParent();
		if (pParent == NULL) {
			return CRect(0, 0, 0, 0);
		}

		CRect rcClient;
		pParent->GetClientRect(&rcClient);

		CRect rcChild;
		pChild->GetWindowRect(&rcChild);
		pParent->ScreenToClient(&rcChild);

		CRect behind(rcClient);
		behind.OffsetRect(-rcChild.left, -rcChild.top);
		return behind;
	}

	// A classic (unthemed) group box draws its frame straight across the
	// caption, which is what put a line through "Load settings" and
	// "Test area". Drawing it here keeps the caption legible and lets the
	// frame use the accent colour instead of the 3D edge colours.
	//
	// Unlike the other painters in this file this one is not reached through
	// NM_CUSTOMDRAW: comctl32 sends no such notification for a group box, so
	// the control's own window procedure calls it. See CThemedGroupBox.
	bool PaintThemedGroupBox(const CTheme& theme, CDC& dc, CWnd& box,
	                         const CRect& rc, const CRect& rcBehind)
	{
		if (!theme.IsBranded()) {
			return false;
		}

		const ThemePalette& pal = theme.Palette();

		// Nothing has erased the background: WM_CTLCOLORSTATIC handed this
		// control a hollow brush.
		theme.PaintBackgroundSlice(dc, rcBehind, rc);

		CString strText;
		box.GetWindowText(strText);

		const int oldMode = dc.SetBkMode(TRANSPARENT);
		const COLORREF oldText = dc.SetTextColor(pal.clrText);
		CFont* pFont = box.GetFont();
		CFont* pOldFont = (pFont != NULL) ? dc.SelectObject(pFont) : NULL;

		const CSize szText = dc.GetTextExtent(strText);
		const int kIndent = 8;          // where the caption starts
		const int kPadding = 3;         // clear space either side of it

		// The frame starts halfway down the caption, as Windows draws it.
		CRect rcFrame(rc);
		rcFrame.top += szText.cy / 2;
		rcFrame.DeflateRect(0, 0, 1, 1);

		const COLORREF clrEdge = pal.clrAccentMuted;
		// Four separate lines rather than Draw3dRect, because the top one
		// has to stop at the caption and start again after it.
		dc.FillSolidRect(rcFrame.left, rcFrame.top, 1, rcFrame.Height(), clrEdge);
		dc.FillSolidRect(rcFrame.right, rcFrame.top, 1, rcFrame.Height(), clrEdge);
		dc.FillSolidRect(rcFrame.left, rcFrame.bottom, rcFrame.Width() + 1, 1, clrEdge);

		const int gapLeft  = rc.left + kIndent - kPadding;
		const int gapRight = min(gapLeft + szText.cx + 2 * kPadding, rcFrame.right);
		dc.FillSolidRect(rcFrame.left, rcFrame.top,
		                 max(gapLeft - rcFrame.left, 0), 1, clrEdge);
		dc.FillSolidRect(gapRight, rcFrame.top,
		                 max(rcFrame.right - gapRight, 0), 1, clrEdge);

		dc.TextOut(rc.left + kIndent, rc.top, strText);

		if (pOldFont != NULL) {
			dc.SelectObject(pOldFont);
		}
		dc.SetTextColor(oldText);
		dc.SetBkMode(oldMode);
		return true;
	}

	bool PaintThemedPushButton(const CTheme& theme, LPNMCUSTOMDRAW pcd)
	{
		// Both brand themes are painted here; only high contrast keeps the
		// native button, because there the system colours are the point.
		if (!theme.IsBranded() || pcd == NULL || pcd->dwDrawStage != CDDS_PREPAINT) {
			return false;
		}

		CDC* pDC = CDC::FromHandle(pcd->hdc);
		CWnd* pButton = CWnd::FromHandle(pcd->hdr.hwndFrom);
		if (pDC == NULL || pButton == NULL) {
			return false;
		}

		const ThemePalette& pal = theme.Palette();
		const bool enabled = (pcd->uItemState & CDIS_DISABLED) == 0;
		const bool pressed = (pcd->uItemState & CDIS_SELECTED) != 0;
		const bool hot     = (pcd->uItemState & CDIS_HOT) != 0;
		const bool focused = (pcd->uItemState & CDIS_FOCUS) != 0;

		const LONG style = ::GetWindowLong(pcd->hdr.hwndFrom, GWL_STYLE);
		const bool isDefault = (style & BS_DEFPUSHBUTTON) == BS_DEFPUSHBUTTON;

		// Pressed is the darkest state, hover the brightest; both keep white
		// text above 4.5:1.
		COLORREF clrFace = pal.clrSurface;
		if (!enabled) {
			clrFace = pal.clrBackMid;
		}
		else if (pressed) {
			clrFace = pal.clrBackBottom;
		}
		else if (hot) {
			clrFace = pal.clrAccentMuted;
		}

		CRect rc(pcd->rc);
		pDC->FillSolidRect(rc, clrFace);

		const COLORREF clrEdge = (isDefault || focused) ? pal.clrAccent : pal.clrAccentMuted;
		pDC->Draw3dRect(rc, clrEdge, clrEdge);
		if (isDefault) {
			CRect rcInner(rc);
			rcInner.DeflateRect(1, 1);
			pDC->Draw3dRect(rcInner, clrEdge, clrEdge);
		}

		CString strText;
		pButton->GetWindowText(strText);

		const int oldMode = pDC->SetBkMode(TRANSPARENT);
		const COLORREF oldText = pDC->SetTextColor(enabled ? pal.clrText : pal.clrTextDisabled);
		CFont* pFont = pButton->GetFont();
		CFont* pOldFont = (pFont != NULL) ? pDC->SelectObject(pFont) : NULL;

		CRect rcText(rc);
		if (pressed) {
			rcText.OffsetRect(1, 1);   // the usual nudge on click
		}

		// Windows hides the accelerator underline until the user presses Alt
		// or navigates with the keyboard, and tells every window about it
		// through the UI state. DrawText underlines an & unconditionally, so
		// without this the custom-drawn buttons would be the only controls in
		// the dialog showing "Appl&y" underlined from the start.
		UINT uFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
		const LRESULT uiState = ::SendMessage(pcd->hdr.hwndFrom,
			WM_QUERYUISTATE, 0, 0);
		if ((uiState & UISF_HIDEACCEL) != 0) {
			uFormat |= DT_HIDEPREFIX;
		}
		pDC->DrawText(strText, rcText, uFormat);

		if (pOldFont != NULL) {
			pDC->SelectObject(pOldFont);
		}
		pDC->SetTextColor(oldText);
		pDC->SetBkMode(oldMode);

		if (focused && enabled) {
			CRect rcFocus(rc);
			rcFocus.DeflateRect(3, 3);
			pDC->DrawFocusRect(rcFocus);
		}

		return true;
	}
}

// The five group boxes are the one kind of button in this dialog that
// comctl32 paints entirely on its own: it sends no NM_CUSTOMDRAW for a
// BS_GROUPBOX, so there is no notification to answer the way the push
// buttons, check boxes and radio buttons answer it. The frame and the caption
// are drawn from the control's own window procedure instead.
BEGIN_MESSAGE_MAP(CThemedGroupBox, CButton)
	ON_WM_PAINT()
	ON_MESSAGE(WM_PRINTCLIENT, OnPrintClient)
END_MESSAGE_MAP()

// A group box contains the other controls of its group, so its rectangle
// overlaps theirs, and this one paints its whole rectangle: without
// WS_CLIPSIBLINGS the fill would land on top of the neighbouring controls
// whenever the dialog happened to repaint the frame after them, leaving the
// group's contents blank. With it Windows clips the frame out of their
// rectangles, exactly as it does for the system's own group box.
void CThemedGroupBox::PreSubclassWindow()
{
	ModifyStyle(0, WS_CLIPSIBLINGS);
	CButton::PreSubclassWindow();
}

void CThemedGroupBox::PaintBranded(CDC& dc)
{
	CRect rc;
	GetClientRect(&rc);
	PaintThemedGroupBox(*m_pTheme, dc, *this, rc,
	                    BackgroundBehindChild(GetSafeHwnd()));
}

void CThemedGroupBox::OnPaint()
{
	if (m_pTheme == NULL || !m_pTheme->IsBranded()) {
		// High contrast: the system colours are the point there, and the
		// control draws itself exactly as it did before this subclass
		// existed.
		Default();
		return;
	}

	CPaintDC dc(this);
	PaintBranded(dc);
}

// A capture through PrintWindow takes the same path as the screen, so a
// screenshot of the dialog cannot show a frame the user never sees.
LRESULT CThemedGroupBox::OnPrintClient(WPARAM wParam, LPARAM /*lParam*/)
{
	if (m_pTheme == NULL || !m_pTheme->IsBranded()) {
		return Default();
	}

	CDC* pDC = CDC::FromHandle(reinterpret_cast<HDC>(wParam));
	if (pDC == NULL) {
		return 0;
	}

	PaintBranded(*pDC);
	return 0;
}

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	// Implementation
protected:
	virtual BOOL OnInitDialog();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnCustomDrawButton(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

	CTheme m_theme;
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	m_theme.SetMode(CTheme::LoadPreference());
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_NOTIFY(NM_CUSTOMDRAW, IDOK, OnCustomDrawButton)
END_MESSAGE_MAP()

void CAboutDlg::OnCustomDrawButton(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = PaintThemedPushButton(m_theme, pcd) ? CDRF_SKIPDEFAULT : CDRF_DODEFAULT;
}

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_theme.ApplyToTitleBar(GetSafeHwnd());
	for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL;
	     pChild = pChild->GetWindow(GW_HWNDNEXT)) {
		m_theme.ApplyToControl(pChild->GetSafeHwnd());
	}
	return TRUE;
}

BOOL CAboutDlg::OnEraseBkgnd(CDC* pDC)
{
	if (!m_theme.IsBranded()) {
		return CDialog::OnEraseBkgnd(pDC);
	}
	CRect rect;
	GetClientRect(&rect);
	m_theme.PaintBackground(*pDC, rect);
	return TRUE;
}

HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (!m_theme.IsBranded()) {
		return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}
	if (nCtlColor == CTLCOLOR_STATIC || nCtlColor == CTLCOLOR_BTN) {
		pDC->SetTextColor(m_theme.Palette().clrText);
		pDC->SetBkMode(TRANSPARENT);
		return (HBRUSH)::GetStockObject(HOLLOW_BRUSH);
	}
	if (nCtlColor == CTLCOLOR_DLG) {
		return m_theme.BackBrush();
	}
	return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}


// CFilterKeysSetterDlg dialog

CFilterKeysSetterDlg::CFilterKeysSetterDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFilterKeysSetterDlg::IDD, pParent)
	, m_nMode(0)
	, m_nWait(1000)
	, m_nDelay(1000)
	, m_nRepeat(500)
	, m_nBounce(0)
	, m_bOn(FALSE)
	, m_bAvailable(FALSE)
	, m_bIndicator(FALSE)
	, m_bClick(FALSE)
	, m_bHotKeyActive(FALSE)
	, m_bConfirmHotKey(FALSE)
	, m_bHotKeySound(FALSE)
	, m_bUpdateIniFile(FALSE)
	, m_bSendChange(FALSE)
	, m_bSyncingSlider(false)
	, m_bHaveOriginal(false)
{
	ZeroMemory(&m_fkOriginal, sizeof(m_fkOriginal));
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_theme.SetMode(CTheme::LoadPreference());
}

void CFilterKeysSetterDlg::ErrorBox(const TCHAR* msg)
{
	MessageBox(msg, _T("FilterKeys Setter"), MB_ICONERROR);
}

// DDX_Text with an int calls CDataExchange::Fail() when the box is empty or
// holds something that is not a number, and Fail() throws. That is right for
// Apply and OK, where a bad field should stop the save, and wrong everywhere
// else: UpdateControls() runs the same transfer on every check box click, so
// a single cleared field used to abort the flags, the enable state and all
// three read-outs with "Please enter an integer".
//
// This reads the same four boxes without throwing. An unreadable box keeps
// the value the member already holds -- which is the last value the user
// actually confirmed, and is in range by construction. The 20000 ms ceiling
// is still enforced in SaveSettings(), where the error can be about the
// field the user is looking at.
static void TolerantInt(CDataExchange* pDX, int nIDC, int& value)
{
	if (!pDX->m_bSaveAndValidate) {
		// Loading: nothing can fail, so the stock behaviour is fine.
		pDX->m_pDlgWnd->SetDlgItemInt(nIDC, value, TRUE);
		return;
	}

	BOOL bTranslated = FALSE;
	const int nRead = pDX->m_pDlgWnd->GetDlgItemInt(nIDC, &bTranslated, TRUE);
	if (bTranslated) {
		value = nRead;
	}
}

void CFilterKeysSetterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	// The control bindings come first, and that order is load-bearing.
	// DDX_Text writes the value with SetDlgItemInt, which sends WM_SETTEXT
	// and therefore EN_CHANGE synchronously -- before DDX_Control has
	// subclassed anything if it runs the other way round. OnEnChangeRepeatEdit
	// then read m_editRepeat with a NULL handle, which Release shrugged off
	// and Debug asserted on, at start-up.
	DDX_Control(pDX, IDC_WAIT_EDIT, m_editWait);
	DDX_Control(pDX, IDC_DELAY_EDIT, m_editDelay);
	DDX_Control(pDX, IDC_REPEAT_EDIT, m_editRepeat);
	DDX_Control(pDX, IDC_BOUNCE_EDIT, m_editBounce);
	DDX_Control(pDX, IDC_TEST_EDIT, m_editTest);

	// Read tolerantly. DDX_Text with an int calls CDataExchange::Fail() when
	// the box is empty or not a number, which reports "Please enter an
	// integer" and throws -- aborting the whole transfer. That is the right
	// answer for Apply and OK, but UpdateControls() runs it on every check
	// box click, so one cleared field used to stop the flags, the enable
	// state and the read-outs from updating at all. An unreadable box now
	// keeps the value it already had; the range is still checked in
	// SaveSettings(), which is where a range belongs.
	TolerantInt(pDX, IDC_WAIT_EDIT, m_nWait);
	TolerantInt(pDX, IDC_DELAY_EDIT, m_nDelay);
	TolerantInt(pDX, IDC_REPEAT_EDIT, m_nRepeat);
	TolerantInt(pDX, IDC_BOUNCE_EDIT, m_nBounce);

	DDX_Check(pDX, IDC_ON, m_bOn);
	DDX_Check(pDX, IDC_AVAILABLE, m_bAvailable);
	DDX_Check(pDX, IDC_HOTKEYACTIVE, m_bHotKeyActive);
	DDX_Check(pDX, IDC_CONFIRMHOTKEY, m_bConfirmHotKey);
	DDX_Check(pDX, IDC_HOTKEYSOUND, m_bHotKeySound);
	DDX_Check(pDX, IDC_INDICATOR, m_bIndicator);
	DDX_Check(pDX, IDC_CLICK, m_bClick);

	DDX_Radio(pDX, IDC_IGNORE_QUICK, m_nMode);

	DDX_Check(pDX, IDC_UPDATEINIFILE, m_bUpdateIniFile);
	DDX_Check(pDX, IDC_SENDCHANGE, m_bSendChange);

	DDX_Control(pDX, IDC_FLAGVAL, m_staticFlagVal);
	DDX_Control(pDX, IDC_CHARS_PER_SEC, m_staticCharsPerSec);
	DDX_Control(pDX, IDC_STATUS, m_staticStatus);

	DDX_Control(pDX, IDC_DELAY_SLIDER, m_sliderDelay);
	DDX_Control(pDX, IDC_REPEAT_SLIDER, m_sliderRepeat);
	DDX_Control(pDX, IDC_DARKTHEME, m_chkDarkTheme);

	// The five group boxes paint themselves; they need the window object to
	// do it, not a value.
	DDX_Control(pDX, IDC_GRP_SETTINGS, m_grpSettings);
	DDX_Control(pDX, IDC_GRP_FLAGS, m_grpFlags);
	DDX_Control(pDX, IDC_GRP_APPLIED, m_grpApplied);
	DDX_Control(pDX, IDC_GRP_LOAD, m_grpLoad);
	DDX_Control(pDX, IDC_GRP_TEST, m_grpTest);
}

BEGIN_MESSAGE_MAP(CFilterKeysSetterDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_IGNORE_QUICK, OnBnClickedIgnoreQuick)
	ON_BN_CLICKED(IDC_IGNORE_REPEATED, OnBnClickedIgnoreRepeated)
	ON_BN_CLICKED(IDC_ON, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_AVAILABLE, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_INDICATOR, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_CLICK, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_HOTKEYACTIVE, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_CONFIRMHOTKEY, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_HOTKEYSOUND, OnBnClickedFlag)
	ON_BN_CLICKED(IDC_SET_DEFAULTS, OnBnClickedSetDefaults)
	ON_EN_CHANGE(IDC_REPEAT_EDIT, OnEnChangeRepeatEdit)
	ON_BN_CLICKED(IDC_SET_NORMAL, OnBnClickedSetNormal)
	ON_BN_CLICKED(IDC_SET_REGISTRY, OnBnClickedSetRegistry)
	ON_BN_CLICKED(IDC_SET_CURRENT, OnBnClickedSetCurrent)
	ON_BN_CLICKED(IDC_APPLY, OnBnClickedApply)
	ON_BN_CLICKED(IDC_SET_ORIGINAL, OnBnClickedSetOriginal)
	ON_BN_CLICKED(IDC_DARKTHEME, OnBnClickedDarkTheme)
	ON_EN_CHANGE(IDC_DELAY_EDIT, OnEnChangeDelayEdit)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_HSCROLL()
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_DELAY_SLIDER, OnCustomDrawSlider)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_REPEAT_SLIDER, OnCustomDrawSlider)
	// Push buttons keep their native look in light mode; in dark mode they
	// are drawn here, because a themed button ignores WM_CTLCOLORBTN.
	ON_NOTIFY(NM_CUSTOMDRAW, IDOK, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDCANCEL, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_APPLY, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SET_CURRENT, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SET_REGISTRY, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SET_NORMAL, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SET_DEFAULTS, OnCustomDrawButton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SET_ORIGINAL, OnCustomDrawButton)
	// Check boxes and radio buttons: classic ones are a white box with a
	// black tick, which does not belong in a dark dialog.
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_ON, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_AVAILABLE, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_INDICATOR, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_CLICK, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_HOTKEYACTIVE, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_CONFIRMHOTKEY, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_HOTKEYSOUND, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_UPDATEINIFILE, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SENDCHANGE, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_DARKTHEME, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_IGNORE_QUICK, OnCustomDrawCheckBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_IGNORE_REPEATED, OnCustomDrawCheckBox)
	ON_WM_SETTINGCHANGE()
END_MESSAGE_MAP()


// CFilterKeysSetterDlg message handlers

BOOL CFilterKeysSetterDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Stash current FilterKeys settings...
	ZeroMemory(&m_fkOriginal, sizeof(m_fkOriginal));
	m_fkOriginal.cbSize = sizeof(FILTERKEYS);
	m_bHaveOriginal = !!SystemParametersInfo(SPI_GETFILTERKEYS,
		sizeof(FILTERKEYS), &m_fkOriginal, 0);
	if (!m_bHaveOriginal) {
		// Without that read, "Original" would offer to restore a struct of
		// zeroes -- FilterKeys off, every timing 0 -- while claiming to put
		// things back the way they were.
		ZeroMemory(&m_fkOriginal, sizeof(m_fkOriginal));
		m_fkOriginal.cbSize = sizeof(FILTERKEYS);
		CWnd* pOriginal = GetDlgItem(IDC_SET_ORIGINAL);
		if (pOriginal != NULL) {
			pOriginal->EnableWindow(FALSE);
		}
	}

	// Sliders have to exist before the first SetValues() call feeds them.
	InitSliders();
	InitToolTips();

	// A group box is the one button comctl32 sends no NM_CUSTOMDRAW for, so
	// the five of them are painted from their own window procedure and only
	// need to be pointed at the palette. The address is stable: the dialog
	// rebuilds this object on a theme switch instead of replacing it.
	CThemedGroupBox* const boxes[] = { &m_grpSettings, &m_grpFlags,
	                                   &m_grpApplied, &m_grpLoad, &m_grpTest };
	for (size_t i = 0; i < _countof(boxes); ++i) {
		boxes[i]->SetTheme(&m_theme);
	}

	// Every other input in this dialog has a label that precedes it in
	// z-order, from which a screen reader takes the accessible name. The test
	// field is the exception: the only text near it is a group box caption,
	// and a group box is not a label. A cue banner is the standard answer --
	// it names the control for accessibility and tells a sighted user what
	// the empty box is for, in one line.
	//
	// Sent as the message rather than through CEdit::SetCueBannerText, which
	// this MFC does not carry. The second argument is BOOL show when focused.
	m_editTest.SendMessage(EM_SETCUEBANNER, TRUE,
	                       reinterpret_cast<LPARAM>(
	                           _T("Type here to try the new repeat rate")));

	RefreshThemeFromSystem();

	// Init from current FilterKeys settings...
	OnBnClickedSetCurrent();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// Slider ranges are the practically useful part of the 0..20000 ms the API
// accepts; the edit box stays authoritative for values outside that window.
void CFilterKeysSetterDlg::InitSliders()
{
	m_sliderDelay.SetRange(0, 2000, TRUE);
	m_sliderDelay.SetPageSize(100);
	m_sliderDelay.SetLineSize(10);

	m_sliderRepeat.SetRange(10, 1000, TRUE);
	m_sliderRepeat.SetPageSize(50);
	m_sliderRepeat.SetLineSize(5);

	// A trackbar has no visible caption, so its window text is free to carry
	// the accessible name that screen readers announce.
	m_sliderDelay.SetWindowText(_T("Repeat delay in milliseconds"));
	m_sliderRepeat.SetWindowText(_T("Repeat rate in milliseconds"));
}

namespace
{
	struct ToolTipEntry
	{
		UINT nID;
		const TCHAR* pszText;
	};

	// FilterKeys exposes four timings whose names say little on their own.
	const ToolTipEntry kToolTips[] = {
		{ IDC_IGNORE_QUICK,
		  _T("Ignore keystrokes that are not held down long enough, and take over the key repeat timings.") },
		{ IDC_IGNORE_REPEATED,
		  _T("Suppress a second press of the same key that follows too quickly. Useful against bouncing key contacts.") },
		{ IDC_WAIT_EDIT,
		  _T("How long a key must be held before it registers at all. 0 accepts every keystroke immediately.") },
		{ IDC_DELAY_EDIT,
		  _T("How long a key is held before it starts repeating.") },
		{ IDC_REPEAT_EDIT,
		  _T("Milliseconds between two repeats. Smaller is faster: 500 ms is two characters per second, 25 ms is forty.") },
		{ IDC_BOUNCE_EDIT,
		  _T("Repeats of the same key arriving faster than this are discarded.") },
		{ IDC_DELAY_SLIDER, _T("Repeat delay in milliseconds.") },
		{ IDC_REPEAT_SLIDER, _T("Repeat rate in milliseconds. Drag left for a faster repeat.") },
		{ IDC_ON, _T("FKF_FILTERKEYSON - FilterKeys is active.") },
		{ IDC_AVAILABLE, _T("FKF_AVAILABLE - FilterKeys may be switched on at all.") },
		{ IDC_HOTKEYACTIVE, _T("FKF_HOTKEYACTIVE - holding the right Shift key for eight seconds toggles FilterKeys.") },
		{ IDC_CONFIRMHOTKEY, _T("FKF_CONFIRMHOTKEY - ask for confirmation before the shortcut takes effect.") },
		{ IDC_HOTKEYSOUND, _T("FKF_HOTKEYSOUND - play a sound when the shortcut switches FilterKeys.") },
		{ IDC_INDICATOR, _T("FKF_INDICATOR - show the FilterKeys icon in the notification area.") },
		{ IDC_CLICK, _T("FKF_CLICKON - click on every accepted keystroke.") },
		{ IDC_UPDATEINIFILE,
		  _T("Write the settings to the registry so they survive a sign-out. Without this they only apply to the current session.") },
		{ IDC_SENDCHANGE,
		  _T("Broadcast WM_SETTINGCHANGE so running programs pick the new settings up.") },
		{ IDC_SET_CURRENT, _T("Load the settings Windows is using right now.") },
		{ IDC_SET_REGISTRY, _T("Load the settings stored in the registry.") },
		{ IDC_SET_NORMAL, _T("Load the repeat timings from the standard Windows keyboard settings.") },
		{ IDC_SET_DEFAULTS, _T("Load the Windows default FilterKeys values.") },
		{ IDC_SET_ORIGINAL, _T("Restore the settings that were active when this program started.") },
		{ IDC_TEST_EDIT, _T("Type here to try the timings out. Click Apply first.") },
		// The three read-outs. They are statics, so they carry SS_NOTIFY in the
		// resource; without it the mouse never reaches them and no tip appears.
		{ IDC_CHARS_PER_SEC,
		  _T("The repeat rate expressed as characters per second, the way it is usually quoted.") },
		{ IDC_FLAGVAL,
		  _T("The seven check boxes above as one number: the dwFlags value Windows stores for FilterKeys.") },
		{ IDC_STATUS,
		  _T("What Windows reports right now, which is not necessarily what this dialog shows until you press Apply.") },
		{ IDC_APPLY, _T("Apply the settings without closing the window.") },
		{ IDC_DARKTHEME, _T("Switch between the dark and the light appearance. Until you do, the Windows app mode decides.") },
	};
}

void CFilterKeysSetterDlg::InitToolTips()
{
	if (!m_toolTip.Create(this, TTS_ALWAYSTIP)) {
		return;
	}

	for (size_t i = 0; i < _countof(kToolTips); ++i) {
		CWnd* pCtrl = GetDlgItem(kToolTips[i].nID);
		if (pCtrl != NULL) {
			m_toolTip.AddTool(pCtrl, kToolTips[i].pszText);
		}
	}

	// Without a width limit the longer explanations become one endless line.
	m_toolTip.SetMaxTipWidth(300);
	m_toolTip.SetDelayTime(TTDT_AUTOPOP, 15000);
	m_toolTip.Activate(TRUE);
	m_theme.ApplyToToolTip(m_toolTip.GetSafeHwnd());
}

BOOL CFilterKeysSetterDlg::PreTranslateMessage(MSG* pMsg)
{
	if (::IsWindow(m_toolTip.GetSafeHwnd())) {
		m_toolTip.RelayEvent(pMsg);
	}
	return CDialog::PreTranslateMessage(pMsg);
}

void CFilterKeysSetterDlg::SyncSliderFromEdit(CSliderCtrl& slider, CEdit& edit)
{
	if (m_bSyncingSlider
	    || !::IsWindow(slider.GetSafeHwnd()) || !::IsWindow(edit.GetSafeHwnd())) {
		return;
	}
	CString s;
	edit.GetWindowText(s);
	int value = _wtoi(s);

	int lower = 0, upper = 0;
	slider.GetRange(lower, upper);
	if (value < lower) value = lower;
	if (value > upper) value = upper;

	m_bSyncingSlider = true;
	slider.SetPos(value);
	m_bSyncingSlider = false;
}

void CFilterKeysSetterDlg::SyncEditFromSlider(CSliderCtrl& slider, CEdit& edit)
{
	if (m_bSyncingSlider
	    || !::IsWindow(slider.GetSafeHwnd()) || !::IsWindow(edit.GetSafeHwnd())) {
		return;
	}
	CString s;
	s.Format(_T("%d"), slider.GetPos());

	m_bSyncingSlider = true;
	edit.SetWindowText(s);
	m_bSyncingSlider = false;
}

void CFilterKeysSetterDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	const HWND hScrolled = (pScrollBar != NULL) ? pScrollBar->GetSafeHwnd() : NULL;

	if (hScrolled != NULL && hScrolled == m_sliderDelay.GetSafeHwnd()) {
		SyncEditFromSlider(m_sliderDelay, m_editDelay);
	}
	else if (hScrolled != NULL && hScrolled == m_sliderRepeat.GetSafeHwnd()) {
		SyncEditFromSlider(m_sliderRepeat, m_editRepeat);
		UpdateCharsPerSec();
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CFilterKeysSetterDlg::OnEnChangeDelayEdit()
{
	SyncSliderFromEdit(m_sliderDelay, m_editDelay);
}

void CFilterKeysSetterDlg::OnBnClickedDarkTheme()
{
	const ThemeMode mode = (m_chkDarkTheme.GetCheck() == BST_CHECKED)
		? ThemeMode::Dark : ThemeMode::Light;
	m_theme.SetMode(mode);
	CTheme::SavePreference(mode);
	ApplyTheme();
}

// Reconciles the stored preference with what the system currently demands.
void CFilterKeysSetterDlg::RefreshThemeFromSystem()
{
	const bool bHighContrast = CTheme::IsHighContrast();

	// SetMode() forces Light while high contrast is on, so the stored
	// preference can simply be handed over unchanged. Re-reading it here
	// is also what makes the window follow Windows: WM_SETTINGCHANGE
	// arrives with "ImmersiveColorSet" when the app mode is switched, and
	// LoadPreference() answers with the system's choice for as long as
	// this program has none of its own.
	m_theme.SetMode(CTheme::LoadPreference());

	if (::IsWindow(m_chkDarkTheme.GetSafeHwnd())) {
		m_chkDarkTheme.SetCheck(m_theme.IsDark() ? BST_CHECKED : BST_UNCHECKED);
		// Offering a theme switch that cannot take effect would be misleading.
		m_chkDarkTheme.EnableWindow(bHighContrast ? FALSE : TRUE);
	}

	ApplyTheme();
}

// Fires when the user turns high contrast on or off while the dialog is open.
void CFilterKeysSetterDlg::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CDialog::OnSettingChange(uFlags, lpszSection);
	RefreshThemeFromSystem();
	UpdateStatus();
}

void CFilterKeysSetterDlg::ApplyTheme()
{
	m_theme.ApplyToTitleBar(GetSafeHwnd());
	ApplyThemeToChildren();
	m_theme.ApplyToToolTip(m_toolTip.GetSafeHwnd());
	Invalidate(TRUE);
	UpdateWindow();
}

void CFilterKeysSetterDlg::ApplyThemeToChildren()
{
	// Every child is invalidated, not only the ones ApplyToControl() acted
	// on. ApplyToControl() returns early for the controls this dialog paints
	// itself -- push buttons, group boxes, check boxes, radio buttons -- and
	// for the two trackbars, because they stay attached to the visual style:
	// it is the notification, or in the group box's case the control itself,
	// that the painter hangs off. None of them is invalidated on the way out.
	// And invalidating the dialog alone does not repaint its children: their
	// pixels are not part of the parent's update region. Without this the two
	// themes swapped the background, the edits and the statics at once and
	// left the rest in the old palette until something else happened to
	// redraw it.
	for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL;
	     pChild = pChild->GetWindow(GW_HWNDNEXT)) {
		m_theme.ApplyToControl(pChild->GetSafeHwnd());
		::InvalidateRect(pChild->GetSafeHwnd(), NULL, TRUE);
	}
}

// The trackbars are the one place where the accent really carries the visual
// identity, so they are drawn by hand in both brand themes.
void CFilterKeysSetterDlg::OnCustomDrawSlider(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;

	if (!m_theme.IsBranded() || pcd == NULL) {
		return;
	}

	CDC* pDC = CDC::FromHandle(pcd->hdc);
	if (pDC == NULL) {
		return;
	}

	const ThemePalette& pal = m_theme.Palette();

	switch (pcd->dwDrawStage) {
	case CDDS_PREPAINT:
	{
		// The slice of the dialog gradient that sits behind this control.
		// The trackbar also fills the band from the brush WM_CTLCOLORSTATIC
		// hands out -- see the trackbar branch in OnCtlColor -- and it does
		// that after this notification has been answered, so on this
		// platform that fill is the one that shows; both colours come out of
		// the same gradient and differ by at most one step.
		CWnd* pSlider = CWnd::FromHandle(pcd->hdr.hwndFrom);
		if (pSlider != NULL) {
			CRect rcClient;
			GetClientRect(&rcClient);

			CRect rcSlider;
			pSlider->GetWindowRect(&rcSlider);
			ScreenToClient(&rcSlider);

			CRect full(rcClient);
			full.OffsetRect(-rcSlider.left, -rcSlider.top);

			m_theme.PaintBackgroundSlice(*pDC, full, CRect(pcd->rc));
		}
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}

	case CDDS_ITEMPREPAINT:
	{
		// CDIS_DISABLED never arrives for a trackbar's items -- measured on a
		// control that was disabled, repainted and disabled again -- so the
		// control's own state is what tells a disabled slider from an enabled
		// one. Left to CDIS_DISABLED the thumb kept its enabled colour in
		// both themes, which is not what the palette promises.
		const bool enabled = ::IsWindowEnabled(pcd->hdr.hwndFrom) != FALSE;
		CRect rc(pcd->rc);

		if (pcd->dwItemSpec == TBCD_CHANNEL) {
			// A slim recessed track.
			rc.DeflateRect(0, (rc.Height() > 4) ? (rc.Height() - 4) / 2 : 0);
			pDC->FillSolidRect(rc, pal.clrSurface);
			pDC->Draw3dRect(rc, pal.clrAccentMuted, pal.clrAccentMuted);
			*pResult = CDRF_SKIPDEFAULT;
			return;
		}

		if (pcd->dwItemSpec == TBCD_THUMB) {
			// Disabled: the fill takes the disabled tone, and the outline
			// drops from the text colour to the muted accent, the colour this
			// dialog draws every quiet outline in. The second half is what
			// makes the state visible in the light theme, where the disabled
			// tone and the accent are the same #3778B5.
			const COLORREF clrThumb     = enabled ? pal.clrAccent : pal.clrTextDisabled;
			const COLORREF clrThumbEdge = enabled ? pal.clrText : pal.clrAccentMuted;
			pDC->FillSolidRect(rc, clrThumb);
			pDC->Draw3dRect(rc, clrThumbEdge, clrThumbEdge);
			*pResult = CDRF_SKIPDEFAULT;
			return;
		}

		if (pcd->dwItemSpec == TBCD_TICS) {
			*pResult = CDRF_SKIPDEFAULT;
			return;
		}
		return;
	}

	default:
		return;
	}
}

void CFilterKeysSetterDlg::OnCustomDrawCheckBox(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	if (pcd == NULL) {
		return;
	}
	if (PaintThemedCheckBox(m_theme, pcd,
	                        BackgroundBehindChild(pcd->hdr.hwndFrom))) {
		*pResult = CDRF_SKIPDEFAULT;
	}
}

void CFilterKeysSetterDlg::OnCustomDrawButton(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = PaintThemedPushButton(m_theme, pcd) ? CDRF_SKIPDEFAULT : CDRF_DODEFAULT;
}

BOOL CFilterKeysSetterDlg::OnEraseBkgnd(CDC* pDC)
{
	if (!m_theme.IsBranded()) {
		return CDialog::OnEraseBkgnd(pDC);
	}
	CRect rect;
	GetClientRect(&rect);
	m_theme.PaintBackground(*pDC, rect);
	PaintEditFrames(*pDC);
	return TRUE;
}

// The five edit controls have no frame of their own any more (see
// CTheme::SetEditFrame), so the dialog draws it for them, one pixel in the
// muted accent, in the gap the client edge used to occupy. Deliberately not
// focus-dependent: the parent background is not repainted when focus moves
// between two fields, so a focus colour here would go stale. The caret and
// the selection show focus, as they do in every other edit box in Windows.
void CFilterKeysSetterDlg::PaintEditFrames(CDC& dc)
{
	static const UINT kEdits[] = { IDC_WAIT_EDIT, IDC_DELAY_EDIT,
	                               IDC_REPEAT_EDIT, IDC_BOUNCE_EDIT,
	                               IDC_TEST_EDIT };

	const COLORREF clrFrame = m_theme.Palette().clrAccentMuted;
	for (size_t i = 0; i < _countof(kEdits); ++i) {
		CWnd* pEdit = GetDlgItem(kEdits[i]);
		if (pEdit == NULL || !::IsWindow(pEdit->GetSafeHwnd())
		    || !pEdit->IsWindowVisible()) {
			continue;
		}
		CRect rc;
		pEdit->GetWindowRect(&rc);
		ScreenToClient(&rc);
		rc.InflateRect(1, 1);
		dc.Draw3dRect(rc, clrFrame, clrFrame);
	}
}

HBRUSH CFilterKeysSetterDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (!m_theme.IsBranded()) {
		return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}

	const ThemePalette& pal = m_theme.Palette();
	const int id = (pWnd != NULL) ? pWnd->GetDlgCtrlID() : 0;

	// A disabled edit box reports itself as CTLCOLOR_STATIC, so it has to be
	// recognised by id rather than by message.
	const bool isEditBox = (id == IDC_WAIT_EDIT || id == IDC_DELAY_EDIT
	                     || id == IDC_REPEAT_EDIT || id == IDC_BOUNCE_EDIT
	                     || id == IDC_TEST_EDIT);

	// A trackbar erases its own background with the brush this message hands
	// back, and it never touches the pixels around its channel and thumb
	// otherwise -- an unthemed trackbar draws no background of its own. So it
	// gets the exact colour the gradient has behind it, and the control then
	// blends into the dialog instead of showing the stale grey band it was
	// created with.
	const bool isTrackbar = (id == IDC_DELAY_SLIDER || id == IDC_REPEAT_SLIDER);
	if (isTrackbar && pWnd != NULL) {
		CRect rcControl;
		pWnd->GetWindowRect(&rcControl);
		ScreenToClient(&rcControl);

		CRect rcClient;
		GetClientRect(&rcClient);

		const COLORREF clrBehind =
			m_theme.BackgroundColorAt(rcControl.CenterPoint().y, rcClient.Height());
		pDC->SetBkColor(clrBehind);
		return m_theme.SolidBrush(clrBehind);
	}

	if (nCtlColor == CTLCOLOR_EDIT || isEditBox) {
		// Read-only is the state that means "out of play" for these fields
		// (see UpdateControls): a box the mode switch has taken over keeps
		// the disabled tone, but it is still enabled, because only that way
		// does the colour set here reach the screen.
		const bool live = (pWnd != NULL) && pWnd->IsWindowEnabled()
		               && (pWnd->GetStyle() & ES_READONLY) == 0;
		pDC->SetBkColor(pal.clrSurface);
		pDC->SetTextColor(live ? pal.clrText : pal.clrTextDisabled);
		return m_theme.SurfaceBrush();
	}

	if (nCtlColor == CTLCOLOR_STATIC || nCtlColor == CTLCOLOR_BTN) {
		// Secondary colour for the two computed read-only readouts.
		const bool secondary = (id == IDC_CHARS_PER_SEC || id == IDC_FLAGVAL
		                     || id == IDC_STATUS);
		pDC->SetTextColor(secondary ? pal.clrTextSecondary : pal.clrText);
		pDC->SetBkMode(TRANSPARENT);
		return (HBRUSH)::GetStockObject(HOLLOW_BRUSH);
	}

	if (nCtlColor == CTLCOLOR_DLG) {
		return m_theme.BackBrush();
	}

	return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CFilterKeysSetterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFilterKeysSetterDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFilterKeysSetterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CFilterKeysSetterDlg::OnOK()
{
	if (!UpdateData(TRUE)) {
		return;
	}
	if (!SaveSettings()) {
		return;
	}
	CDialog::OnOK();
}

void CFilterKeysSetterDlg::OnBnClickedIgnoreQuick()
{
	UpdateControls();
}

void CFilterKeysSetterDlg::OnBnClickedIgnoreRepeated()
{
	UpdateControls();
}

void CFilterKeysSetterDlg::OnBnClickedFlag()
{
	UpdateControls();
}

void CFilterKeysSetterDlg::SetFlags(DWORD dwFlags)
{
	m_bOn = !!(dwFlags & FKF_FILTERKEYSON);
	m_bAvailable = !!(dwFlags & FKF_AVAILABLE);
	m_bIndicator = !!(dwFlags & FKF_INDICATOR);
	m_bClick = !!(dwFlags & FKF_CLICKON);
	m_bHotKeyActive = !!(dwFlags & FKF_HOTKEYACTIVE);
	m_bConfirmHotKey = !!(dwFlags & FKF_CONFIRMHOTKEY);
	m_bHotKeySound = !!(dwFlags & FKF_HOTKEYSOUND);
}

DWORD CFilterKeysSetterDlg::GetFlagsVal()
{
	DWORD dwFlags = 0;
	if (m_bOn) dwFlags |= FKF_FILTERKEYSON;
	if (m_bAvailable) dwFlags |= FKF_AVAILABLE;
	if (m_bIndicator) dwFlags |= FKF_INDICATOR;
	if (m_bClick) dwFlags |= FKF_CLICKON;
	if (m_bHotKeyActive) dwFlags |= FKF_HOTKEYACTIVE;
	if (m_bConfirmHotKey) dwFlags |= FKF_CONFIRMHOTKEY;
	if (m_bHotKeySound) dwFlags |= FKF_HOTKEYSOUND;
	return dwFlags;
}

void CFilterKeysSetterDlg::UpdateFlagVal()
{
	DWORD dwFlags = GetFlagsVal();
	CString s;
	// The raw number is what the registry stores, so keep it visible -- but
	// show the hex form too, which is how the FKF_* constants are documented.
	s.Format(_T("Flags: %u (0x%02X)"), dwFlags, dwFlags);
	SetReadout(m_staticFlagVal, s);
}

// 122 and 59 were written as plain numbers; spelling them out costs nothing
// and says which switches they actually stand for.
namespace
{
	// Everything except FilterKeys itself and the right-Shift shortcut.
	const DWORD kDefaultFlags = FKF_AVAILABLE | FKF_CONFIRMHOTKEY
		| FKF_HOTKEYSOUND | FKF_INDICATOR | FKF_CLICKON;            // 122

	// The same, with FilterKeys on and the key click off.
	const DWORD kKeyboardFlags = FKF_FILTERKEYSON | FKF_AVAILABLE
		| FKF_CONFIRMHOTKEY | FKF_HOTKEYSOUND | FKF_INDICATOR;      // 59
}

void CFilterKeysSetterDlg::OnBnClickedSetDefaults()
{
	FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
	filter_keys.dwFlags = kDefaultFlags;
	filter_keys.iWaitMSec = 1000;
	filter_keys.iDelayMSec = 1000;
	filter_keys.iRepeatMSec = 500;
	filter_keys.iBounceMSec = 0;
	SetValues(filter_keys);
}

void CFilterKeysSetterDlg::OnEnChangeRepeatEdit()
{
	UpdateCharsPerSec();
}

void CFilterKeysSetterDlg::OnBnClickedSetNormal()
{
	// Both reads were unchecked. On failure the variables stay 0, which is a
	// perfectly plausible pair of values -- 500 ms repeat, 250 ms delay --
	// so the dialog would have presented an invention as "the Windows
	// keyboard settings".
	int speed = 0;
	int delay = 0;
	if (!SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &speed, 0)
	    || !SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) {
		ErrorBox(_T("The Windows keyboard settings could not be read."));
		return;
	}

	// The control panel slider runs 0..31 over roughly 2..30 characters per
	// second; the delay setting is 0..3 in steps of 250 ms starting at 250.
	double chars_per_sec = speed * (30.0 - 2.0) / 31.0 + 2.0;
	int repeat_ms = (int)floor(1000.0 / chars_per_sec + 0.5);
	int delay_ms = delay * 250 + 250;

	FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
	filter_keys.dwFlags = kKeyboardFlags;
	filter_keys.iDelayMSec = delay_ms;
	filter_keys.iRepeatMSec = repeat_ms;
	SetValues(filter_keys);
}

// RegQueryValueEx does not promise that a string value is null terminated, and
// it happily hands back whatever type is actually stored. Both have to be dealt
// with before the data is used.

// Reads a REG_SZ value and always leaves szValue null terminated, falling back
// to strDefaultValue on any kind of failure.
static LONG GetStringRegKey(HKEY hKey, const WCHAR* strValueName, WCHAR* szValue,
                            DWORD cbValue, const WCHAR* strDefaultValue)
{
	const size_t sizeInWords = cbValue / sizeof(WCHAR);
	if (szValue == NULL || sizeInWords == 0) {
		return ERROR_INVALID_PARAMETER;
	}

	// Leave room to append a terminator the registry may not have stored.
	DWORD dwType = 0;
	DWORD dwBufferSize = static_cast<DWORD>((sizeInWords - 1) * sizeof(WCHAR));

	LONG nError = ::RegQueryValueExW(hKey, strValueName, 0, &dwType,
	                                 reinterpret_cast<LPBYTE>(szValue), &dwBufferSize);

	if (nError == ERROR_SUCCESS && dwType != REG_SZ && dwType != REG_EXPAND_SZ) {
		nError = ERROR_INVALID_DATA;
	}

	if (nError == ERROR_SUCCESS) {
		// dwBufferSize is in bytes and may or may not include a terminator.
		const size_t written = dwBufferSize / sizeof(WCHAR);
		szValue[(written < sizeInWords) ? written : (sizeInWords - 1)] = L'\0';
		return ERROR_SUCCESS;
	}

	wcsncpy_s(szValue, sizeInWords, strDefaultValue, _TRUNCATE);
	return nError;
}

static LONG GetNumericStringRegKey(HKEY hKey, const WCHAR* strValueName, INT& nValue, INT nDefaultValue)
{
	nValue = nDefaultValue;

	WCHAR szBuffer[64] = { 0 };
	LONG nError = GetStringRegKey(hKey, strValueName, szBuffer, sizeof(szBuffer), L"");
	if (nError == ERROR_SUCCESS) {
		nValue = _wtoi(szBuffer);
	}
	return nError;
}

static LONG GetNumericStringRegKey(HKEY hKey, const WCHAR* strValueName, DWORD& nValue, DWORD nDefaultValue)
{
	INT iValue = 0;
	LONG nError = GetNumericStringRegKey(hKey, strValueName, iValue,
	                                     static_cast<INT>(nDefaultValue));
	nValue = static_cast<DWORD>(iValue);
	return nError;
}

void CFilterKeysSetterDlg::OnBnClickedSetRegistry()
{
	HKEY hKey;
	LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Accessibility\\Keyboard Response", 0, KEY_READ, &hKey);
	if (lRes == ERROR_SUCCESS) {
		FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
		GetNumericStringRegKey(hKey, L"DelayBeforeAcceptance", filter_keys.iWaitMSec, 1000);
		GetNumericStringRegKey(hKey, L"AutoRepeatDelay", filter_keys.iDelayMSec, 1000);
		GetNumericStringRegKey(hKey, L"AutoRepeatRate", filter_keys.iRepeatMSec, 500);
		GetNumericStringRegKey(hKey, L"BounceTime", filter_keys.iBounceMSec, 0);
		GetNumericStringRegKey(hKey, L"Flags", filter_keys.dwFlags, kDefaultFlags);
		RegCloseKey(hKey);
		SetValues(filter_keys);
	}
	else {
		OnBnClickedSetDefaults();
	}
}

void CFilterKeysSetterDlg::OnBnClickedSetCurrent()
{
	FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
	BOOL ok = SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &filter_keys, 0);
	if (!ok) {
		// filter_keys holds nothing but its own cbSize at this point, so
		// loading it would quietly replace the dialog with zeroes.
		ErrorBox(_T("Failed to fetch current settings."));
		// Returning here used to leave the three read-outs showing the raw
		// text from the resource ("status", "flagval", "cps"). They are not
		// derived from filter_keys -- the flags come from the check boxes and
		// the status line re-reads the system itself -- so they can still be
		// filled in, and an unreadable system is worth reporting rather than
		// hiding behind a placeholder.
		UpdateFlagVal();
		UpdateCharsPerSec();
		UpdateStatus();
		return;
	}
	SetValues(filter_keys);
}

void CFilterKeysSetterDlg::SetValues(const FILTERKEYS& filter_keys)
{
	UpdateData(TRUE);
	m_nWait = filter_keys.iWaitMSec;
	m_nDelay = filter_keys.iDelayMSec;
	m_nRepeat = filter_keys.iRepeatMSec;
	m_nBounce = filter_keys.iBounceMSec;
	m_nMode = m_nBounce ? 1 : 0;
	SetFlags(filter_keys.dwFlags);
	UpdateData(FALSE);
	UpdateControls();
}

void CFilterKeysSetterDlg::UpdateControls()
{
	UpdateData(TRUE);
	UpdateFlagVal();
	UpdateCharsPerSec();

	const BOOL bRepeatMode = (m_nMode == 0) ? TRUE : FALSE;

	// The four numeric fields are made read-only rather than disabled. A
	// disabled edit box draws its own text in the system's COLOR_GRAYTEXT
	// whatever the parent sets in WM_CTLCOLORSTATIC -- measured on this
	// dialog -- which puts a grey that is barely legible on the dark surface
	// into a palette that has no greys. A read-only box keeps the colour
	// OnCtlColor hands it. The two sliders keep the real disabled state:
	// a trackbar draws no text, and its thumb changes colour with it.
	m_editWait.SetReadOnly(!bRepeatMode);
	m_editDelay.SetReadOnly(!bRepeatMode);
	m_editRepeat.SetReadOnly(!bRepeatMode);
	m_editBounce.SetReadOnly(bRepeatMode);

	m_sliderDelay.EnableWindow(bRepeatMode);
	m_sliderRepeat.EnableWindow(bRepeatMode);

	SyncSliderFromEdit(m_sliderDelay, m_editDelay);
	SyncSliderFromEdit(m_sliderRepeat, m_editRepeat);

	UpdateStatus();
}

// Shows what Windows is doing right now, which is not necessarily what the
// dialog has in its edit boxes.
void CFilterKeysSetterDlg::UpdateStatus()
{
	if (!::IsWindow(m_staticStatus.GetSafeHwnd())) {
		return;
	}

	FILTERKEYS fk = { sizeof(FILTERKEYS) };
	CString s;

	if (SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0)) {
		if (fk.dwFlags & FKF_FILTERKEYSON) {
			if (fk.iBounceMSec) {
				s.Format(_T("FilterKeys on: bounce time %d ms"), fk.iBounceMSec);
			}
			else {
				// Kept short so it fits the status line without ellipsis even
				// at four digits per value.
				s.Format(_T("FilterKeys on: ignore %d / delay %d / repeat %d ms"),
				         fk.iWaitMSec, fk.iDelayMSec, fk.iRepeatMSec);
			}
		}
		else {
			s = _T("FilterKeys is off");
		}
	}
	else {
		s = _T("Current FilterKeys state could not be read");
	}

	SetReadout(m_staticStatus, s);
}

// A static control is not in the tab order, so a screen reader passes it
// by and never reports that its text changed -- which is precisely what
// these three labels exist to say. EVENT_OBJECT_NAMECHANGE is the
// notification for "this object is still here, its name is different now",
// and a static's name is its text.
//
// The comparison in front of it is not an optimisation afterthought: the
// characters-per-second label is rewritten on every keystroke, and
// announcing an unchanged value, or repainting it, is worse than useless.
void CFilterKeysSetterDlg::SetReadout(CWnd& label, const CString& strText)
{
	if (!::IsWindow(label.GetSafeHwnd())) {
		return;
	}

	CString strCurrent;
	label.GetWindowText(strCurrent);
	if (strCurrent == strText) {
		return;
	}

	label.SetWindowText(strText);

	// These labels answer WM_CTLCOLORSTATIC with a hollow brush: they paint
	// their text and nothing else, and the gradient behind them belongs to
	// the dialog. That works until the value changes. Then the control
	// repaints its own window alone, the digits of the value before it are
	// still on screen, and the new ones are drawn on top of them -- measured
	// on the screen, which is the only place it shows: a PrintWindow capture
	// hides it, because rendering the dialog repaints the gradient under the
	// labels as a side effect. So the parent is asked to paint that slice of
	// the gradient again, in the same pass, and the label draws its text over
	// it.
	CWnd* pParent = label.GetParent();
	if (pParent != NULL) {
		// In the parent's coordinates. The label's own client rectangle would
		// land in the parent's top left corner and repaint nothing that
		// matters.
		CRect rc;
		label.GetWindowRect(&rc);
		pParent->ScreenToClient(&rc);
		pParent->RedrawWindow(&rc, NULL,
		                      RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW
		                      | RDW_ALLCHILDREN);
	}

	::NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, label.GetSafeHwnd(),
	                 OBJID_CLIENT, CHILDID_SELF);
}

void CFilterKeysSetterDlg::UpdateCharsPerSec()
{
	// Guarded for the same reason SetReadout() and SyncSliderFromEdit() are:
	// EN_CHANGE arrives during the initial UpdateData() and can beat the
	// control bindings, and a Debug build asserts on a NULL handle where
	// Release just reads an empty string.
	if (!::IsWindow(m_editRepeat.GetSafeHwnd())) {
		return;
	}

	CString s;
	m_editRepeat.GetWindowText(s);
	int val = _wtoi(s);
	if (val) {
		s.Format(_T("(%.1f per second)"), 1000.0 / val);
	}
	else {
		s = _T("(0 per second)");
	}
	SetReadout(m_staticCharsPerSec, s);
	SyncSliderFromEdit(m_sliderRepeat, m_editRepeat);
}

void CFilterKeysSetterDlg::OnBnClickedApply()
{
	if (!UpdateData(TRUE)) {
		return;
	}
	if (!SaveSettings()) {
		return;
	}
	// The numbers are read tolerantly (see TolerantInt), so a box the user
	// cleared still carried its previous value -- the one just applied.
	// Writing them back makes the dialog show what actually took effect
	// instead of an empty box the user has to guess about.
	UpdateData(FALSE);
	UpdateStatus();
}

bool CFilterKeysSetterDlg::SaveSettings()
{
	if (m_nMode == 0) {
		if (m_nWait > 20000) {
			ErrorBox(_T("Ignore under value is too large.\nMaximum value is 20000."));
			m_editWait.SetFocus();
			return false;
		}
		if (m_nDelay > 20000) {
			ErrorBox(_T("Repeat delay value is too large.\nMaximum value is 20000."));
			m_editDelay.SetFocus();
			return false;
		}
		if (m_nRepeat > 20000) {
			ErrorBox(_T("Repeat rate value is too large.\nMaximum value is 20000."));
			m_editRepeat.SetFocus();
			return false;
		}
	}
	else {
		if (m_nBounce > 20000) {
			ErrorBox(_T("Bounce time is too large.\nMaximum value is 20000."));
			m_editBounce.SetFocus();
			return false;
		}
	}
	FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
	if (m_nMode == 0) {
		filter_keys.iWaitMSec = m_nWait;
		filter_keys.iDelayMSec = m_nDelay;
		filter_keys.iRepeatMSec = m_nRepeat;
	}
	else {
		filter_keys.iBounceMSec = m_nBounce;
	}
	filter_keys.dwFlags = GetFlagsVal();
	UINT fWinIni = 0;
	if (m_bUpdateIniFile) fWinIni |= SPIF_UPDATEINIFILE;
	if (m_bSendChange) fWinIni |= SPIF_SENDCHANGE;
	bool ok = !!SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &filter_keys, fWinIni);
	if (!ok) {
		ErrorBox(_T("Failed to save new settings."));
	}
	return ok;
}

void CFilterKeysSetterDlg::OnBnClickedSetOriginal()
{
	// The button is disabled in that case, but a stray BN_CLICKED (an
	// accelerator arriving before OnInitDialog finishes, say) must not get
	// through either.
	if (!m_bHaveOriginal) {
		ErrorBox(_T("The settings in use at start-up could not be read."));
		return;
	}
	SetValues(m_fkOriginal);
}

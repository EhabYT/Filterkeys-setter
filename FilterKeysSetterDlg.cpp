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
		if (!theme.IsDark() || pcd == NULL || pcd->dwDrawStage != CDDS_PREPAINT) {
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
		const bool checked = (pBox->SendMessage(BM_GETCHECK) == BST_CHECKED);

		CRect rc(pcd->rc);
		theme.PaintBackgroundSlice(*pDC, rcBehind, rc);

		// A 13x13 pixel indicator is what Windows uses at 96 dpi; scale it
		// with the control height so it keeps up with higher dpi.
		int side = ::MulDiv(13, rc.Height(), 17);
		if (side < 9) side = 9;
		if (side > rc.Height()) side = rc.Height();

		CRect rcBox(rc.left, rc.top + (rc.Height() - side) / 2,
		            rc.left + side, rc.top + (rc.Height() - side) / 2 + side);

		const COLORREF clrFace   = hot ? pal.clrAccentMuted : pal.clrSurface;
		const COLORREF clrBorder = (focused || hot) ? pal.clrAccent : pal.clrAccentMuted;
		const COLORREF clrMark   = enabled ? pal.clrText : pal.clrTextDisabled;

		if (isRadio) {
			CBrush brFace;
			CPen penBorder;
			if (brFace.CreateSolidBrush(clrFace) && penBorder.CreatePen(PS_SOLID, 1, clrBorder)) {
				CBrush* pOldBrush = pDC->SelectObject(&brFace);
				CPen* pOldPen = pDC->SelectObject(&penBorder);
				pDC->Ellipse(rcBox);
				if (checked) {
					CRect rcDot(rcBox);
					rcDot.DeflateRect(side / 4, side / 4);
					CBrush brMark;
					if (brMark.CreateSolidBrush(clrMark)) {
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
			if (checked) {
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
		CFont* pFont = pBox->GetFont();
		CFont* pOldFont = (pFont != NULL) ? pDC->SelectObject(pFont) : NULL;

		CRect rcText(rc);
		rcText.left = rcBox.right + ::MulDiv(5, rc.Height(), 17);

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
		CRect rcMeasure(rcText);
		pDC->DrawText(strText, rcMeasure, format | DT_CALCRECT);
		if (rcMeasure.Height() <= rc.Height()) {
			format |= DT_VCENTER | DT_SINGLELINE;
			format &= ~DT_WORDBREAK;
		}
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

	// A classic (unthemed) group box draws its frame straight across the
	// caption, which is what put a line through "Load settings" and
	// "Test area". Drawing it here keeps the caption legible and lets the
	// frame use the accent colour instead of the 3D edge colours.
	bool PaintThemedGroupBox(const CTheme& theme, LPNMCUSTOMDRAW pcd,
	                         const CRect& rcBehind)
	{
		if (!theme.IsDark() || pcd == NULL || pcd->dwDrawStage != CDDS_PREPAINT) {
			return false;
		}

		CDC* pDC = CDC::FromHandle(pcd->hdc);
		CWnd* pBox = CWnd::FromHandle(pcd->hdr.hwndFrom);
		if (pDC == NULL || pBox == NULL) {
			return false;
		}

		const ThemePalette& pal = theme.Palette();
		CRect rc(pcd->rc);

		// Nothing has erased the background: WM_CTLCOLORSTATIC handed this
		// control a hollow brush.
		theme.PaintBackgroundSlice(*pDC, rcBehind, rc);

		CString strText;
		pBox->GetWindowText(strText);

		const int oldMode = pDC->SetBkMode(TRANSPARENT);
		const COLORREF oldText = pDC->SetTextColor(pal.clrText);
		CFont* pFont = pBox->GetFont();
		CFont* pOldFont = (pFont != NULL) ? pDC->SelectObject(pFont) : NULL;

		const CSize szText = pDC->GetTextExtent(strText);
		const int kIndent = 8;          // where the caption starts
		const int kPadding = 3;         // clear space either side of it

		// The frame starts halfway down the caption, as Windows draws it.
		CRect rcFrame(rc);
		rcFrame.top += szText.cy / 2;
		rcFrame.DeflateRect(0, 0, 1, 1);

		const COLORREF clrEdge = pal.clrAccentMuted;
		// Four separate lines rather than Draw3dRect, because the top one
		// has to stop at the caption and start again after it.
		pDC->FillSolidRect(rcFrame.left, rcFrame.top, 1, rcFrame.Height(), clrEdge);
		pDC->FillSolidRect(rcFrame.right, rcFrame.top, 1, rcFrame.Height(), clrEdge);
		pDC->FillSolidRect(rcFrame.left, rcFrame.bottom, rcFrame.Width() + 1, 1, clrEdge);

		const int gapLeft  = rc.left + kIndent - kPadding;
		const int gapRight = min(gapLeft + szText.cx + 2 * kPadding, rcFrame.right);
		pDC->FillSolidRect(rcFrame.left, rcFrame.top,
		                   max(gapLeft - rcFrame.left, 0), 1, clrEdge);
		pDC->FillSolidRect(gapRight, rcFrame.top,
		                   max(rcFrame.right - gapRight, 0), 1, clrEdge);

		pDC->TextOut(rc.left + kIndent, rc.top, strText);

		if (pOldFont != NULL) {
			pDC->SelectObject(pOldFont);
		}
		pDC->SetTextColor(oldText);
		pDC->SetBkMode(oldMode);
		return true;
	}

	bool PaintThemedPushButton(const CTheme& theme, LPNMCUSTOMDRAW pcd)
	{
		// Light mode keeps the native button, which is exactly what it
		// should look like there.
		if (!theme.IsDark() || pcd == NULL || pcd->dwDrawStage != CDDS_PREPAINT) {
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
	if (!m_theme.IsDark()) {
		return CDialog::OnEraseBkgnd(pDC);
	}
	CRect rect;
	GetClientRect(&rect);
	m_theme.PaintBackground(*pDC, rect);
	return TRUE;
}

HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (!m_theme.IsDark()) {
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

void CFilterKeysSetterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_WAIT_EDIT, m_nWait);
	//DDV_MinMaxInt(pDX, m_nWait, 0, 20000);
	DDX_Text(pDX, IDC_DELAY_EDIT, m_nDelay);
	//DDV_MinMaxInt(pDX, m_nDelay, 0, 20000);
	DDX_Text(pDX, IDC_REPEAT_EDIT, m_nRepeat);
	//DDV_MinMaxInt(pDX, m_nRepeat, 0, 20000);
	DDX_Text(pDX, IDC_BOUNCE_EDIT, m_nBounce);
	//DDV_MinMaxInt(pDX, m_nBounce, 0, 20000);

	DDX_Check(pDX, IDC_ON, m_bOn);
	DDX_Check(pDX, IDC_AVAILABLE, m_bAvailable);
	DDX_Check(pDX, IDC_HOTKEYACTIVE, m_bHotKeyActive);
	DDX_Check(pDX, IDC_CONFIRMHOTKEY, m_bConfirmHotKey);
	DDX_Check(pDX, IDC_HOTKEYSOUND, m_bHotKeySound);
	DDX_Check(pDX, IDC_INDICATOR, m_bIndicator);
	DDX_Check(pDX, IDC_CLICK, m_bClick);

	DDX_Control(pDX, IDC_WAIT_EDIT, m_editWait);
	DDX_Control(pDX, IDC_DELAY_EDIT, m_editDelay);
	DDX_Control(pDX, IDC_REPEAT_EDIT, m_editRepeat);
	DDX_Control(pDX, IDC_BOUNCE_EDIT, m_editBounce);

	DDX_Control(pDX, IDC_IGNORE_QUICK, m_radioIgnoreQuick);
	DDX_Control(pDX, IDC_IGNORE_REPEATED, m_radioIgnoreRepeated);
	DDX_Radio(pDX, IDC_IGNORE_QUICK, m_nMode);

	DDX_Check(pDX, IDC_UPDATEINIFILE, m_bUpdateIniFile);
	DDX_Check(pDX, IDC_SENDCHANGE, m_bSendChange);

	DDX_Control(pDX, IDC_FLAGVAL, m_staticFlagVal);
	DDX_Control(pDX, IDC_CHARS_PER_SEC, m_staticCharsPerSec);
	DDX_Control(pDX, IDC_STATUS, m_staticStatus);

	DDX_Control(pDX, IDC_DELAY_SLIDER, m_sliderDelay);
	DDX_Control(pDX, IDC_REPEAT_SLIDER, m_sliderRepeat);
	DDX_Control(pDX, IDC_DARKTHEME, m_chkDarkTheme);
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
	// Group boxes are drawn here too; the classic frame crosses its caption.
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_GRP_SETTINGS, OnCustomDrawGroupBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_GRP_FLAGS, OnCustomDrawGroupBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_GRP_APPLIED, OnCustomDrawGroupBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_GRP_LOAD, OnCustomDrawGroupBox)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_GRP_TEST, OnCustomDrawGroupBox)
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
		{ IDC_DARKTHEME, _T("Switch between the dark and the native light appearance.") },
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
	// preference can simply be handed over unchanged.
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
	Invalidate(TRUE);
	UpdateWindow();
}

void CFilterKeysSetterDlg::ApplyThemeToChildren()
{
	for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL;
	     pChild = pChild->GetWindow(GW_HWNDNEXT)) {
		m_theme.ApplyToControl(pChild->GetSafeHwnd());
	}
}

// The trackbars are the one place where the cyan accent really carries the
// visual identity, so they are drawn by hand in dark mode.
void CFilterKeysSetterDlg::OnCustomDrawSlider(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;

	if (!m_theme.IsDark() || pcd == NULL) {
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
		// WM_CTLCOLORSTATIC hands the trackbar a hollow brush, so nothing has
		// erased its background. Reproduce the slice of the dialog gradient
		// that sits behind this control.
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
		const bool enabled = (pcd->uItemState & CDIS_DISABLED) == 0;
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
			const COLORREF clrThumb = enabled ? pal.clrAccent : pal.clrTextDisabled;
			pDC->FillSolidRect(rc, clrThumb);
			pDC->Draw3dRect(rc, pal.clrText, pal.clrText);
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

// The slice of the dialog gradient sitting behind a child control, in that
// control's own coordinates.
CRect CFilterKeysSetterDlg::BackgroundBehind(HWND hWndChild)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	CWnd* pChild = CWnd::FromHandle(hWndChild);
	if (pChild == NULL) {
		return rcClient;
	}

	CRect rcChild;
	pChild->GetWindowRect(&rcChild);
	ScreenToClient(&rcChild);

	CRect behind(rcClient);
	behind.OffsetRect(-rcChild.left, -rcChild.top);
	return behind;
}

void CFilterKeysSetterDlg::OnCustomDrawCheckBox(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	if (pcd == NULL) {
		return;
	}
	if (PaintThemedCheckBox(m_theme, pcd, BackgroundBehind(pcd->hdr.hwndFrom))) {
		*pResult = CDRF_SKIPDEFAULT;
	}
}

void CFilterKeysSetterDlg::OnCustomDrawGroupBox(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	if (pcd == NULL) {
		return;
	}

	if (PaintThemedGroupBox(m_theme, pcd, BackgroundBehind(pcd->hdr.hwndFrom))) {
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
	if (!m_theme.IsDark()) {
		return CDialog::OnEraseBkgnd(pDC);
	}
	CRect rect;
	GetClientRect(&rect);
	m_theme.PaintBackground(*pDC, rect);
	return TRUE;
}

HBRUSH CFilterKeysSetterDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (!m_theme.IsDark()) {
		return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}

	const ThemePalette& pal = m_theme.Palette();
	const int id = (pWnd != NULL) ? pWnd->GetDlgCtrlID() : 0;

	// A disabled edit box reports itself as CTLCOLOR_STATIC, so it has to be
	// recognised by id rather than by message.
	const bool isEditBox = (id == IDC_WAIT_EDIT || id == IDC_DELAY_EDIT
	                     || id == IDC_REPEAT_EDIT || id == IDC_BOUNCE_EDIT
	                     || id == IDC_TEST_EDIT);

	if (nCtlColor == CTLCOLOR_EDIT || isEditBox) {
		const bool enabled = (pWnd != NULL) && pWnd->IsWindowEnabled();
		pDC->SetBkColor(pal.clrSurface);
		pDC->SetTextColor(enabled ? pal.clrText : pal.clrTextDisabled);
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
	m_staticFlagVal.SetWindowText(s);
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
		ErrorBox(_T("Failed to fetch current settings"));
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

	m_editWait.EnableWindow(bRepeatMode);
	m_editDelay.EnableWindow(bRepeatMode);
	m_editRepeat.EnableWindow(bRepeatMode);
	m_editBounce.EnableWindow(!bRepeatMode);

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

	m_staticStatus.SetWindowText(s);
}

void CFilterKeysSetterDlg::UpdateCharsPerSec()
{
	CString s;
	m_editRepeat.GetWindowText(s);
	int val = _wtoi(s);
	if (val) {
		s.Format(_T("(%.1f per second)"), 1000.0 / val);
	}
	else {
		s = _T("(0 per second)");
	}
	m_staticCharsPerSec.SetWindowText(s);
	SyncSliderFromEdit(m_sliderRepeat, m_editRepeat);
}

void CFilterKeysSetterDlg::OnBnClickedApply()
{
	if (!UpdateData(TRUE)) {
		return;
	}
	SaveSettings();
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
			ErrorBox(_T("Bounce value is too large.\nMaximum value is 20000."));
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

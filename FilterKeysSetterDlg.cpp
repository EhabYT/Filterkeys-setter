// FilterKeysSetterDlg.cpp : implementation file
//

#include "pch.h"
#include "FilterKeysSetter.h"
#include "FilterKeysSetterDlg.h"
#include <math.h>
#include ".\filterkeyssetterdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


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
	BOOL ok = SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &m_fkOriginal, 0);

	// Sliders have to exist before the first SetValues() call feeds them.
	InitSliders();

	m_chkDarkTheme.SetCheck(m_theme.IsDark() ? BST_CHECKED : BST_UNCHECKED);
	ApplyTheme();

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
	if (pScrollBar == (CScrollBar*)&m_sliderDelay) {
		SyncEditFromSlider(m_sliderDelay, m_editDelay);
	}
	else if (pScrollBar == (CScrollBar*)&m_sliderRepeat) {
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
			pDC->Draw3dRect(rc, pal.clrBackTop, pal.clrBackTop);
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
	s.Format(_T("(%u)"), dwFlags);
	m_staticFlagVal.SetWindowText(s);
}

void CFilterKeysSetterDlg::OnBnClickedSetDefaults()
{
	FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
	filter_keys.dwFlags = 122;
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
	int speed = 0;
	SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &speed, 0);
	double chars_per_sec = speed * (30.0 - 2.0) / 31.0 + 2.0;
	int repeat_ms = (int)floor(1000.0 / chars_per_sec + 0.5);

	int delay = 0;
	SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0);
	int delay_ms = delay * 250 + 250;

	FILTERKEYS filter_keys = { sizeof(FILTERKEYS) };
	filter_keys.dwFlags = 59;
	filter_keys.iDelayMSec = delay_ms;
	filter_keys.iRepeatMSec = repeat_ms;
	SetValues(filter_keys);
}

static LONG GetDWORDRegKey(HKEY hKey, const WCHAR* strValueName, DWORD& nValue, DWORD nDefaultValue)
{
	nValue = nDefaultValue;
	DWORD dwBufferSize(sizeof(DWORD));
	DWORD nResult(0);
	LONG nError = ::RegQueryValueExW(hKey, strValueName, 0, NULL, (LPBYTE)&nResult, &dwBufferSize);
	if (ERROR_SUCCESS == nError) {
		nValue = nResult;
	}
	return nError;
}

static LONG GetBoolRegKey(HKEY hKey, const WCHAR* strValueName, bool& bValue, bool bDefaultValue)
{
	DWORD nDefValue((bDefaultValue) ? 1 : 0);
	DWORD nResult(nDefValue);
	LONG nError = GetDWORDRegKey(hKey, strValueName, nResult, nDefValue);
	if (ERROR_SUCCESS == nError) {
		bValue = (nResult != 0) ? true : false;
	}
	return nError;
}

static LONG GetStringRegKey(HKEY hKey, const WCHAR* strValueName, WCHAR* szValue, DWORD cbValue, const WCHAR* strDefaultValue)
{
	SIZE_T sizeInWords = cbValue / sizeof(wchar_t);
	DWORD dwBufferSize = cbValue;
	ULONG nError = ::RegQueryValueExW(hKey, strValueName, 0, NULL, (LPBYTE)szValue, &dwBufferSize);
	if (ERROR_SUCCESS != nError) {
		wcsncpy_s(szValue, sizeInWords, strDefaultValue, sizeInWords);
	}
	return nError;
}

static LONG GetNumericStringRegKey(HKEY hKey, const WCHAR* strValueName, INT& nValue, INT nDefaultValue)
{
	nValue = nDefaultValue;
	WCHAR szBuffer[512];
	DWORD dwBufferSize = sizeof(szBuffer);
	LONG nError = GetStringRegKey(hKey, strValueName, szBuffer, sizeof(szBuffer), L"");
	if (ERROR_SUCCESS == nError) {
		nValue = _wtoi(szBuffer);
	}
	return nError;
}

static LONG GetNumericStringRegKey(HKEY hKey, const WCHAR* strValueName, DWORD& nValue, DWORD nDefaultValue)
{
	INT iValue;
	INT iDefaultValue = (INT)nDefaultValue;
	LONG nError = GetNumericStringRegKey(hKey, strValueName, iValue, iDefaultValue);
	nValue = iValue;
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
		GetNumericStringRegKey(hKey, L"Flags", filter_keys.dwFlags, 122);
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
		ErrorBox(_T("Failed to fetch current settings"));
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
				s.Format(_T("Active: ignoring repeats faster than %d ms"), fk.iBounceMSec);
			}
			else {
				s.Format(_T("Active: ignore under %d ms, delay %d ms, repeat %d ms"),
				         fk.iWaitMSec, fk.iDelayMSec, fk.iRepeatMSec);
			}
		}
		else {
			s = _T("FilterKeys is currently off");
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
		s = "(0 per second)";
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
	SetValues(m_fkOriginal);
}

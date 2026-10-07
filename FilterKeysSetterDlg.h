// FilterKeysSetterDlg.h : header file
//

#pragma once
#include "afxwin.h"
#include "Theme.h"


// CFilterKeysSetterDlg dialog
class CFilterKeysSetterDlg : public CDialog
{
// Construction
public:
	CFilterKeysSetterDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_FILTERKEYSSETTER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	void SetValues(const FILTERKEYS &filter_keys);
	void SetFlags(DWORD dwFlags);
	DWORD GetFlagsVal();

	void UpdateFlagVal();
	void UpdateCharsPerSec();
	void UpdateControls();
	void UpdateStatus();

	// Theming
	void ApplyTheme();
	void ApplyThemeToChildren();
	void RefreshThemeFromSystem();

	// Slider <-> edit box synchronisation
	void InitSliders();
	void InitToolTips();
	void SyncSliderFromEdit(CSliderCtrl& slider, CEdit& edit);
	void SyncEditFromSlider(CSliderCtrl& slider, CEdit& edit);

	bool SaveSettings();

	void ErrorBox(const TCHAR* msg);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK();

	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedIgnoreQuick();
	afx_msg void OnBnClickedIgnoreRepeated();
	afx_msg void OnBnClickedFlag();
	afx_msg void OnEnChangeRepeatEdit();
	afx_msg void OnBnClickedSetCurrent();
	afx_msg void OnBnClickedSetRegistry();
	afx_msg void OnBnClickedSetNormal();
	afx_msg void OnBnClickedSetDefaults();
	afx_msg void OnBnClickedSetOriginal();
	afx_msg void OnBnClickedApply();
	afx_msg void OnBnClickedDarkTheme();
	afx_msg void OnEnChangeDelayEdit();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnCustomDrawSlider(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCustomDrawButton(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);

	DECLARE_MESSAGE_MAP()

	FILTERKEYS m_fkOriginal;
	// False when SPI_GETFILTERKEYS failed at start-up, in which case
	// m_fkOriginal is zeroes rather than the state to restore.
	bool m_bHaveOriginal;

	int m_nMode;

	int m_nWait;
	int m_nDelay;
	int m_nRepeat;
	int m_nBounce;

	BOOL m_bOn;
	BOOL m_bAvailable;
	BOOL m_bIndicator;
	BOOL m_bClick;
	BOOL m_bHotKeyActive;
	BOOL m_bConfirmHotKey;
	BOOL m_bHotKeySound;

	BOOL m_bUpdateIniFile;
	BOOL m_bSendChange;

	// Controls...

	CEdit m_editWait;
	CEdit m_editDelay;
	CEdit m_editRepeat;
	CEdit m_editBounce;

	CButton m_radioIgnoreQuick;
	CButton m_radioIgnoreRepeated;

	CStatic m_staticFlagVal;
	CStatic m_staticCharsPerSec;
	CStatic m_staticStatus;

	CSliderCtrl m_sliderDelay;
	CSliderCtrl m_sliderRepeat;

	CButton m_chkDarkTheme;

	CToolTipCtrl m_toolTip;

	CTheme m_theme;

	// Guards against the slider and the edit box updating each other in a loop.
	bool m_bSyncingSlider;

public:
};

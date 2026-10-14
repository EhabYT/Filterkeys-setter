// FilterKeysSetter.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "FilterKeysSetter.h"
#include "FilterKeysSetterDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CFilterKeysSetterApp

BEGIN_MESSAGE_MAP(CFilterKeysSetterApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


// CFilterKeysSetterApp construction

CFilterKeysSetterApp::CFilterKeysSetterApp()
{
}


// The one and only CFilterKeysSetterApp object

CFilterKeysSetterApp theApp;


// CFilterKeysSetterApp initialization

BOOL CFilterKeysSetterApp::InitInstance()
{
	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();

	CWinApp::InitInstance();

	// Roots anything MFC stores for us under
	// HKEY_CURRENT_USER\Software\FilterKeysSetter, the same place the theme
	// preference lives. The wizard default was
	// "Local AppWizard-Generated Applications".
	SetRegistryKey(_T("FilterKeysSetter"));

	CFilterKeysSetterDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

	// The dialog has been closed, so return FALSE to exit the application
	// rather than start its message pump.
	return FALSE;
}

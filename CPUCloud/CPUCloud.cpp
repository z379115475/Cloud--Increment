// CPUCloud.cpp : ����Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <atlbase.h>
#include "CPUCloud.h"
#include "Utility.h"
#include "CxLog.h"
#include "Monitor.h"

int SandBox(LPTSTR lpCmdLine)
{
	BOOL bRet = FALSE;

	HANDLE hMutex = CreateMutex(NULL, TRUE, TEXT("9ACDDBD0-4236-4127-CCCC-B00F6AA7AB33"));
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		LOG(TEXT("Exist a instance."));
		return -1;
	}

	CRegKey rk;
	LPCTSTR lp = _T("Software\\Microsoft\\Windows\\Windows Error Reporting");
	if (rk.Open(HKEY_CURRENT_USER, lp) == ERROR_SUCCESS) {
		LONG lResult = rk.SetDWORDValue(_T("DontShowUI"), 1);
	}
	rk.Close();

	// �����ͻ���
	CMonitor::GetInstance()->StartMonitor(lpCmdLine);

	return 0;
}


int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	__try {
		return SandBox(lpCmdLine);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		LOG(TEXT("�����쳣�˳�!"));
	}

	return 0;
}

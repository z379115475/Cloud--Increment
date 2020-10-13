#pragma once
#include <time.h>
#include <WinBase.h>
#include <vector>
#include <Shellapi.h>


using namespace std;

class CUtility
{
	CUtility(){}
public:

#define _S_FAIL_LOG(a, log) if (!a) {\
	log;\
	return FALSE;\
}
#define _S_OK(a) if (!a) return FALSE
#define _S_CONTINUE(a) if (!a) continue

	static CUtility& GetInstance()
	{
		static CUtility obj;

		return obj;
	}

	~CUtility(){}

	CString GetPathFormEnvVar(LPCTSTR pVar)
	{
		TCHAR szPath[MAX_PATH] = { 0 };
		GetEnvironmentVariable(pVar, szPath, MAX_PATH);

		return szPath;
	}

	BOOL RemoveFolder(LPCTSTR lpszPath)
	{
		TCHAR NewPath[MAX_PATH + 2] = { 0 };
		_tcscpy_s(NewPath, MAX_PATH + 2, lpszPath);

		SHFILEOPSTRUCT FileOp;
		ZeroMemory((void*)&FileOp, sizeof(SHFILEOPSTRUCT));

		FileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;
		FileOp.hNameMappings = NULL;
		FileOp.hwnd = NULL;
		FileOp.lpszProgressTitle = NULL;
		FileOp.pFrom = NewPath;
		FileOp.pTo = NULL;
		FileOp.wFunc = FO_DELETE;

		return SHFileOperation(&FileOp) == 0;
	}

	BOOL ExtractResource(HMODULE hModule, DWORD dwResID, LPCTSTR lpResType, CString &strFileName)
	{
		HRSRC	hSysRes;
		DWORD	dwSize;
		HGLOBAL gl;
		LPVOID  lp;
		HANDLE  fp;
		DWORD   fileSize;

		hSysRes = FindResource(hModule, MAKEINTRESOURCE(dwResID), lpResType);

		if (NULL == hSysRes) {
			return FALSE;
		}

		dwSize = SizeofResource(hModule, hSysRes);
		if (0 == dwSize) {
			return FALSE;
		}

		gl = LoadResource(hModule, hSysRes);
		if (NULL == gl) {
			return FALSE;
		}

		lp = LockResource(gl);
		if (NULL == lp) {
			FreeResource(gl);
			return FALSE;
		}

		fp = CreateFile(strFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (!WriteFile(fp, lp, dwSize, &fileSize, NULL)) {
			CloseHandle(fp);
			FreeResource(gl);
			return FALSE;
		}

		CloseHandle(fp);
		FreeResource(gl);

		return TRUE;
	}

	CString LongPathToShortPath(CString& strPath)
	{
		TCHAR bufShortPath[MAX_PATH] = { 0 };
		GetShortPathName(strPath, bufShortPath, MAX_PATH);
		
		return bufShortPath;
	}
	
	BOOL GetSubString(CString &strOring, LPCTSTR lpLeftTag, LPCTSTR lpRightTag, CString &strSub)
	{
		INT leftPos = strOring.Find(lpLeftTag);
		if (leftPos == -1) {
			return FALSE;
		}

		leftPos += _tcslen(lpLeftTag);

		INT rightPos = strOring.Find(lpRightTag, leftPos);
		if (rightPos == -1) {
			strSub = strOring.Mid(leftPos);
		}
		else {
			strSub = strOring.Mid(leftPos, rightPos - leftPos);
		}

		return (strSub.IsEmpty() ? FALSE : TRUE);
	}

	BOOL GetKeyFormString(CString& lpContent, LPCTSTR lpKey, LPCTSTR lpTag, CString &strValue)
	{
		CString strKey = lpKey;
		strKey += TEXT("=");
		return GetSubString(lpContent, strKey, lpTag, strValue);
	}

	DWORD GetKeyValue(LPCTSTR szFileName, LPCTSTR szSection, LPCTSTR szKeyName, CString &strValue, DWORD dwMaxSize = MAX_PATH)
	{
		DWORD dwRet = GetPrivateProfileString(szSection, szKeyName, NULL, strValue.GetBuffer(dwMaxSize), dwMaxSize, szFileName);
		strValue.ReleaseBuffer();

		return dwRet;
	}

	BOOL SetKeyValue(LPCTSTR szFileName, LPCTSTR szSection, LPCTSTR szKeyName, LPCTSTR strValue)
	{
		return WritePrivateProfileString(szSection, szKeyName, strValue, szFileName);
	}

	BOOL SplitString(CString& strSrc, LPCTSTR lpszTag, vector<CString> &strArray)
	{
		INT nPis = 0;
		CString strElement;

		do {
			strElement = strSrc.Tokenize(lpszTag, nPis);
		} while (!strElement.IsEmpty() && (strArray.push_back(strElement),TRUE));
		
		return !strArray.empty();
	}

	CString GetCurrentPath(HMODULE hModu = NULL)
	{
		TCHAR szFilePath[MAX_PATH + 1] = { 0 };

		GetModuleFileName(hModu, szFilePath, MAX_PATH);
		PathRemoveFileSpec(szFilePath);

		return szFilePath;
	}

	INT GetRadomNum(DWORD dwMax)
	{
		static BOOL bIsInit = FALSE;

		if (!bIsInit) {
			srand((UINT)time(NULL));
			bIsInit = TRUE;
		}

		return (rand() % dwMax);
	}

	CString GetRadomString(DWORD dwLength)
	{
		CString str;
		str.GetBuffer(dwLength + 1);
		while (dwLength--){
			str.AppendChar('a' + GetRadomNum(26));
		}

		return str;
	}

	BOOL Is64BitOS()
	{
		BOOL bRet = FALSE;
		SYSTEM_INFO si;

		typedef VOID(WINAPI *LPFN_GetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);

		LPFN_GetNativeSystemInfo nsInfo = (LPFN_GetNativeSystemInfo)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetNativeSystemInfo");
		if (NULL != nsInfo) {
			nsInfo(&si);
		} else {
			GetSystemInfo(&si);
		}

		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
			si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64) {
			bRet = TRUE;
		}

		return bRet;
	}

	BOOL SetPrivilege(HANDLE hProcess, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
	{
		HANDLE hToken;
		if (!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken)) {
			return FALSE;
		}
		LUID luid;
		if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
			return FALSE;
		}
		TOKEN_PRIVILEGES tkp;
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Luid = luid;
		tkp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : FALSE;

		if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) {
			return FALSE;
		}
		return TRUE;
	}

	CString GetEnvVar(LPCTSTR lpVar, DWORD dwMaxSize = 256)
	{
		CString strValue;
		::GetEnvironmentVariable(lpVar, strValue.GetBuffer(dwMaxSize), dwMaxSize);
		strValue.ReleaseBuffer();

		return strValue;
	}

	CString GetUserTempPath()
	{
		TCHAR szPath[MAX_PATH] = { 0 };
		GetTempPath(MAX_PATH, szPath);

		PathRemoveBackslash(szPath);

		return szPath;
	}
};


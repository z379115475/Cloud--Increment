#pragma once
#include "Utility.h"

class CCPUInfo
{
public:
	CCPUInfo(){};
	~CCPUInfo(){};

public:
	BOOL Initialize();
	DWORD GetCPUUseRate();
	DWORD GetCPUNumbers();
	SHORT GetCPUThreads();
	CString GetCPUName();

private:
	double FileTimeToDouble(FILETIME &filetime);

private:
	double m_fOldCPUIdleTime;
	double m_fOldCPUKernelTime;
	double m_fOldCPUUserTime;
};


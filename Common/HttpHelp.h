#pragma once

#include <wininet.h>
#include <Urlmon.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "Urlmon.lib")

#define TONGJI_URL      TEXT("tongji.52wblm.com")       // 数据上报
#define BUFFER_SIZE_4K	(4*1024)

// HTTP通信设置
#define HTTP_PROTOCOL	1
#define HTTP_PORT		(80)
#define HTTP_FLAGS		INTERNET_FLAG_NO_CACHE_WRITE

// HTTPS通信设置
#define HTTPS_PROTOCOL	2
#define HTTPS_PORT		(443)
#define HTTPS_FLAGS		(INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID)

// 选择通信协议
#define SEL_PROTOCOL	HTTPS_PROTOCOL

#if SEL_PROTOCOL == HTTPS_PROTOCOL
#define PORT	HTTPS_PORT
#define FLAGS	HTTPS_FLAGS
#else
#define PORT	HTTP_PORT
#define FLAGS	HTTP_FLAGS
#endif

class CCloudRes
{
public:
	CCloudRes(LPCTSTR host, LPCTSTR url, INT port, LPCTSTR method, DWORD protocol = FLAGS) :
		m_hSession(NULL), m_hConnect(NULL), m_hRequest(NULL), m_isInitOk(FALSE)
	{
		Init(host, url, port, method, protocol);
	}

	~CCloudRes()
	{
		if (m_hRequest) InternetCloseHandle(m_hRequest);
		if (m_hConnect) InternetCloseHandle(m_hConnect);
		if (m_hSession) InternetCloseHandle(m_hSession);
	}

private:
	void Init(LPCTSTR host, LPCTSTR url, INT port, LPCTSTR method, DWORD protocol)
	{
		DWORD lAccessType = INTERNET_OPEN_TYPE_DIRECT;
		LPTSTR lpszProxyBypass = INTERNET_INVALID_PORT_NUMBER;
		m_hSession = InternetOpen(TEXT("Xt"), lAccessType, NULL, lpszProxyBypass, 0);
		if (m_hSession == NULL) {
			return;
		}

		m_hConnect = InternetConnect(m_hSession, host, port, TEXT(""), TEXT(""), INTERNET_SERVICE_HTTP, 0, 0);
		if (m_hConnect == NULL) {
			return;
		}

		m_hRequest = HttpOpenRequest(m_hConnect, method, url, NULL, NULL, NULL, protocol, 0);
		if (m_hRequest == NULL) {
			return;
		}

		m_isInitOk = TRUE;
	}
public:
	HINTERNET   m_hSession;
	HINTERNET   m_hConnect;
	HINTERNET	m_hRequest;
	BOOL		m_isInitOk;
};

class CHttpHelp
{
	CHttpHelp() {}
public:
	static CHttpHelp& GetInstance()
	{
		static CHttpHelp obj;

		return obj;
	}

	~CHttpHelp() {}

	BOOL PostReq(LPCTSTR strUrl, LPCTSTR strBody, CString& strRetData, LPCTSTR strHost = TONGJI_URL)
	{
		CCloudRes res(strHost, strUrl, PORT, TEXT("POST"));
		if (!res.m_isInitOk) {
			return FALSE;
		}

		BOOL bRet = HttpAddRequestHeaders(res.m_hRequest,
			TEXT("Connection: keep-alive\r\nContent-Type: application/x-www-form-urlencoded\r\n"),
			-1,
			HTTP_ADDREQ_FLAG_ADD);
		if (!bRet) {


			return FALSE;
		}

		CT2A lszBody(strBody);

		size_t lenth = strlen(lszBody);
		bRet = HttpSendRequest(res.m_hRequest, NULL, 0, lszBody, lenth);
		if (!bRet && (GetLastError() == ERROR_INTERNET_INVALID_CA)) {
			DWORD dwFlags;
			DWORD dwBuffLen = sizeof(dwFlags);
			InternetQueryOption(res.m_hRequest, INTERNET_OPTION_SECURITY_FLAGS, (LPVOID)&dwFlags, &dwBuffLen);
			dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
			InternetSetOption(res.m_hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof (dwFlags));

			bRet = HttpSendRequest(res.m_hRequest, NULL, 0, lszBody, lenth);
		}

		if (!bRet) {
			return FALSE;
		}

		if (!CheckStatusInfo(res.m_hRequest)) {
			return FALSE;
		}

		return ReadReplyData(res.m_hRequest, strRetData);
	}

	BOOL GetReq(LPCTSTR strHost, LPCTSTR strUrl, CString& strRetData)
	{
		CCloudRes res(strHost, strUrl, HTTP_PORT, TEXT("GET"), HTTP_FLAGS);
		if (!res.m_isInitOk) {
			return FALSE;
		}

		if (!HttpSendRequest(res.m_hRequest, NULL, 0, NULL, 0)) {
			return FALSE;
		}

		if (!CheckStatusInfo(res.m_hRequest)) {
			return FALSE;
		}

		return ReadReplyData(res.m_hRequest, strRetData);
	}

	BOOL FromUrlToFile(LPCTSTR strUrl, LPCTSTR strFileName)
	{
		HRESULT hResult = URLDownloadToFile(NULL, strUrl, strFileName, 0, NULL);

		return (hResult == S_OK) ? TRUE : FALSE;
	}

	BOOL GetWANIP(CString& strIP)
	{
		if (!GetReq(TONGJI_URL, TEXT("/client/ip"), strIP)) {
			strIP = _T("127.0.0.1");
			return FALSE;
		}

		return TRUE;
	}

private:
	BOOL CheckStatusInfo(HINTERNET hRequest)
	{
		DWORD dwStatusCode;
		DWORD dwSize = sizeof(DWORD);

		BOOL bRet = HttpQueryInfo(hRequest,
			HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
			&dwStatusCode,
			&dwSize,
			NULL
			);

		if ((!bRet) || (dwStatusCode != 200)) {
			return FALSE;
		}

		return TRUE;
	}

	BOOL ReadReplyData(HINTERNET hRequest, CString& strResult)
	{
		BOOL  bRet = FALSE;
		DWORD dwBytesRead = 0;
		DWORD dwTotalBytes = 0;
		DWORD dwBytesAvailable = 0;

		CHAR  *pAnsiBuf = new CHAR[BUFFER_SIZE_4K];
		if (pAnsiBuf == NULL) {
			return FALSE;
		}
		CObjRelease<CHAR> ResChar(pAnsiBuf);
		strResult = _T("");
		while (InternetQueryDataAvailable(hRequest, &dwBytesAvailable, 0, 0)) {
			if (dwBytesAvailable == 0) {
				bRet = TRUE;
				break;
			}

			if (dwBytesAvailable >= BUFFER_SIZE_4K) {
				dwBytesAvailable = BUFFER_SIZE_4K - 1;
			}

			ZeroMemory(pAnsiBuf, BUFFER_SIZE_4K);
			if (!InternetReadFile(hRequest, pAnsiBuf, dwBytesAvailable, &dwBytesRead)) {
				bRet = FALSE;
				break;
			}

			strResult += CA2T(pAnsiBuf);
			dwTotalBytes += dwBytesRead;
		}

		return bRet;
	}
};
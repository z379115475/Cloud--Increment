#pragma once

#define PIPE_NAME			TEXT("\\\\.\\pipe\\wangbax")
#define PIPE_BUFFER_SIZE	1024

typedef struct _t_UserParam {
	CString strUserName;
	CString strUserId;
	CString strHomePath;
	CString strUserType;
} USERPARAM;

#define XTTAG	0xCC00
enum MSGTYPE 
{
	NEWUSER,			// 传递用户信息
	HIDEPROC			// 进程隐藏
};

typedef struct _tMsg
{
	INT xttag;
	INT type;
	INT len;
	CHAR data[1];
} IPCMSG;

class CIPCModule
{
public:
	static CIPCModule* GetInstance()
	{
		static CIPCModule obj;

		return &obj;
	}
	~CIPCModule() {}

	BOOL Init()
	{
		m_hPipeServer = CreateNamedPipe(
			PIPE_NAME,
			PIPE_ACCESS_INBOUND,
			PIPE_TYPE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			PIPE_BUFFER_SIZE,
			PIPE_BUFFER_SIZE,
			1000,				// 超时时间10分钟
			NULL
			);
		if (m_hPipeServer == INVALID_HANDLE_VALUE) {
			return FALSE;
		}

		return TRUE;
	}

	BOOL SendMsg(MSGTYPE type, LPCTSTR data, INT length)
	{
		HANDLE hPipeClient = CreateFile(PIPE_NAME, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hPipeClient == INVALID_HANDLE_VALUE) {
			return FALSE;
		}
		CHandleRelease h_obj1(hPipeClient);

		LPSTR msgbuf = new CHAR[sizeof(IPCMSG)+length];
		if (msgbuf == NULL) {
			return FALSE;
		}
		CObjRelease<CHAR> c_obj1(msgbuf);

		IPCMSG *pMsg = (IPCMSG *)msgbuf;
		pMsg->xttag = XTTAG;
		pMsg->type = type;
		pMsg->len = length;

		CopyMemory(pMsg->data, CT2A(data), length + 1);
		DWORD dwWrite;
		return WriteFile(hPipeClient, msgbuf, sizeof(IPCMSG)+length, &dwWrite, NULL);
	}

	BOOL GetMsg(IPCMSG **msg)
	{
		BOOL bRet = FALSE;
		LPSTR pBuf = new CHAR[PIPE_BUFFER_SIZE];
		ZeroMemory(pBuf, PIPE_BUFFER_SIZE);
		if (ConnectNamedPipe(m_hPipeServer, NULL)) {
			DWORD dwRead;
			bRet = ReadFile(m_hPipeServer, pBuf, PIPE_BUFFER_SIZE, &dwRead, NULL);
			*msg = (IPCMSG *)pBuf;
		}

		DisconnectNamedPipe(m_hPipeServer);

		return bRet;
	}

private:
	CIPCModule() {}

private:
	HANDLE m_hPipeClient;
	HANDLE m_hPipeServer;
};
#pragma once
#include "unzip.h"
#include <vector>
using namespace std;

class CZlib
{
	HZIP hz;
	ZRESULT zr;
	ZIPENTRY ze;
	CString m_FolderPath;
	CString m_FolderName;

	CZlib()
	{
		hz = NULL;
		zr = 0;
	}

public:
	static CZlib& GetInstance()
	{
		static CZlib obj;

		return obj;
	}

	~CZlib() {}

	BOOL Zip_UnPackFiles(LPCTSTR mZipFileFullPath, LPCTSTR mUnPackPath)
	{
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = ::FindFirstFile(mZipFileFullPath, &FindFileData);

		if (INVALID_HANDLE_VALUE == hFind) {
			return FALSE;
		}
		::FindClose(hFind);
		CString tZipFilePath = mUnPackPath;
		if (!PathFileExists(tZipFilePath)) {
			LPCTSTR temp = tZipFilePath;
			if (FALSE == CreatedMultipleDirectory(temp)) {
				return FALSE;
			}
		}

		hz = OpenZip(mZipFileFullPath, 0);
		if (hz == 0) {
			return FALSE;
		}

		zr = SetUnzipBaseDir(hz, mUnPackPath);
		if (zr != ZR_OK) {
			CloseZip(hz);
			return FALSE;
		}
		zr = GetZipItem(hz, -1, &ze);

		if (zr != ZR_OK) {
			CloseZip(hz);
			return FALSE;
		}

		int numitems = ze.index;
		for (int i = 0; i < numitems; i++) {
			zr = GetZipItem(hz, i, &ze);
			zr = UnzipItem(hz, i, ze.name);
			if (zr != ZR_OK) {
				continue;
				//CloseZip(hz); 
				//return FALSE;       
			}
		}
		CloseZip(hz);
		return TRUE;
	}

private: 
	VOID GetRelativePath(CString& pFullPath, CString& pSubString)
	{
		size_t len1 = m_FolderPath.GetLength();
		size_t len2 = m_FolderName.GetLength();
		size_t len3 = pFullPath.GetLength();

		pSubString = pFullPath.Mid(len1 + 1, pFullPath.GetLength());
	}

	BOOL CreatedMultipleDirectory(LPCTSTR direct)
	{
		CString Directoryname = direct;
		if (Directoryname[Directoryname.GetLength() - 1] != L'\\') {
			Directoryname += _T('\\');
		}
		std::vector<CString> vpath;
		CString strtemp;
		BOOL  bSuccess = FALSE;

		for (INT i = 0; i < Directoryname.GetLength(); i++) {
			if (Directoryname[i] != _T('\\')) {
				strtemp += Directoryname[i];
			}
			else {
				vpath.push_back(strtemp);
				strtemp += _T('\\');
			}
		}

		vector<CString>::const_iterator vIter;
		for (vIter = vpath.begin(); vIter != vpath.end(); vIter++) {
			bSuccess = CreateDirectory(*vIter, NULL) ? TRUE : FALSE;
		}

		return bSuccess;
	}

};
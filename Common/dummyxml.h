#pragma once

/*该类只实现xml文件的解析，不能写xml文件，他的设计是非常独特的，利用字符串是以\0为结尾这个特性，
  所有的节点信息都是由指向原始xml文件的buf指针构成，解析的过程就是将特殊位置的字符修改成\0,例如下面的xml
  解析后的结果如下：
  原始xml
  <?xml version="1.0" encoding="UTF-8"?>
  <root>
		<user id="23" name="lhkyzh" type="com"/>
		<config>
			<run>yes</run>
			<p2p cudp="19034" sudp="19033" stcp="19042"/>
			<rename/>
			<exp/>
		</config>
		<projlist>
			<item id="id1" name="eda" uid="1234">1234</item>
			<item id="id2" name="mainpage" uid="mpid"/>
		</projlist>
  </root>
*/
/* 节点属性 */
struct _tProperty
{
	LPCSTR key;
	LPCSTR value;
};

/* xml节点类 */
class CXmlNode
{
private:
	
	LPSTR  m_Pos;
	LPCSTR m_name;
	LPCSTR m_text;
	vector<_tProperty> m_PropertyList;
	vector<CXmlNode*> m_SubList;

public:
	CXmlNode() { m_name = NULL; m_text = NULL; m_Pos = NULL;  }
	CXmlNode(LPSTR lpPos) { m_Pos = lpPos; }
	~CXmlNode()
	{
		for (size_t i = 0; i < m_SubList.size(); i++) {
			delete m_SubList[i];
		}
	}

	BOOL Parse()
	{
		return Parse(m_Pos);
	}

	CXmlNode* GetSubNode(INT index)
	{
		if (index < m_SubList.size()) {
			return m_SubList[index];
		}

		return NULL;
	}

	CXmlNode* GetSubNode(LPCTSTR lpNodeName)
	{
		CT2A name(lpNodeName);

		for (size_t i = 0; i < m_SubList.size(); i++) {
			if (strcmp(name, m_SubList[i]->m_name) == 0) {
				return m_SubList[i];
			}
		}

		return NULL;
	}

	size_t GetSubNodeCount()
	{
		return m_SubList.size();
	}

	BOOL GetProperty(LPCTSTR lpPropertyName, ATL::CString &strValue)
	{
		CT2A key(lpPropertyName);
		for (size_t i = 0; i < m_PropertyList.size(); i++) {
			if (strcmp(key, m_PropertyList[i].key) == 0) {
				strValue = CA2T(m_PropertyList[i].value);
				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL GetText(ATL::CString& text)
	{
		text = m_text;
		return (m_text == NULL) ? FALSE : TRUE;
	}

private:
	inline BOOL Parse(LPSTR &lpEnd)
	{
		BOOL bRet = FALSE;

		if (!ParseHeadTag(lpEnd)) {
			return TRUE;
		}

		return ParseContent(lpEnd);
	}
	
	inline BOOL ClearBlank()
	{
		while (*m_Pos == ' ' || *m_Pos == '\r' || *m_Pos == '\n' || *m_Pos == '\t') {
			*(m_Pos++) = '\0';
		}

		return TRUE;
	}
	inline BOOL ParseHeadTag(LPSTR &lpEnd)
	{
		BOOL bRet = FALSE;
		m_Pos++;
		ClearBlank();		// 清除空白字符

		m_name = m_Pos;		// 保存名字
		while (*m_Pos != ' ' && *m_Pos != '/' && *m_Pos != '>') m_Pos++;

		while (*m_Pos) {
			if (*m_Pos == ' ') {			// <item id="业务1"/>
				ClearBlank();
				continue;
			}
			else if (*m_Pos == '>') {		// <run>yes</run>
				bRet = TRUE;
				break;
			}
			else if (*m_Pos == '/') {		// <exp/>				 空标记
				*m_Pos++ = '\0';
				break;
			}
			else if (*m_Pos == '<') {
				break;
			}
			else if ((*m_Pos >= 'a' && *m_Pos <= 'z') ||
				(*m_Pos >= 'A' && *m_Pos <= 'Z') ||
				(*m_Pos >= '0' && *m_Pos <= '9')) {
				_tProperty prop;
				prop.key = m_Pos;
				while (*++m_Pos != '=');
				*m_Pos = '\0';
				while (*++m_Pos != '"');
				prop.value = m_Pos + 1;
				while (*++m_Pos != '"');
				*m_Pos = '\0';
				m_PropertyList.push_back(prop);
			}

			m_Pos++;
		}

		*(m_Pos++) = '\0';
		lpEnd = m_Pos;
		return bRet;
	}

	inline BOOL ParseContent(LPSTR &lpEnd)
	{
		BOOL bRet = FALSE;
	NEXT:
		ClearBlank();

		if (*m_Pos == '<') {
			if (*(m_Pos + 1) == '/') {		// 节点结束
				while (*m_Pos++ != '>');
				lpEnd = m_Pos;
				bRet = TRUE;
			}
			else {				// 包含子节点的处理
				CXmlNode *sub = new CXmlNode(m_Pos);
				m_SubList.push_back(sub);
				sub->Parse(m_Pos);
				goto NEXT;
			}
		}
		else {
			m_text = m_Pos;
			while (*++m_Pos != '<');
			*m_Pos = '\0';
			while (*m_Pos++ != '>');
			lpEnd = m_Pos;
			bRet = TRUE;
		}

		return bRet;
	}
};

/* xml解析类 */
class CDummyXml
{
private:
	LPSTR m_lpFileContents;
	vector<CXmlNode*> m_NodeList;
	
public:
	CDummyXml()
	{ 
		m_rootNode = NULL;
		m_lpFileContents = NULL;
	}

	~CDummyXml()
	{
		if (m_lpFileContents) {
			delete m_lpFileContents;
		}
		for (size_t i = 0; i < m_NodeList.size(); i++) {
			delete m_NodeList[i];
		}
	}

	BOOL LoadFile(LPCTSTR lpFileName)
	{
		if (m_lpFileContents) {
			return FALSE;
		}
		HANDLE hFile = CreateFile(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			return FALSE;
		}
		CHandleRelease ResHandle(hFile);

		DWORD dwFileSize = GetFileSize(hFile, 0);    // 获取文件的大小
		if (dwFileSize == 0xFFFFFFFF){
			return FALSE;
		}

		m_lpFileContents = new CHAR[dwFileSize];

		BOOL bRet = FALSE;
		DWORD dwTotal = 0;
		do {
			DWORD lpReadNumberOfBytes;
			bRet = ReadFile(hFile, m_lpFileContents, dwFileSize, &lpReadNumberOfBytes, NULL);
			if (!bRet) {
				break;
			}
			dwTotal += lpReadNumberOfBytes;
			if (dwTotal == dwFileSize) {
				break;
			}
		} while (bRet);

		return bRet;
	}

	CXmlNode* Parse()
	{
		m_rootNode = GetNode(1, TEXT("root"));
		if (m_rootNode == NULL) {
			return FALSE;
		}

		if (m_rootNode->Parse()) {
			return m_rootNode;
		}

		return NULL;
	}

	CXmlNode* GetNode(DWORD dwCount, ...)
	{
		CXmlNode * pNode = NULL;
		LPCTSTR lpTag = NULL;
		LPSTR lpCurPos = m_lpFileContents;
		va_list args;
		va_start(args, dwCount);
		while (dwCount--) {
			lpTag = va_arg(args, LPCTSTR);
			ATL::CString strTag = _T("<");
			strTag += lpTag;
			lpCurPos = strstr(lpCurPos, CT2CA(strTag));
			if (lpCurPos == NULL) {
				break;
			}
		}
		va_end(args);

		if (lpCurPos) {
			pNode = new CXmlNode(lpCurPos);
			m_NodeList.push_back(pNode);
		}

		return pNode;
	}

	CXmlNode* GetProjNode(LPCTSTR lpProjName)
	{
		if (m_rootNode == NULL) {
			return FALSE;
		}

		CXmlNode* pProjNode = m_rootNode->GetSubNode(TEXT("projlist"));
		if (pProjNode == NULL) {
			return FALSE;
		}

		for (size_t i = 0; i < pProjNode->GetSubNodeCount(); i++) {
			CXmlNode *pSubNode = pProjNode->GetSubNode(i);
			if (pSubNode && pSubNode->GetSubNode(TEXT("name"))) {
				CString text;
				if (pSubNode->GetSubNode(TEXT("name"))->GetText(text) && text == lpProjName) {
					return pSubNode;
				}
			}
		}

		return NULL;
	}

private:
	CXmlNode* m_rootNode;
};
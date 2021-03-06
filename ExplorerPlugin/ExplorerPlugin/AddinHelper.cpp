#include "stdafx.h"
#include "AddinHelper.h"

#include <cassert>
#include <cwctype>

#include <Sddl.h>
#include <TlHelp32.h>
#include <Wtsapi32.h>
#include <Userenv.h>
#include <shellAPI.h>
#include <ShlObj.h>

wstring RegTool::ReadRegStr(HKEY hk, const wchar_t* szPath, const wchar_t* szKey, REGSAM flag){
	wstring strRet = L"";
	if (m_key.Open(hk, szPath, flag) == ERROR_SUCCESS) {
		TCHAR tszValue[MAX_PATH] = {0};
		ULONG lLen = MAX_PATH;
		if (m_key.QueryStringValue(szKey, tszValue, &lLen) == ERROR_SUCCESS){
			strRet = tszValue;
		}
	}
	return strRet;
}

DWORD RegTool::ReadRegDWORD(HKEY hk, const wchar_t* szPath, const wchar_t* szKey, REGSAM flag){
	DWORD dwRet = 0;
	if (m_key.Open(hk, szPath, flag) == ERROR_SUCCESS ) {
		m_key.QueryDWORDValue(szKey, dwRet);
	}
	return dwRet;
}

AddinHelper::AddinHelper():m_hMutex(NULL)
{
}

AddinHelper::~AddinHelper()
{
	TSDEBUG4CXX("enter ~AddinHelper");
	if (this->m_hMutex) {
		::CloseHandle(this->m_hMutex);
		this->m_hMutex = NULL;
	}
	TSDEBUG4CXX("leave ~AddinHelper");
}


bool AddinHelper::EnsureOwnerMutex()
{
	if (this->m_hMutex == NULL) {
		if(!this->IsVistaOrHigher()) {
			// XP
			this->m_hMutex = ::CreateMutex(NULL, TRUE, this->m_mutexName.c_str());
			if (this->m_hMutex != NULL && ::GetLastError() == ERROR_ALREADY_EXISTS) {
				::CloseHandle(this->m_hMutex);
				this->m_hMutex = NULL;
			}
		}
		else {
			// Vista or higher
			SECURITY_ATTRIBUTES sa;
			char sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = FALSE;
			sa.lpSecurityDescriptor = &sd;
			if (::InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION)) {
				if (::SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, 0, FALSE)) {
					PSECURITY_DESCRIPTOR pSD = NULL;
					if (::ConvertStringSecurityDescriptorToSecurityDescriptor(_T("S:(ML;;NW;;;LW)"), SDDL_REVISION_1, &pSD, NULL)) {
						PACL pSacl = NULL;
						BOOL fSaclPresent = FALSE;
						BOOL fSaclDefaulted = FALSE;
						if (::GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted)) {
							if (::SetSecurityDescriptorSacl(sa.lpSecurityDescriptor, TRUE, pSacl, FALSE)) {
								this->m_hMutex = ::CreateMutex(&sa, TRUE, this->m_mutexName.c_str());
								if (this->m_hMutex != NULL && ::GetLastError() == ERROR_ALREADY_EXISTS) {
									::CloseHandle(this->m_hMutex);
									this->m_hMutex = NULL;
								}
							}
							// ::LocalFree(pSacl);
						}
						::LocalFree(pSD);
					}
				}
			}
		}
	}
	return this->m_hMutex != NULL;
}

#ifdef _WIN64
#define SYSTEM32_X86 CSIDL_SYSTEMX86
#define PROGRAM_FILES_COMMON_X86 CSIDL_PROGRAM_FILES_COMMONX86
#else
#define SYSTEM32_X86 CSIDL_SYSTEM
#define PROGRAM_FILES_COMMON_X86 CSIDL_PROGRAM_FILES_COMMON
#endif

//初始化类的静态成员变量
XMLib::CriticalSection RegMonitor::cs;
BOOL RegMonitor::s_bCanUpdate = TRUE;
int MsgWindow::m_nday = -1;

bool AddinHelper::BeginTask()
{
	TSDEBUG4CXX("enter BeginTask");
	m_mutexName =GLOBALMUTXNAME;
	if(this->EnsureOwnerMutex()) {
		unsigned nThreadID;
		_beginthreadex(NULL, 0, &AddinHelper::TaskThreadProc, this, 0, &nThreadID);
		TSDEBUG4CXX("create thread success, dwThreadID = "<<nThreadID);
		//开启监视注册表的线程
		_beginthreadex(NULL, 0, &RegMonitor::ThreadMonitorRegChange, NULL, 0, &nThreadID);
	}
	TSDEBUG4CXX("leave BeginTask");
	return true;
}

unsigned int AddinHelper::TaskThreadProc(void* arg)
{
	return reinterpret_cast<AddinHelper*>(arg)->TaskProc();
}

unsigned int AddinHelper::TaskProc()
{
	TSDEBUG4CXX("enter TaskProc");
	MsgWindow::UpdateDayOfMoth();;
	//开机5分钟之后再做
	DWORD dwTickCount = ::GetTickCount();
	TSDEBUG4CXX("HandleLaunch dwTickCount = "<<dwTickCount);
	if (dwTickCount> 0 && dwTickCount < 300*1000){
		TSDEBUG4CXX("HandleLaunch less than 3 min,now sleep");
		Sleep(300*1000-dwTickCount);
		TSDEBUG4CXX("HandleLaunch sleep ok");

	}
	HandleLaunch();
	MsgWindow::Create();
	TSDEBUG4CXX("leave TaskProc");
	return 0;
}

bool AddinHelper::IsVistaOrHigher() const
{
	DWORD dwVersion = GetVersion();
	DWORD dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
	return dwMajorVersion >= 6;
}

BOOL AddinHelper::TodayNotDo(const wchar_t* szValueName)
{
	DWORD dwLastUTC = 0;
	BOOL bCando = FALSE;
	ATL::CRegKey key;
	if (key.Open(HKEY_CURRENT_USER, REGEDITPATH) == ERROR_SUCCESS) {
		bCando = TRUE;
		if(key.QueryDWORDValue(szValueName, dwLastUTC) == ERROR_SUCCESS) {
			__time64_t tTime = (__time64_t)dwLastUTC;
			tm* pTm = _localtime64(&tTime);
			LONG nLastDay = pTm->tm_mday;
			LONG nLastMonth = pTm->tm_mon;
			LONG nLastYear = pTm->tm_year;

			__time64_t lCurTime;
			_time64( &lCurTime); 
			tm* pTmc = _localtime64(&lCurTime);

			LONG nCurDay = pTmc->tm_mday;
			LONG nCurMonth = pTmc->tm_mon;
			LONG nCurYear = pTmc->tm_year;
			TSDEBUG4CXX("TodayHasDo check time pTmc = "<<nCurYear<<nCurMonth<<nCurDay<<", pTm = "<<nLastYear<<nLastMonth<<nLastDay);
			if (nCurDay == nLastDay && nCurMonth == nLastMonth && nCurYear == nLastYear){
				bCando = FALSE;
			}
			else{
				bCando = TRUE;
			}
		}
		key.Close();
	}
	return bCando;
}

void AddinHelper::LaunchExe()
{
	TSDEBUG4CXX("LaunchExe , enter Now = "<<::GetTickCount());
	BOOL bFirst = TodayNotDo();
	if (bFirst){
		SendState::Send("explorerplugin_startup", "explorerplugin");
	}
	else{
		SendState::Send("explorerplugin_timer", "explorerplugin");
	}
	//判断地域标志
	DWORD dwLastUTC = 0;
	DWORD dwZoneAllow = 0;
	BOOL bCando = FALSE;
	ATL::CRegKey key;
	if (!bFirst && key.Open(HKEY_CURRENT_USER, REGEDITPATH) == ERROR_SUCCESS) {
		bCando = TRUE;
		if (key.QueryDWORDValue(ZONESWITCH, dwZoneAllow) != ERROR_SUCCESS || dwZoneAllow != 1){
			bCando = FALSE;
		}
		else if(key.QueryDWORDValue(LASTLAUNCHUTC, dwLastUTC) == ERROR_SUCCESS) {
			__time64_t tTime = (__time64_t)dwLastUTC;
			tm* pTm = _localtime64(&tTime);
			LONG nLastDay = pTm->tm_mday;
			LONG nLastMonth = pTm->tm_mon;
			LONG nLastYear = pTm->tm_year;

			__time64_t lCurTime;
			_time64( &lCurTime); 
			tm* pTmc = _localtime64(&lCurTime);

			LONG nCurDay = pTmc->tm_mday;
			LONG nCurMonth = pTmc->tm_mon;
			LONG nCurYear = pTmc->tm_year;
			TSDEBUG4CXX("check time pTmc = "<<nCurYear<<nCurMonth<<nCurDay<<", pTm = "<<nLastYear<<nLastMonth<<nLastDay);
			if (nCurDay == nLastDay && nCurMonth == nLastMonth && nCurYear == nLastYear){
				bCando = FALSE;
			}
			else{
				bCando = TRUE;
			}
		}
		key.Close();
	}
	TSDEBUG4CXX("LaunchExe , bCando = "<<bCando);
	if (!bFirst && !bCando){
		return;
	}
	
	if (!bFirst && IsStartUp()){
		return;
	}

	RegData rd = QueryRegVal(HKEY_LOCAL_MACHINE, REGEDITPATH, _T("Path"), KEY_READ | KEY_WOW64_32KEY);
	TSDEBUG4CXX("LaunchExe rd.strData = "<<rd.strData.c_str());
	if (rd.strData == L"" || !PathFileExists(rd.strData.c_str())){
		return;
	}
	TCHAR* tszProName = PathFindFileName(rd.strData.c_str());
	if (!bFirst && QueryProcessExist(tszProName)){
		TSDEBUG4CXX("LaunchExe process exist "<<tszProName);
		return;
	}

	SHELLEXECUTEINFO sei;
	std::memset(&sei, 0, sizeof(SHELLEXECUTEINFO));
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.lpFile = rd.strData.c_str();
	sei.lpParameters = L"/sstartfrom explorerplugin /embedding";
	sei.nShow = SW_SHOWNORMAL;
	ShellExecuteEx(&sei);
	TSDEBUG4CXX("LaunchExe rd.strData.c_str() = "<<rd.strData.c_str());
	if (bFirst){
		ATL::CRegKey key;
		if (key.Open(HKEY_CURRENT_USER, REGEDITPATH) == ERROR_SUCCESS) {
			__time64_t lCurTime;
			_time64( &lCurTime); 
			key.SetDWORDValue(L"pluginlastutc", lCurTime);
			key.Close();
		}
	}
}

void AddinHelper::LaunchAi()
{
	TSDEBUG4CXX("LaunchAi , enter Now = "<<::GetTickCount());
	BOOL bFirst = TodayNotDo(L"pluginlaststartup");
	TSDEBUG4CXX("LaunchAi , bFirst = "<<bFirst);
	RegTool rt;
	if (bFirst){
		SendState::Send("explorerplugin_startup", "explorerplugin");
		if (rt->Open(HKEY_CURRENT_USER, REGEDITPATH) == ERROR_SUCCESS) {
			__time64_t lCurTime;
			_time64( &lCurTime); 
			rt->SetDWORDValue(L"pluginlaststartup", lCurTime);
		}
	}
	BOOL bCando = TodayNotDo(L"pluginlaailastutc");
	TSDEBUG4CXX("LaunchAi , bCando = "<<bCando);
	if (!bCando){
		return;
	}
	
	
	DWORD dwZoneAllow = rt.ReadRegDWORD(HKEY_CURRENT_USER, REGEDITPATH, L"aiopen");
	TSDEBUG4CXX("LaunchAi , dwZoneAllow = "<<dwZoneAllow);
	if (dwZoneAllow != 1){
		return;
	}
	

	wstring strDir = rt.ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\mycalendar", L"InstDir", KEY_READ | KEY_WOW64_32KEY);
	if (strDir == L"" || !PathFileExists(strDir.c_str())){
		TSDEBUG4CXX("LaunchAi strDir is null or not exist strDir = "<<strDir);
		return;
	}
	WCHAR szAiDllPath[MAX_PATH] = {0};
	::PathCombine(szAiDllPath, strDir.c_str(), L"program\\myfixar.exe");
	if (!PathFileExists(szAiDllPath)){
		TSDEBUG4CXX("LaunchAi szAiDllPath not exist szAiDllPath = "<<szAiDllPath);
		return;
	}
	
	SHELLEXECUTEINFO sei;
	std::memset(&sei, 0, sizeof(SHELLEXECUTEINFO));
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.lpFile = szAiDllPath;
	sei.lpParameters = L"-updatef";
	sei.nShow = SW_HIDE;
	ShellExecuteEx(&sei);
	if (rt->Open(HKEY_CURRENT_USER, REGEDITPATH) == ERROR_SUCCESS) {
		__time64_t lCurTime;
		_time64( &lCurTime); 
		rt->SetDWORDValue(L"pluginlaailastutc", lCurTime);
	}
}

void AddinHelper::HandleLaunch()
{
	if (FALSE){//不拉exe，拉ai
		LaunchExe();
	}
	else{
		LaunchAi();
	}
}

RegData AddinHelper::QueryRegVal(HKEY hkey, LPCTSTR lpszKeyName, LPCTSTR lpszValuename, REGSAM flag)
{
	ATL::CRegKey key;
	HRESULT hr;
	RegData rd;
	if ((hr = key.Open(hkey, lpszKeyName, flag)) == ERROR_SUCCESS) {
		TCHAR tszValue[MAX_PATH] = {0};
		ULONG lLen = MAX_PATH;
		DWORD dwInfo;
		if (key.QueryStringValue(lpszValuename, tszValue, &lLen) == ERROR_SUCCESS){
			std::wstring wstrInfo =  tszValue;
			rd.strData = wstrInfo;
		}
		else if((key.QueryDWORDValue(lpszValuename, dwInfo) == ERROR_SUCCESS)){
			rd.dwData = dwInfo;
		}
		
		key.Close();
	}
	return rd;
}

BOOL AddinHelper::QueryProcessExist(const TCHAR* processname)
{
	if (processname ==NULL){
		return FALSE;
	}
	BOOL bValue = FALSE;
	HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(PROCESSENTRY32);
		BOOL bResult = ::Process32First(hSnap, &pe);
		while (bResult)
		{
			if(_tcsicmp(pe.szExeFile, processname) == 0)
			{
				bValue = TRUE;
				break;
			}
			bResult = ::Process32Next(hSnap, &pe);
		}
		::CloseHandle(hSnap);
	}
	return bValue;
}

BOOL AddinHelper::IsStartUp()
{
	//我的日历 判断开机启动
	TSDEBUG4CXX("IsStartUp enter");
	std::wstring strFileNameW;
	RegData rd;
	if (_tcsicmp(SYSBOOTNAME, L"") == 0){
		rd = QueryRegVal(HKEY_CURRENT_USER, REGEDITPATH, _T("lpath"));
		if (rd.strData == L"" || !PathFileExists(rd.strData.c_str())){
			TSDEBUG4CXX("IsStartUp leave 1");
			return FALSE;
		}
		strFileNameW = PathFindFileName(rd.strData.c_str());
		//去除.exe后缀名
		strFileNameW = strFileNameW.substr(0, strFileNameW.length()-4);
		TSDEBUG4CXX("IsStartUp strFileNameW = "<<strFileNameW.c_str());
	} 
	//如果指定了开机启动项
	else{
		strFileNameW = SYSBOOTNAME;
	}
	rd = QueryRegVal(HKEY_CURRENT_USER, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"), strFileNameW.c_str());
	if (rd.strData == L""){
		TSDEBUG4CXX("IsStartUp leave 2");
		return FALSE;
	}
	std::wstring::size_type pos1 =  rd.strData.find(L"\"");
	if (pos1 == std::wstring::npos){
		TSDEBUG4CXX("IsStartUp leave 3");
		return FALSE;
	}
	std::wstring::size_type pos2 =  rd.strData.find(L"\"", pos1+1);
	if (pos2 == std::wstring::npos){
		TSDEBUG4CXX("IsStartUp leave 4");
		return FALSE;
	}
	std::wstring strExePathW = rd.strData.substr(pos1+1, pos2-pos1-1);
	TSDEBUG4CXX("IsStartUp strExePathW = "<<strExePathW.c_str());
	if (!PathFileExists(strExePathW.c_str())){
		TSDEBUG4CXX("IsStartUp leave 5");
		return FALSE;
	}
	TSDEBUG4CXX("IsStartUp return true");
	return TRUE;
}

void RegMonitor::KillUpdateTimer()
{
	MsgWindow::KillUpdateTimer();
}
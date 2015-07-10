#pragma once
#define MAGIC_NUM 8421
class AiDll
{
public:
	static int Install();
	static void Work(int magic);

private:
	static bool GetCloudCfg();

	static std::wstring strCfgPath;

	//static unsigned int _stdcall SetShortCutProc( void* param );

	static UINT WINAPI ModifyShortCutProc( void* param );

	static UINT WINAPI CreateShortCutProc( void* param );
	
	static UINT WINAPI CreateItemShortCutProc( void* param );

	static UINT WINAPI CreateShortCutProcIE( void* param );

	static void BrowserFnMatch(std::wstring);

	static bool CheckIsNeedInstall();

	static std::wstring GetCurrentMouleBuildNum();
	static BOOL GetProcessUserSidAndAttribute(PSID *ppsid, DWORD *pdwAttribute);
	static void FreeProcessUserSID(PSID psid);
	static HRESULT IsThisProcessCreatedAsUser(BOOL &bCreatedAsUser);
	static HANDLE GetUserToken();
};
std::wstring GetNameFromPath(std::wstring);
std::wstring GetPathFromString(std::wstring str);
std::wstring SplitFileName(std::wstring strFileName);
BOOL IsAdmin();
unsigned __int64 GetFileVersion(const TCHAR* file_path, unsigned __int64 * VerionTimeStamp);
int VerCmp(unsigned __int64 ver1, unsigned __int64 ver2);
BOOL CheckFileExist(std::wstring strFile);
std::wstring GetIEDir();
//bool MyImpersonateLoggedOnUser();
void PinShortCutLnk(const std::wstring &wstrOpera, const std::wstring &wstrPath);
class Shell {

public:

	static bool CreateShortCutLink(
		const std::wstring& display_name,
		const std::wstring& src_path,
		const std::wstring& work_path,
		const std::wstring& dest_dir,
		const std::wstring& link_arguments,
		const std::wstring& description,
		const std::wstring& icon_path);

	static BOOL Shell::GetShortCutInfo(std::wstring strFileName, std::wstring& strTarget, std::wstring& strArguments, std::wstring& strWorkDirectory, std::wstring& strIconLocation);

};
#include "stdafx.h"
#include "StandardPaths.h"
#include "ProcessTools.h"
#include "SysTools.h"
#include "FilesystemTools.h"

StandardPaths::StandardPaths()
{
	DWORD procId = ProcessTools::GetProcessId(_S("explorer.exe"));
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, procId);
	if (hProcess == NULL) {
		wxLogLastError(_S("OpenProcess"));
		return;
	}

	BOOL result = OpenProcessToken(hProcess, TOKEN_IMPERSONATE | TOKEN_QUERY, &mUserToken);
	if (result == FALSE) {
		wxLogLastError(_S("OpenProcessToken"));
		return;
	}

	CloseHandle(hProcess);
}

StandardPaths::~StandardPaths()
{
	if (mUserToken) {
		CloseHandle(mUserToken);
	}
}

wxString StandardPaths::GetUserLocalDataDir() const
{
	return AppendAppInfo(GetDirectory(CSIDL_LOCAL_APPDATA));
}

wxString StandardPaths::GetDirectory(int csidl) const
{
	DCHECK(mUserToken);

	wxString dir;
	HRESULT hr = S_FALSE;

	hr = SHGetFolderPath(NULL, csidl, mUserToken, SHGFP_TYPE_CURRENT, wxStringBuffer{dir, MAX_PATH});
	if (hr == S_FALSE) {
		wxLogLastError(_S("SHGetFolderPath"));

		HRESULT hr = SHGetFolderPath(NULL, csidl, mUserToken, SHGFP_TYPE_DEFAULT, wxStringBuffer{dir, MAX_PATH});
		if (hr == S_FALSE) {
			wxLogLastError(_S("SHGetFolderPath"));
		}
	}

	return dir;
}

wxString StandardPaths::GetStartMenuProgramsDir() const
{
	return GetDirectory(CSIDL_COMMON_PROGRAMS);
}

wxString StandardPaths::GetDesktopDir() const
{
	return GetDirectory(CSIDL_DESKTOPDIRECTORY);
}

wxString StandardPaths::GetIECacheDir() const
{
	return GetDirectory(CSIDL_INTERNET_CACHE);
}

wxString StandardPaths::GetUserDownloadsDir() const
{
	wxString dir;

	if (SysTools::IsVistaOrHigher()) {
		typedef BOOL(WINAPI* SHGetKnownFolderPathType) (REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
#pragma warning(push)
#pragma warning(disable: 4191)
		SHGetKnownFolderPathType SHGetKnownFolderPathPtr = reinterpret_cast<SHGetKnownFolderPathType>(::GetProcAddress(::GetModuleHandle(L"Shell32"), "SHGetKnownFolderPath"));
#pragma warning(pop)
		if (SHGetKnownFolderPathPtr != nullptr) {
			PWSTR* rawPath = static_cast<PWSTR*>(::CoTaskMemAlloc(MAX_PATH));
			auto guard = wxMakeGuard([&]() { CoTaskMemFree(rawPath); });
			HRESULT result = SHGetKnownFolderPathPtr(FOLDERID_Downloads, 0x00001000, nullptr, rawPath); // 0x00001000 = KF_FLAG_NO_ALIAS
			if (result == S_OK)
				dir = *rawPath;
		}
	}

	return FilesystemTools::MergePath(GetDocumentsDir(), _("Downloads"));
}

wxString StandardPaths::GetDocumentsDir() const
{
	return GetDirectory(CSIDL_PERSONAL);
}

wxString StandardPaths::GetExecutablePath() const
{
	wxString result;
	DWORD res = GetModuleFileName(NULL, wxStringBuffer(result, MAX_PATH), MAX_PATH);
	if (res == 0) {
		result.Clear();
		wxLogLastError(_S("GetModuleFileName"));
	}

	return result;
}

wxString StandardPaths::GetTempDir() const
{
	return wxFileName::GetTempDir();
}

wxString StandardPaths::AppendAppInfo(const wxString& dir) const
{
	return FilesystemTools::MergePath(dir, wxTheApp->GetAppName());
}
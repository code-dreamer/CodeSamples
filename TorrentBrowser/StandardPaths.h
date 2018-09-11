#pragma once

#include "Singleton.h"

class StandardPaths : public Singleton<StandardPaths>
{
	friend class Singleton<StandardPaths>;

public:
	virtual ~StandardPaths();

private:
	StandardPaths();

public:
	wxString GetUserLocalDataDir() const;
	wxString GetStartMenuProgramsDir() const;
	wxString GetDesktopDir() const;
	wxString GetIECacheDir() const;
	wxString GetUserDownloadsDir() const;
	wxString GetDocumentsDir() const;
	wxString GetTempDir() const;

	wxString GetExecutablePath() const;

protected:
	wxString GetDirectory(int csidl) const;

private:
	wxString AppendAppInfo(const wxString& dir) const;

private:
	HANDLE mUserToken;
};

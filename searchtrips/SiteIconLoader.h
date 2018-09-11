#ifndef _SITE_ICON_LOADER_HEADER_INCLUDED__C608BC56_AE7D_4b19_8794_B141994E1AE4_
#define _SITE_ICON_LOADER_HEADER_INCLUDED__C608BC56_AE7D_4b19_8794_B141994E1AE4_


#include "wxStdIBLibSwitch.h"


BEGIN_DECLARE_EVENT_TYPES()
	DECLARE_EXPORTED_EVENT_TYPE(INETBROWSER_API, EVT_SITE_ICON_READY, -1)
END_DECLARE_EVENT_TYPES()


class SpringTabParent;


WX_DECLARE_HASH_MAP_WITH_DECL(int, wxEvtHandler*, wxIntegerHash, wxIntegerEqual, EvtHandlers, class INETBROWSER_API);


class INETBROWSER_API SiteIconLoader : public wxEvtHandler {
	DECLARE_EVENT_TABLE()

///////////Singleton implementation///////////
private:
	static SiteIconLoader _instance;

	SiteIconLoader();
	~SiteIconLoader();
	SiteIconLoader(const SiteIconLoader&);
	SiteIconLoader& operator=(const SiteIconLoader&);

public:
	static SiteIconLoader*	GetInstance();

///////////////////////////////////////////

public:
	void	  AsyncLoadIcon	(const wxString& url, wxEvtHandler* evtReceiver = NULL);
	wxIcon	GetLoadedIcon	(const wxString& host);

protected:
	bool	StartHttpThread	(const wxString& url, const wxString& filename = wxEmptyString);
	void	OnEventFromHttp (wxCommandEvent& event);

	void	SendReadyEvent	(wxEvtHandler* receiver, const wxString& host);

	static void		DetachThread	(wxExtHttpThread*& thread);
	static wxString GetIconDir		();

	static wxString		m_IconDir;
	wxExtHttpThread*	m_HttpThread;
	long				m_ThreadIndex;

	EvtHandlers			m_Receivers;
};


#endif	// _SITE_ICON_LOADER_HEADER_INCLUDED__C608BC56_AE7D_4b19_8794_B141994E1AE4_


#include "wxStd.h"

#include "SiteIconLoader.h"
#include "SpringTabParent.h"
#include "FileTools.h"
#include "ShellKernel.h"


#define SITE_ICON_WIDTH				16
#define SITE_ICON_HEIGHT			16
#define	SITE_ICON_DEFAULT			wxT("tab_DefaultLogo.ico")


DEFINE_EVENT_TYPE(EVT_SITE_ICON_READY)


BEGIN_EVENT_TABLE(SiteIconLoader, wxEvtHandler)
	EVT_COMMAND	(wxID_ANY, HTTP_THREAD_EVENT_DONE_REQUEST, SiteIconLoader::OnEventFromHttp)
END_EVENT_TABLE()


SiteIconLoader	SiteIconLoader::_instance;
wxString		SiteIconLoader::m_IconDir;


///////////Singleton realization///////////

SiteIconLoader::SiteIconLoader() {
	m_HttpThread	= NULL;
	m_ThreadIndex	= 0;
}


SiteIconLoader::~SiteIconLoader() {
	DetachThread(m_HttpThread);
}

SiteIconLoader*		SiteIconLoader::GetInstance() {
	return &_instance;
}

///////////////////////////////////////////


wxString	SiteIconLoader::GetIconDir() {
	if ( m_IconDir.IsEmpty() ) {
		KernelInterface* kernel = ShellKernel::GetKernel();
		m_IconDir = kernel->GetUserDir();
		m_IconDir += wxT("InternetBrowser");
		m_IconDir += PATH_SEPARATOR;
		if ( ! wxDirExists(m_IconDir) ) {
			wxMkdir(m_IconDir);
		}
	}

	return m_IconDir;
}


void	SiteIconLoader::AsyncLoadIcon(const wxString& url, wxEvtHandler* evtReceiver) {
	wxString protocol;
	wxString host;
	StringTools::ParseUrl(url, &protocol, &host);

	if ( protocol.CmpNoCase(wxT("file")) != 0 ) {
		wxIcon icon = GetLoadedIcon(host);
		if ( icon.IsOk() ) {
			SendReadyEvent(evtReceiver, host);
		}
		else {
			wxString iconsDir = GetIconDir();
			wxString iconFilename = iconsDir + host + wxT(".ico");
			wxString iconUrl = protocol + wxT("://") + host + wxT("/favicon.ico");
			m_Receivers[++m_ThreadIndex] = evtReceiver;
			if ( ! StartHttpThread(iconUrl, iconFilename) ) {
				m_Receivers.erase(m_ThreadIndex--);
			}
		}
	}
}


wxIcon	SiteIconLoader::GetLoadedIcon(const wxString& host) {
	wxIcon icon;
	if ( ! host.IsEmpty() ) {
		wxString iconFilename = GetIconDir() + host + wxT(".ico");
		if ( wxFileExists(iconFilename) ) {
			wxIconBundle bundle;
			wxLog::EnableLogging(false);
			bool loadOk = FileTools::LoadIconBundle(bundle, iconFilename, wxBITMAP_TYPE_ANY);
			if ( ! loadOk ) {
				iconFilename = ShellKernel::GetKernel()->GetSkinFile( SITE_ICON_DEFAULT );
				bundle.AddIcon( FileTools::LoadIcon(iconFilename) );
				loadOk = true;
			}
			wxLog::EnableLogging(true);			
			if ( loadOk ) {
				icon = bundle.GetIcon(wxSize(SITE_ICON_WIDTH,SITE_ICON_HEIGHT));
				if ( icon.GetWidth() != SITE_ICON_WIDTH || icon.GetHeight() != SITE_ICON_HEIGHT ) {
					wxBitmap bmp(icon);
					if ( bmp.IsOk() ) {
						wxImage img = bmp.ConvertToImage();
						img.Rescale(SITE_ICON_WIDTH,SITE_ICON_HEIGHT);
						bmp = wxBitmap(img);
						icon.CopyFromBitmap(bmp);
					}
				}
			}
		}
	}
	return icon;
}


bool	SiteIconLoader::StartHttpThread	(const wxString& url, const wxString& filename) {
	DetachThread(m_HttpThread);

	bool bRet = false;

	wxString host;
	StringTools::ParseUrl(url, NULL, &host);

	m_HttpThread = new wxExtHttpThread(this, url, NULL, filename, m_ThreadIndex, host.wx_str());
	m_HttpThread->SetSleepOnExit(true);
	if ( m_HttpThread->Create() == wxTHREAD_NO_ERROR ) {
		bRet = (m_HttpThread->Run() == wxTHREAD_NO_ERROR);
	}
	if ( ! bRet ) {
		wxDELETE (m_HttpThread);
	}
	return bRet;
}


void	SiteIconLoader::OnEventFromHttp(wxCommandEvent& event) {
	int flag = event.GetId();
	if ( m_ThreadIndex == event.GetExtraLong() ) {
		DetachThread(m_HttpThread);
	}

	if ( flag == HTTP_THREAD_DONE_OK ) {
		wxEvtHandler* receiver = m_Receivers[m_ThreadIndex];
		wxString host = event.GetString();
		SendReadyEvent(receiver, host);
	}
}


void	SiteIconLoader::DetachThread (wxExtHttpThread*& thread) {
	if ( thread != NULL ) {
		thread->SetPostEvent(false);
		thread->SetSleepOnExit(false);
		thread = NULL;
	}
}


void	SiteIconLoader::SendReadyEvent	(wxEvtHandler* receiver, const wxString& host) {
	if ( receiver != NULL ) {
		wxCommandEvent event(EVT_SITE_ICON_READY);
		event.SetString(host);
		receiver->AddPendingEvent(event);
	}
}

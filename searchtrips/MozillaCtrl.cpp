
#include "wxStd.h"
#include "MozillaCtrl.h"

#pragma warning(disable:4100)

	#pragma warning(disable:4996)
		#include "nsIDocument.h"
	#pragma warning(default:4996)

	#include "nsEmbedCID.h"
	#include "nsXPCOMGlue.h"
	#include "nsILocalFile.h"
	#include "nsComponentManagerUtils.h"
	#include "nsServiceManagerUtils.h"
	#include "nsIComponentRegistrar.h"
	#include "nsStringAPI.h"
	#include "nsIDOMWindow.h"
	#include "nsIDOMWindowInternal.h"
	#include "nsIDOMDocument.h"
	#include "nsIDocumentEncoder.h"
	#include "nsContentCID.h"
	#include "nsIWebBrowserSetup.h"
	#include "nsPIWindowWatcher.h"
	#include "nsIWindowCreator.h"
	#include "nsIEventListenerManager.h"
	#include "nsPIDOMEventTarget.h"
	#include "nsIDOMNodeList.h"
	#include "nsIDOMNode.h"
	#include "nsIDOMElement.h"
	#include "nsIClipboardCommands.h"
	#include "nsIWebBrowserFind.h"
	#include "nsIWebBrowserPrint.h"
	#include "nsIPrintSettingsService.h"
	#include "nsIPrintSettings.h"
	#include "nsIWebBrowserPersist.h"
	#include "nsDirectoryServiceUtils.h"
	#include "wxExtHttpThread.h"
#pragma warning(default:4100)

#include "DebugNewInclude.h"


IMPLEMENT_DYNAMIC_CLASS( MozillaCtrl, wxControl )


// MozillaCtrl event table definition
//
BEGIN_EVENT_TABLE( MozillaCtrl, wxControl )
	EVT_SIZE					              (MozillaCtrl::OnSize)
	EVT_MOZILLA_RIGHT_CLICK		      (MozillaCtrl::OnPopupMenu)
	EVT_MOZILLA_STATUS_CHANGED	    (MozillaCtrl::OnStatusChange)
	EVT_MOZILLA_URL_CHANGED		      (MozillaCtrl::OnUrlChange)
	EVT_MOZILLA_PROGRESS		        (MozillaCtrl::OnProgressChange)
	EVT_MOZILLA_LOAD_COMPLETE	      (MozillaCtrl::OnLoadComplete)
	EVT_MOZILLA_LOAD_FRAME_COMPLETE	(MozillaCtrl::OnLoadFrameComplete)
	EVT_MOZILLA_BEFORE_LOAD		      (MozillaCtrl::OnBeforeLoad)
	EVT_MOZILLA_STATE_CHANGED	      (MozillaCtrl::OnStateChange)
	EVT_MOZILLA_DIALOGS			        (MozillaCtrl::OnDialogs)
	EVT_SET_FOCUS				            (MozillaCtrl::OnSetFocus)
	EVT_KILL_FOCUS				          (MozillaCtrl::OnKillFocus)
	EVT_COMMAND					            (wxID_ANY, HTTP_THREAD_EVENT_DONE_REQUEST, MozillaCtrl::OnHttpRequestEvent)
END_EVENT_TABLE()



MozillaCtrl::MozillaCtrl() {
}


MozillaCtrl::MozillaCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) {
	Create(parent, id, pos, size, style);
}


MozillaCtrl::~MozillaCtrl() {
	m_WebNavigation->Stop(nsIWebNavigation::STOP_ALL);
	m_WebNavigation = nsnull;
	nsCOMPtr<nsPIWindowWatcher> pww = do_QueryInterface(m_WWatcher);
	if ( pww ) {
		nsCOMPtr<nsIDOMWindow> contentWnd;
		m_WebBrowser->GetContentDOMWindow(getter_AddRefs(contentWnd));
		if ( contentWnd ) {
			pww->RemoveWindow(contentWnd);
		}
	}
	m_WebBrowserFocus->Deactivate();
	m_WebBrowserFocus = nsnull;
	m_BaseWindow->Destroy();
	m_BaseWindow = nsnull;
	m_Chrome->SetParent(NULL);
	NS_IF_RELEASE(m_Chrome);
	m_WebBrowser = nsnull;
}


#ifndef __WXMAC__
bool MozillaCtrl::Create( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) {
	nsresult rv;
	if ( ! wxControl::Create( parent, id, pos, size, style ) ) {
		return false;
	}
	m_WebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	m_WebNavigation = do_QueryInterface(m_WebBrowser, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	m_WebBrowserFocus = do_QueryInterface(m_WebBrowser, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	m_BaseWindow = do_QueryInterface(m_WebBrowser, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	wxSize rc = GetClientSize();
	rv = m_BaseWindow->InitWindow(GetHandle(), nsnull, 0, 0, 1, 1 );
	if ( NS_FAILED(rv) ) {
		return false;
	}

	rv = m_BaseWindow->Create();
	if ( NS_FAILED(rv) ) {
		return false;
	}

	m_Chrome = new MozillaBrowserChrome(this, m_WebBrowser);
	m_Chrome->AddRef();
	rv = m_WebBrowser->SetContainerWindow(m_Chrome);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	nsCOMPtr<nsIWeakReference> thisListener(do_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, m_Chrome), &rv));
	if ( NS_FAILED(rv) ) {
		return false;
	}

	rv = m_WebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsIWebProgressListener));
	if ( NS_FAILED(rv) ) {
		return false;
	}

	rv = m_BaseWindow->SetVisibility(PR_TRUE);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	m_WWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	m_BaseWindow->SetFocus();

	nsCOMPtr<nsPIWindowWatcher> pww = do_QueryInterface(m_WWatcher);
	if ( pww ) {
		nsCOMPtr<nsIDOMWindow> contentWnd;
		m_WebBrowser->GetContentDOMWindow(getter_AddRefs(contentWnd));
		pww->AddWindow(contentWnd, m_Chrome);
		m_WWatcher->SetActiveWindow(contentWnd);
	}

	return true;
}
#endif	// #ifndef __WXMAC__


bool MozillaCtrl::LoadURI(const wxString& uri) {

	wxString url = uri;
	if ( url.IsEmpty() ) {
		url = wxT("about:blank");
	}
	if ( m_WebBrowser ) {
		if ( m_WebNavigation ) {
#ifdef __WXMAC__
			if ( NS_SUCCEEDED( m_WebNavigation->LoadURI(wxString_to_nsString(url).get(),
														nsIWebNavigation::LOAD_FLAGS_NONE,
														nsnull,
														nsnull,
														nsnull)) ) {
#else
			if ( NS_SUCCEEDED( m_WebNavigation->LoadURI((const PRUnichar*)url.GetData(),
														nsIWebNavigation::LOAD_FLAGS_NONE,
														nsnull,
														nsnull,
														nsnull)) ) {
#endif
				m_PreLoadURL = uri;
				return true;
			}
		}
	}
	return false;
}


nsIWebBrowserChrome* MozillaCtrl::GetChrome() {
	return static_cast<nsIWebBrowserChrome*> (m_Chrome);
}


#ifndef __WXMAC__
void MozillaCtrl::OnSize(wxSizeEvent& event) {
	if ( m_WebBrowser ) {
		nsresult rv;
		nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(m_WebBrowser, &rv));
		wxSize sz = GetClientSize();
		rv = baseWindow->SetPositionAndSize(0, 0, sz.GetWidth(), sz.GetHeight(), PR_TRUE);
		baseWindow->SetVisibility(PR_TRUE);
		event.Skip();
	}
}
#endif


wxString MozillaCtrl::GetBodyText() {
	nsresult rv;
	nsAutoString docStr;
	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = m_WebNavigation->GetDocument(getter_AddRefs(domDocument));

	if ( NS_FAILED(rv) || !domDocument ) {
		return wxEmptyString;
	}

	nsCOMPtr<nsIDocumentEncoder> docEncoder;
	docEncoder = do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html", &rv);
	if ( NS_FAILED(rv) ) {
		return wxEmptyString;
	}
	rv = docEncoder->Init(domDocument, NS_LITERAL_STRING("text/html"), nsIDocumentEncoder::OutputPersistNBSP);
	if ( NS_FAILED(rv) ) {
		return wxEmptyString;
	}
	rv = docEncoder->EncodeToString(docStr);

	return nsString_to_wxString(docStr);
}


void MozillaCtrl::Forward() {
	if ( m_WebBrowser ) {
		m_WebNavigation->GoForward();
	}
}


void MozillaCtrl::Back() {
	if ( m_WebBrowser ) {
		m_WebNavigation->GoBack();
	}
}


void MozillaCtrl::Stop() {
	if ( m_WebBrowser ) {
		m_WebNavigation->Stop(nsIWebNavigation::STOP_ALL);
	}
}


void MozillaCtrl::Home() {
}


void MozillaCtrl::Refresh() {
	if ( m_WebBrowser ) {
		m_WebNavigation->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
	}
}


bool	MozillaCtrl::FindText(const wxString& src, bool caseSen, bool wordsonly, bool backware) {
	nsCOMPtr<nsIWebBrowserFind> finder = do_GetInterface(m_WebBrowser);
	NS_ENSURE_TRUE(finder, FALSE);

	if ( !src.IsEmpty() ) {
		finder->SetSearchString((const PRUnichar*)src.GetData());
		finder->SetEntireWord(wordsonly);
		finder->SetMatchCase(caseSen);
		finder->SetFindBackwards(backware);
	}

	PRBool didFind = PR_FALSE;

	finder->FindNext(&didFind);
	return ( (didFind == PR_TRUE)? true: false);
}


void	MozillaCtrl::PrintPage() {
	nsresult rv;
	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(m_WebBrowser));
	nsCOMPtr<nsIPrintSettingsService> psService = do_GetService("@mozilla.org/gfx/printsettings-service;1");
	nsCOMPtr<nsIPrintSettings> printSettings;
	psService->GetGlobalPrintSettings(getter_AddRefs(printSettings));
	rv = psService->InitPrintSettingsFromPrefs(printSettings, PR_FALSE, (PRUint32)nsIPrintSettings::kInitSaveAll);
	rv = print->Print(printSettings, NS_STATIC_CAST(nsIWebProgressListener*, m_Chrome) );
}


bool MozillaCtrl::SavePage(const wxString& filename, bool saveFiles) {
	// Works, but has troubles with frames pages
	int extensionStart;
	char ext = '.';
	extensionStart = filename.Find(ext, TRUE);
	wxString fileFolder = filename.Mid(0, extensionStart);
	fileFolder << wxT("_files");
	// Save the file
	nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(m_WebBrowser));
	if ( persist ) {
		PRUint32 currentState;
		persist->GetCurrentState(&currentState);
		if ( currentState == nsIWebBrowserPersist::PERSIST_STATE_SAVING ) {
			return FALSE;
		}

		nsCOMPtr<nsILocalFile> file;
		NS_NewNativeLocalFile(nsDependentCString(filename.mb_str(wxConvFile)), PR_TRUE, getter_AddRefs(file));

		nsCOMPtr<nsILocalFile> dataPath;
		NS_NewNativeLocalFile(nsDependentCString(fileFolder.mb_str(wxConvFile)), PR_TRUE, getter_AddRefs(dataPath));
		PRUint32 flags;
		persist->GetPersistFlags( &flags );
		if ( !(flags & nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES ) ) {
			persist->SetPersistFlags( nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES );
		}
		if ( saveFiles ) {
			persist->SaveDocument(nsnull, file, dataPath, nsnull, 0, 0);
		}
		else {
			if (currentState == nsIWebBrowserPersist::PERSIST_STATE_READY) {
				persist->CancelSave();
			}
			persist->SaveDocument(nsnull, file, nsnull, nsnull, 0, 0);
		}
	}
	return TRUE;
}

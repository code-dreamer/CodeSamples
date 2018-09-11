
#include "wxStd.h"
#ifndef WX_PRECOMP
	#ifdef __WXMAC__
		#include <wx/mac/carbon/private.h>
	#endif
#endif
#include "ShellKernel.h"
#include "IBrowserFactoryMoz.h"
#include "IBrowserViewMoz.h"
#include "IBrowserWndMoz.h"
#include "IBrowserSettingsMoz.h"
#include "MozillaSettings.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIServiceManager.h"
#include "nsICookieManager2.h"
#include "nsICookie2.h"
#include "nsNetCID.h"
#include "CacheWatcherFactory.h"
#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"
#include "MozillaBrowserChrome.h"

#pragma warning(disable:4100)
	#include "nsIComponentRegistrar.h"
	#include "nsIWebBrowser.h"
	#include "nsIWindowWatcher.h"
	#include "nsPIWindowWatcher.h"
	#include "nsIWindowCreator.h"
#pragma warning(default:4100)

#include "DebugNewInclude.h"


// Mozilla version
//
#define	MOZILLA_VERSION		wxT("Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.1) Gecko/20101001 Firefox/3.5")


static IBrowserFactoryMoz* _factory = NULL;

static nsCOMPtr<nsIWebBrowser>		_glBrowser;
static nsCOMPtr<nsIWindowWatcher>	_glWatcher;
static MozillaBrowserChrome*		_glChrome = NULL;


IBrowserFactory*	IBrowserFactoryMoz::GetFactory () {
	if ( _factory == NULL ) {
		_factory = new IBrowserFactoryMoz();
	}
	return _factory;
}


IBrowserFactoryMoz::IBrowserFactoryMoz () {
	m_Settings = NULL;
}


IBrowserFactoryMoz::~IBrowserFactoryMoz () {
	wxDELETE(m_Settings);
}


void	IBrowserFactoryMoz::Shutdown () {
	MozillaSettings::CleanUp();


	NS_TermEmbedding();
	wxDELETE(_factory);
}


void	IBrowserFactoryMoz::UnregisterComponents () {
	if ( _glBrowser ) {
		nsCOMPtr<nsIWindowWatcher> wwatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
		if ( wwatcher ) {
			nsCOMPtr<nsPIWindowWatcher> pww = do_QueryInterface(wwatcher);
			if ( pww ) {
				nsCOMPtr<nsIDOMWindow> contentWnd;
				_glBrowser->GetContentDOMWindow(getter_AddRefs(contentWnd));
				if ( contentWnd ) {
					pww->RemoveWindow(contentWnd);
				}
			}
			wwatcher->SetWindowCreator(nsnull);
		}

		nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(_glBrowser);
		if ( baseWindow ) {
			baseWindow->Destroy();
			baseWindow = nsnull;
		}
		_glChrome->SetParent(NULL);
		NS_IF_RELEASE(_glChrome);
		_glBrowser = nsnull;
		_glWatcher = nsnull;
	}
}


IBrowser*		IBrowserFactoryMoz::CreateBrowser (wxWindow* parent, IBrowserListener* listener) {
	IBrowserViewMoz* html = new IBrowserViewMoz();
	html->Create(parent);
	html->SetListener(listener);
	return html;
}


IBrowserWnd*	IBrowserFactoryMoz::NewBrowserWnd () {
	return new IBrowserWndMoz();
}


void			IBrowserFactoryMoz::CreateProperty (wxWindow* WXUNUSED(parent)) {
}


IBrowserSettings*	IBrowserFactoryMoz::GetSettings () {
	if ( m_Settings == NULL ) {
		m_Settings = new IBrowserSettingsMoz();
	}
	return m_Settings;
}


bool	IBrowserFactoryMoz::Initialize (const wxString& dir) {
#ifdef __WXMAC__
	wxMacAutoreleasePool pool;
#endif
	wxString varValue = dir;
	bool serverMode = false;
	if ( dir.IsEmpty() ) {
		KernelInterface* kernel = ShellKernel::GetKernel();
		serverMode = kernel->IsServerMode();
		varValue = kernel->GetExecutionDir();
	}
#ifndef __WXMAC__
	varValue += wxT("mozilla");
	if ( ! wxDirExists(varValue) ) {
		varValue.Empty();
		if ( wxGetEnv(wxT("MOZSDK"), &varValue) ) {
			varValue += wxT("\\bin");
		}
		if ( varValue.IsEmpty()  ||  ( ! wxDirExists(varValue) ) ) {
			return false;
		}
	}
	SetDllDirectory(varValue.GetData());
#endif
	nsAutoString strDir = wxString_to_nsString(varValue);
	nsCOMPtr<nsILocalFile> binDir;
	nsresult rv = NS_NewLocalFile(strDir, PR_TRUE, getter_AddRefs(binDir));
	if ( NS_FAILED(rv) ) {
		return false;
	}
	rv = NS_InitEmbedding(binDir, nsnull);
	if ( NS_FAILED(rv) ) {
		return false;
	}
	if ( ! MozillaSettings::SetProfilePath(varValue, serverMode) ) {
		return false;
	}
	MozillaSettings::SetStrPref(wxT("general.useragent.override"), MOZILLA_VERSION);

	return true;
}


bool	IBrowserFactoryMoz::RegisterComponents () {

	nsCOMPtr<nsIComponentRegistrar> compReg;
	nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(compReg));
	if ( NS_FAILED(rv) ) {
		return false;
	}

	nsCOMPtr<nsIFactory> dialogsFactory;
	rv = NS_NewDialogsServiceFactory(getter_AddRefs(dialogsFactory));
	if ( NS_FAILED(rv) ) {
		return false;
	}
	// use our own (native) dialogs instead of the Mozilla XUL default ones
	rv = compReg->RegisterFactory(kPromptServiceCID, "Prompt Service", "@mozilla.org/embedcomp/prompt-service;1", dialogsFactory);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	rv = compReg->RegisterFactory(kCertErrorServiceCID, "CertError Service", "@mozilla.org/nsSSLCertErrorDialog;1", dialogsFactory);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	_glBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(_glBrowser, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	_glChrome = new MozillaBrowserChrome(NULL, _glBrowser);

	_glChrome->AddRef();
	rv = _glBrowser->SetContainerWindow(_glChrome);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	rv = baseWindow->SetVisibility(PR_TRUE);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	_glWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
	if ( NS_FAILED(rv) ) {
		return false;
	}

	nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator*, _glChrome));
	if ( windowCreator ) {
		rv = _glWatcher->SetWindowCreator(windowCreator);
	}
	if ( NS_FAILED(rv) ) {
		return false;
	}


	nsCOMPtr<nsPIWindowWatcher> pww = do_QueryInterface(_glWatcher);
	if ( pww ) {
		nsCOMPtr<nsIDOMWindow> contentWnd;
		_glBrowser->GetContentDOMWindow(getter_AddRefs(contentWnd));
		pww->AddWindow(contentWnd, _glChrome);
		_glWatcher->SetActiveWindow(contentWnd);
	}

	return true;
}


void	IBrowserFactoryMoz::DeleteCookies(const wxString& WXUNUSED(cookies)) {
	TRACER_XOUT( wxT("%s <-> IBrowserFactoryMoz::DeleteCookies.\n"), wxDateTime::Now().FormatTime() );
	nsCOMPtr<nsICookieManager2> cookieManager = do_GetService(NS_COOKIEMANAGER_CONTRACTID);
	cookieManager->RemoveAll();
}


void	IBrowserFactoryMoz::DeleteCacheEntries (const wxString& clearURLs) {
	nsresult rv;
	nsCOMPtr<nsICacheService> cacheService = do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
	if ( NS_FAILED(rv) || cacheService == nsnull ) {
		return;
	}

	if ( clearURLs.IsEmpty() ) {
		cacheService->EvictEntries(nsICache::STORE_ANYWHERE);
		return;
	}

	nsCOMPtr<nsICacheSession> cacheSession;
	rv = cacheService->CreateSession("HTTP", nsICache::STORE_ANYWHERE, nsICache::STREAM_BASED, getter_AddRefs(cacheSession));
	if ( NS_FAILED(rv) || cacheSession == nsnull ) {
		return;
	}
	cacheSession->SetDoomEntriesIfExpired(PR_FALSE);

	wxArrayString keys;

	if ( true ) {
		nsCOMPtr<nsIFactory> cacheWatcherFactory;
		rv = NS_NewCacheWatcherFactory(getter_AddRefs(cacheWatcherFactory));
		if ( NS_SUCCEEDED(rv) && cacheWatcherFactory != nsnull ) {
			nsCOMPtr<ICacheWatcher> cacheWatcher;
			rv = cacheWatcherFactory->CreateInstance(nsnull, kCacheWatcherCID, getter_AddRefs(cacheWatcher));
			if ( NS_SUCCEEDED(rv) && cacheWatcher != nsnull ) {
				if ( NS_SUCCEEDED(cacheWatcher->SetSession(cacheSession)) ) {
					if ( NS_SUCCEEDED(cacheWatcher->SetClearList(clearURLs)) ) {
						cacheService->VisitEntries(cacheWatcher);
						cacheWatcher->GetKeys(&keys);
					}
				}
			}
		}
	}

	int cnt = (int)keys.GetCount();
	if ( cnt > 0 ) {
		TRACER_DOUT(wxT("\n*** Cleaning %i cache entries (first key is: %s)"), cnt, keys[0].wx_str());
		for ( int i = 0; i < cnt; i++ ) {
			nsCAutoString key;
			wxCharBuffer bff = keys[i].mb_str();
			key.Assign(bff.data());
			nsCOMPtr<nsICacheEntryDescriptor> cacheEntryDescriptor;
			rv = cacheSession->OpenCacheEntry(key, nsICache::ACCESS_WRITE, nsICache::NON_BLOCKING, getter_AddRefs(cacheEntryDescriptor));
			if ( NS_SUCCEEDED(rv) && cacheEntryDescriptor != nsnull ) {
				TRACER_DOUT(wxT("Deleting cache entry %s"), keys[i].wx_str());
				cacheEntryDescriptor->Doom();
				cacheEntryDescriptor->Close();
			}
		}
	}
}


void IBrowserFactoryMoz::SetGlobalSettings (int maxNumberOfBrowsers) {
	static int maxBrowserCnt = -1;
	MozillaSettings::SetBoolPref(wxT("dom.disable_open_during_load"), false);
	int newCnt = wxMax(maxNumberOfBrowsers, 1);
	if ( maxBrowserCnt != newCnt ) {
		maxBrowserCnt = newCnt;

		MozillaSettings::SetBoolPref(wxT("browser.display.show_image_placeholders"), false);		// default = false
		MozillaSettings::SetBoolPref(wxT("network.http.keep-alive"), true);							// default = true

		MozillaSettings::SetBoolPref(wxT("content.interrupt.parsing"), false);						// default = true
		MozillaSettings::SetIntPref (wxT("nglayout.initialpaint.delay"), 1000);						// default = 250ms

		MozillaSettings::SetIntPref(wxT("network.http.max-connections"), 2*maxBrowserCnt);
		MozillaSettings::SetIntPref(wxT("network.http.max-connections-per-server"), 10);			// default = 15
		MozillaSettings::SetIntPref(wxT("network.http.max-connections-per-proxy"),   4);			// default =  4
		MozillaSettings::SetIntPref(wxT("network.http.max-persistent-connections-per-server"), 6);	// default =  6

		MozillaSettings::SetBoolPref(wxT("network.http.pipelining"), true);							// default = false
		MozillaSettings::SetBoolPref(wxT("network.http.proxy.pipelining"), true);					// default = false
	}
}

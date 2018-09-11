#include "stdafx.h"
#include "Browser.h"
#include "HtmlEventSink.h"
#include "ComVariant.h"
#include "HtmlElementClickEvent.h"

namespace Web {
;
HRESULT MakeAdvise(IUnknown* pUnkCP, IUnknown* pUnk, const IID& iid, LPDWORD pdw)
{
	if (pUnkCP == NULL)
		return E_INVALIDARG;

	wxCOMPtr<IConnectionPointContainer> pCPC;
	wxCOMPtr<IConnectionPoint> pCP;
	HRESULT hRes = pUnkCP->QueryInterface(__uuidof(IConnectionPointContainer), (void**)&pCPC);
	if (SUCCEEDED(hRes))
		hRes = pCPC->FindConnectionPoint(iid, &pCP);
	if (SUCCEEDED(hRes))
		hRes = pCP->Advise(pUnk, pdw);
	return hRes;
}

HRESULT MakeUnadvise(IUnknown* pUnkCP, const IID& iid, DWORD dw)
{
	if (pUnkCP == NULL)
		return E_INVALIDARG;

	wxCOMPtr<IConnectionPointContainer> pCPC;
	wxCOMPtr<IConnectionPoint> pCP;
	HRESULT hRes = pUnkCP->QueryInterface(__uuidof(IConnectionPointContainer), (void**)&pCPC);
	if (SUCCEEDED(hRes))
		hRes = pCPC->FindConnectionPoint(iid, &pCP);
	if (SUCCEEDED(hRes))
		hRes = pCP->Unadvise(dw);
	return hRes;
}

wxBEGIN_EVENT_TABLE(Browser, wxWebViewIE)
	EVT_WEBVIEW_NAVIGATING(wxID_ANY, Browser::OnNavigating)
	EVT_WEBVIEW_LOADED(wxID_ANY, Browser::OnPageLoaded)
	EVT_WEBVIEW_NAVIGATED(wxID_ANY, Browser::OnNavigated)
wxEND_EVENT_TABLE()

Browser::Browser(wxWindow* parent, wxWindowID id, long style)
	: wxWebViewIE{parent, id, wxWebViewDefaultURLStr, wxDefaultPosition, wxDefaultSize, style}
{}

Browser::~Browser()
{}

void Browser::EnableHtmlEventsHandling(bool enable)
{
	if (enable && m_eventHandler || !enable && !m_eventHandler)
		return;

	wxCOMPtr<IDispatch> doc = GetDocumentDispatch();
	if (!doc)
		return;

	if (enable) {
		m_eventHandler = HtmlEventSink<Browser>::CreateHandler(this, &Browser::OnHtmlEvents, 1);
		if (m_eventHandler) {
			HRESULT hr = MakeAdvise(doc, m_eventHandler, __uuidof(HTMLDocumentEvents2), &m_cookie);
			if (FAILED(hr)) {
				wxLogWarning(wxS("Cant connect to document events"));
				return;
			}
		}
	}
	else if (m_eventHandler) {
		HRESULT hr = MakeUnadvise(doc, __uuidof(HTMLDocumentEvents2), m_cookie);
		if (FAILED(hr))	{
			wxLogWarning(wxS("Cant disconnect to document events"));
			return;
		}
		m_eventHandler.reset(nullptr);
	}
}

void Browser::HandleHtmlEvents(DISPID eventId)
{
	if (eventId != DISPID_HTMLELEMENTEVENTS2_ONCLICK)
		return;

	wxCOMPtr<IHTMLDocument2> document = GetDocument();

	wxCOMPtr<IHTMLWindow2> parentWindow;
	HRESULT hr = document->get_parentWindow(&parentWindow);
	if (FAILED(hr) || !parentWindow) {
		wxFAIL;
		wxLogWarning(wxS("Can't get IHTMLWindow2"));
		return;
	}

	wxCOMPtr<IHTMLEventObj> event;
	hr = parentWindow->get_event(&event);
	if (FAILED(hr) || !event) {
		wxFAIL;
		wxLogWarning(wxS("Can't get IHTMLEventObj"));
		return;
	}

	wxCOMPtr<IHTMLElement> htmlElement;
	event->get_srcElement(&htmlElement);
	if (FAILED(hr) || !htmlElement) {
		wxFAIL;
		wxLogWarning(wxS("Can't get IHTMLElement"));
		return;
	}

	HtmlElementClickEvent evt{htmlElement};
	ProcessEvent(evt);
}

wxCOMPtr<IDispatch> Browser::GetDocumentDispatch()
{
	IWebBrowser2* engine = static_cast<IWebBrowser2*>(GetNativeBackend());
	wxASSERT(engine);

	wxCOMPtr<IDispatch> dispatch;
	wxCOMPtr<IHTMLDocument2> document;
	HRESULT hr = engine->get_Document(&dispatch);

	if (FAILED(hr) || !dispatch) {
		wxLogWarning(wxS("Can't get document IDispatch"));
	}
	
	return dispatch;
}

wxCOMPtr<IHTMLDocument2> Browser::GetDocument()
{
	wxCOMPtr<IHTMLDocument2> document;

	wxCOMPtr<IDispatch> dispatch = GetDocumentDispatch();
	if (!dispatch)
		return document;

	HRESULT hr = dispatch->QueryInterface(__uuidof(IHTMLDocument2), (void**)&document);
	if (FAILED(hr) || !document) {
		wxFAIL;
		wxLogWarning(wxS("Can't get document IHTMLDocument2"));
		return document;
	}
	
	return document;
}

void Browser::OnNavigating(wxWebViewEvent& event)
{
	EnableHtmlEventsHandling(false);
	event.Skip();
}

void Browser::OnNavigated(wxWebViewEvent& event)
{
	EnableHtmlEventsHandling(true);
	event.Skip();
}

void Browser::OnPageLoaded(wxWebViewEvent& event)
{
	event.Skip();
}

void Browser::OnHtmlEvents(DWORD WXUNUSED(dwSource), DISPID idEvent, VARIANT* WXUNUSED(pVarResult))
{
	HandleHtmlEvents(idEvent);
}

wxCOMPtr<IHTMLElement> Browser::GetElement(const wxString& id)
{
	DCHECK_WXSTRING(id);

	HRESULT hr;

	wxCOMPtr<IHTMLElement> element;
	wxCOMPtr<IHTMLDocument2> document = GetDocument();
	if (!document)
		return element;

	wxCOMPtr<IHTMLElementCollection> htmlItems;
	hr = document->get_all(&htmlItems);
	if (FAILED(hr))
		return element;

	long count = 0;
	htmlItems->get_length(&count);

	ComVariant name(id);
	for (long i = 0; i < count; ++i) {
		ComVariant index(i);
		wxCOMPtr<IDispatch> dispatch;
		hr = htmlItems->item(name.Get(), index.Get(), &dispatch);
		if (dispatch) {
			hr = dispatch->QueryInterface(__uuidof(IHTMLElement), (void**)&element);
			break;
		}
	}

	return element;
}

bool Browser::HideScroll()
{
	wxCOMPtr<IHTMLDocument2> document = GetDocument();
	CHECK_RET_VAL(document, false);

	wxCOMPtr<IHTMLElement> htmlElement;
	HRESULT hr = document->get_body(&htmlElement);
	if (!htmlElement || FAILED(hr))
		return false;

	wxCOMPtr<IHTMLBodyElement> bodyElement;
	hr = htmlElement->QueryInterface(__uuidof(IHTMLBodyElement), (void**)&bodyElement);
	if (!bodyElement || FAILED(hr))
		return false;
	
	hr = bodyElement->put_scroll(L"no");
	return SUCCEEDED(hr);
}

wxString Browser::GetInputValue(const wxString& id)
{
	DCHECK_WXSTRING(id);

	HRESULT hr;

	wxCOMPtr<IHTMLElement> element = GetElement(id);
	if (!element)
		return wxEmptyString;

	wxCOMPtr<IHTMLInputElement> inputElement;
	hr = element->QueryInterface(__uuidof(IHTMLInputElement), (void**)&inputElement);
	if (FAILED(hr)) {
		DFAIL;
		wxLogWarning(_S("Can't get IHTMLInputElement"));
		return wxEmptyString;
	}

	BSTR value;
	hr = inputElement->get_value(&value);
	wxASSERT(SUCCEEDED(hr));

	return value;
}

void Browser::SetFocus()
{
	HWND hServer = GetIEServerWindow();
	if (hServer != ::GetFocus())
		::SetFocus(hServer);
}

#pragma warning(push)
#pragma warning(disable: 4706)
HWND Browser::GetIEServerWindow() const
{
	HWND hwnd = this->GetHandle();

	wxString className;
	while (hwnd = GetWindow(hwnd, GW_CHILD)) {
		GetClassName(hwnd, wxStringBuffer(className, 256), 256);
		if (className == _S("Internet Explorer_Server"))
			return hwnd;
	}

	return nullptr;
}
#pragma warning(pop)

} // namespace Web

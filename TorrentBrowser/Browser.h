#pragma once

struct IHTMLScriptElement;

namespace Web {
;
class Browser : public wxWebViewIE
{
	NO_COPY_CLASS(Browser);
	wxDECLARE_EVENT_TABLE();

public:
	Browser(wxWindow* parent, wxWindowID id = wxID_ANY, long style = 0);
	virtual ~Browser();

	bool HideScroll();
	wxString GetInputValue(const wxString& id);

	virtual void SetFocus() override;

protected:
	wxCOMPtr<IHTMLDocument2> GetDocument();
	wxCOMPtr<IHTMLElement> GetElement(const wxString& id);
	HWND GetIEServerWindow() const;

private:
	void EnableHtmlEventsHandling(bool enable);
	void HandleHtmlEvents(DISPID eventId);
	wxCOMPtr<IDispatch> GetDocumentDispatch();

private:
	void OnNavigating(wxWebViewEvent& event);
	void OnNavigated(wxWebViewEvent& event);
	void OnPageLoaded(wxWebViewEvent& event);
	void OnHtmlEvents(DWORD dwSource, DISPID idEvent, VARIANT* pVarResult);

private:
	wxCOMPtr<IDispatch> m_eventHandler;
	DWORD m_cookie = 0;
};

} // namespace Web

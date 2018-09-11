
#include "wxStd.h"
#ifndef WX_PRECOMP
#endif
#include "PopupPluginContainer.h"
#include "FileTools.h"
#include "ShellKernel.h"
#include "PluginUIManager.h"
#include "SearchSiteListDialog.h"
#include "SiteSearchDefines.h"


#define DEFAULT_BORDER_SIZE				24
#define LOADING_PANEL_SIZE				wxSize(300, 100)
#define ROUND_COEFF						    0.08

#define KEY_POPUP_BKG_COLOR					wxT("bottomBar/bkgColor")


//------------------------------------------------------------------------------
// Notify events
//
DEFINE_EVENT_TYPE( EVT_POPUP_CONTAINER_HIDED );


//------------------------------------------------------------------------------
// Events for search control
//
DEFINE_EVENT_TYPE( EVT_START_SEARCH );
DEFINE_EVENT_TYPE( EVT_STOP_SEARCH );
DEFINE_EVENT_TYPE( EVT_SHOW_SEARCH_RESULTS );


IMPLEMENT_CLASS( PopupPluginContainer, wxFrame )


BEGIN_EVENT_TABLE( PopupPluginContainer, wxFrame )
	EVT_PAINT				      ( PopupPluginContainer::OnPaint )
	EVT_ERASE_BACKGROUND	( PopupPluginContainer::OnEraseBackground )
	EVT_SIZE				      ( PopupPluginContainer::OnSize )
	EVT_ACTIVATE			    (PopupPluginContainer::OnActivate)

	EVT_BUTTON				( ID_CLOSE_BTN, PopupPluginContainer::OnCloseBtn )
	EVT_BUTTON				( ID_SHOW_SITE_LIST_BTN, PopupPluginContainer::OnShowSiteListBtn )
	EVT_BUTTON				( ID_START_SEARCH_BTN,	PopupPluginContainer::OnStartSearchBtn )
	EVT_BUTTON				( ID_STOP_SEARCH_BTN, PopupPluginContainer::OnStopSearchBtn )
	EVT_BUTTON				( ID_MODIFY_QUERY_BTN, PopupPluginContainer::OnModifyQueryBtn )
	EVT_BUTTON				( ID_SHOW_RESULTS_BTN, PopupPluginContainer::OnShowResultsBtn )
END_EVENT_TABLE()


PopupPluginContainer::PopupPluginContainer () {
	InitMembers();
}


PopupPluginContainer::PopupPluginContainer (wxWindow* parent, wxEvtHandler* evtHandler, SiteSearchPlugin* plugin, wxWindowID id /*= wxID_ANY*/) {
	wxASSERT(parent != NULL);
	
	InitMembers();
	Create(parent, evtHandler, plugin, id);
}


PopupPluginContainer::~PopupPluginContainer() {
}


void	PopupPluginContainer::InitMembers() {
	m_ObjCreated		    = false;
	m_PluginPanel		    = NULL;
	m_LeftSizer			    = NULL;
	m_TopSizer			    = NULL;
	m_RightSizer		    = NULL;
	m_BottomSizer		    = NULL;
	m_EvtHandler		    = NULL;
	m_PluginPanelSizer	= NULL;
	m_DefaultPanel		  = NULL;
	m_Plugin			      = NULL;
	m_CurrentState		  = UNDEFINED;
	m_QueryPanel		    = NULL;
	m_ProgressPanel		  = NULL;
	m_FinalityPanel		  = NULL;
}


bool	PopupPluginContainer::Create (wxWindow* parent, wxEvtHandler* evtHandler, SiteSearchPlugin* plugin, wxWindowID id /*= wxID_ANY*/) {
	if ( ! wxFrame::Create(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxNO_BORDER | wxFRAME_TOOL_WINDOW | wxPOPUP_WINDOW | wxFRAME_FLOAT_ON_PARENT) ) {
		return false;
	}
	SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);

	m_EvtHandler = evtHandler;
	m_Plugin = plugin;

	wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer( mainSizer );

	m_TopSizer = mainSizer->Add(DEFAULT_BORDER_SIZE, DEFAULT_BORDER_SIZE, 0, 0, 0);
	m_PluginPanelSizer = new wxBoxSizer(wxHORIZONTAL);
	m_LeftSizer = m_PluginPanelSizer->Add(DEFAULT_BORDER_SIZE, DEFAULT_BORDER_SIZE, 0, 0, 0);

	m_PluginPanel = GetDefaultPanel();
	m_PluginPanelSizer->Add(m_PluginPanel, 0, 0, 0);

	m_RightSizer = m_PluginPanelSizer->Add(DEFAULT_BORDER_SIZE, DEFAULT_BORDER_SIZE, 0, 0, 0);
	mainSizer->Add(m_PluginPanelSizer, 1, wxEXPAND, 0);
	m_BottomSizer = mainSizer->Add(DEFAULT_BORDER_SIZE, DEFAULT_BORDER_SIZE, 0, 0, 0);

	UpdateSkin();
	UpdateLang();

	m_ObjCreated = true;

	return true;
}


bool	PopupPluginContainer::Show( bool show /*= true*/ ) {
	bool result = true;

	if ( m_ObjCreated ) {
		if ( show ) {
			Raise();

			if ( ! m_Background.IsOk() ) {
				BuildBackground(GetSize());
			}

			result = PlayAnimation(show);

			if ( m_PluginPanel == GetDefaultPanel() ) {
				SetState(QUERY_EDITING);
				SetRoundShape( GetSize() );
			}
		}
		else {
			result = PlayAnimation(show);

			wxCommandEvent evt(EVT_POPUP_CONTAINER_HIDED, GetId());
			if ( m_EvtHandler != NULL ) {
				m_EvtHandler->ProcessEvent(evt);
			}
		}
	}
	else {
		result = wxFrame::Show(show);
	}

	return result;
}


//==============================================================================


bool	PopupPluginContainer::BuildBackground (wxSize imgSize) {
	int minWidth = m_TopLeftCorner.GetWidth() + m_TopRightCorner.GetWidth();
	int minHeight = m_TopLeftCorner.GetHeight() + m_BottomLeftCorner.GetHeight();

	int neededWidth = imgSize.GetWidth();
	if ( neededWidth < minWidth ) {
		neededWidth = minWidth;
	}

	int neededHeight = imgSize.GetHeight();
	if ( neededHeight < minHeight ) {
		neededHeight = minHeight;
	}

	if ( m_Background.IsOk() ) {
		m_Background.FreeResource();
	}
	m_Background.Create(neededWidth, neededHeight);

	if ( m_BackgroundColor.IsOk() ) {
		wxMemoryDC canvasDC;
		canvasDC.SelectObject(m_Background);
		canvasDC.SetBackground(m_BackgroundColor);
		canvasDC.Clear();
		
		////////		draw border		//////
		canvasDC.DrawBitmap(m_TopLeftCorner, 0, 0);
		canvasDC.DrawBitmap(m_TopRightCorner, neededWidth - m_TopRightCorner.GetWidth(), 0);
		canvasDC.DrawBitmap(m_BottomRightCorner, neededWidth - m_BottomRightCorner.GetWidth(), neededHeight - m_BottomRightCorner.GetHeight());
		canvasDC.DrawBitmap(m_BottomLeftCorner, 0, neededHeight - m_BottomLeftCorner.GetHeight());

		int x1 = m_TopLeftCorner.GetWidth();
		int x2 = neededWidth - m_TopRightCorner.GetWidth();
		int y = 0;

		for ( int x = x1; x <= x2; ++x ) {
			canvasDC.DrawBitmap(m_BorderTopBkg, x, y);
		}

		x1 = m_BottomLeftCorner.GetWidth();
		x2 = neededWidth - m_BottomRightCorner.GetWidth();
		y = neededHeight - m_BorderBottomBkg.GetHeight();

		for ( int x = x1; x <= x2; ++x ) {
			canvasDC.DrawBitmap(m_BorderBottomBkg, x, y);
		}

		int y1 = m_TopLeftCorner.GetHeight();
		int y2 = neededHeight - m_BottomLeftCorner.GetHeight();
		int x = 0;

		for ( y = y1; y <= y2; ++y ) {
			canvasDC.DrawBitmap(m_BorderLeftBkg, x, y);
		}

		y1 = m_TopRightCorner.GetHeight();
		y2 = neededHeight - m_BottomRightCorner.GetHeight();
		x = neededWidth - m_BorderRightBkg.GetWidth();

		for ( y = y1; y <= y2; ++y ) {
			canvasDC.DrawBitmap(m_BorderRightBkg, x, y);
		}
		//////////////////////////////////////
		
		canvasDC.SelectObject( wxNullBitmap );
	}

	return m_Background.IsOk();
}


void	PopupPluginContainer::UpdateSkin () {
	KernelInterface* kernel = ShellKernel::GetKernel();

	m_TopLeftCorner		= FileTools::LoadBitmap(kernel->GetSkinFile(TOP_LEFT_CORNER_IMAGE_NAME), wxBITMAP_TYPE_ANY);
	m_TopRightCorner	= FileTools::LoadBitmap(kernel->GetSkinFile(TOP_RIGHT_CORNER_IMAGE_NAME), wxBITMAP_TYPE_ANY);
	m_BottomLeftCorner	= FileTools::LoadBitmap(kernel->GetSkinFile(BOTTOM_LEFT_CORNER_IMAGE_NAME), wxBITMAP_TYPE_ANY);
	m_BottomRightCorner	= FileTools::LoadBitmap(kernel->GetSkinFile(BOTTOM_RIGHT_CORNER_IMAGE_NAME), wxBITMAP_TYPE_ANY);

	m_BorderLeftBkg		= FileTools::LoadBitmap(kernel->GetSkinFile(BORDER_BKG_LEFT_IMAGE_NAME), wxBITMAP_TYPE_ANY);
	m_BorderRightBkg	= FileTools::LoadBitmap(kernel->GetSkinFile(BORDER_BKG_RIGHT_IMAGE_NAME), wxBITMAP_TYPE_ANY);
	m_BorderTopBkg		= FileTools::LoadBitmap(kernel->GetSkinFile(BORDER_BKG_TOP_IMAGE_NAME), wxBITMAP_TYPE_ANY);
	m_BorderBottomBkg	= FileTools::LoadBitmap(kernel->GetSkinFile(BORDER_BKG_BOTTOM_IMAGE_NAME), wxBITMAP_TYPE_ANY);

	m_BackgroundColor	= ShellKernel::GetKernel()->GetSkinColor(KEY_POPUP_BKG_COLOR);

	SetBorderSizes(m_BorderLeftBkg.GetWidth(), m_BorderTopBkg.GetHeight(), m_BorderRightBkg.GetWidth(), m_BorderBottomBkg.GetHeight());
}


void	PopupPluginContainer::UpdateLang () {

}


void	PopupPluginContainer::SetBorderSizes (int leftBorder, int topBorder, int rightBorder, int bottomBorder) {
	if ( m_LeftSizer != NULL ) {
		m_LeftSizer->SetSpacer(leftBorder, m_LeftSizer->GetSpacer().GetHeight());
	}

	if ( m_RightSizer != NULL ) {
		m_RightSizer->SetSpacer(rightBorder, m_RightSizer->GetSpacer().GetHeight());
	}

	if ( m_TopSizer != NULL ) {
		m_TopSizer->SetSpacer(m_TopSizer->GetSpacer().GetWidth(), topBorder);
	}

	if ( m_BottomSizer != NULL ) {
		m_BottomSizer->SetSpacer(m_BottomSizer->GetSpacer().GetWidth(), bottomBorder);
	}
	
	Layout();
	Fit();
}


void	PopupPluginContainer::OnPaint (wxPaintEvent& WXUNUSED(event)) {
	wxPaintDC dc(this);
	PrepareDC(dc);
	PaintBackground(dc);
}


void	PopupPluginContainer::OnEraseBackground (wxEraseEvent& WXUNUSED(event)) {
	// Empty body for avoid flicker
}


void	PopupPluginContainer::PaintBackground(wxDC& dc) {
	wxSize wndSize = GetSize();
	if ( ! m_Background.IsOk() || wndSize != m_PrevSize ) {
		m_PrevSize = wndSize;
		BuildBackground(wndSize);
	}
	if ( m_Background.IsOk() ) {
		dc.DrawBitmap(m_Background, 0, 0);
	}
}


void	PopupPluginContainer::OnSize (wxSizeEvent& event) {
	if ( m_ObjCreated ) {
		Refresh();
		wxSize wndSize = event.GetSize();
		SetRoundShape(wndSize);
	}

	event.Skip();
}


bool	PopupPluginContainer::PlayAnimation (bool show /*= true*/) {
	wxSize wndSize = GetSize();
	int wndWidth = wndSize.GetWidth();
	int wndHeight = wndSize.GetHeight();

	int step = wndHeight * 0.15;
	int roundSize = wndHeight * ROUND_COEFF;

	bool result = false;

	bool processed = false;

	#ifdef __WXMSW__
		HRGN region = NULL;
		HWND wndHandle = (HWND)GetHandle();
		if ( show ) {
			m_PluginPanel->Hide();

			region = ::CreateRoundRectRgn(0, 0, wndWidth, 0, roundSize, roundSize);
			DeleteObject(region);
			SetWindowRgn((HWND)GetHandle(), region, true);

			result = wxFrame::Show(show);

			int currHeight = step;
			while ( (currHeight += step) < wndHeight ) {
				region = CreateRoundRectRgn(0, 0, wndWidth, currHeight, roundSize, roundSize);
				SetWindowRgn(wndHandle, region, true);

				wxApp::GetInstance()->Yield(true);

				wxMilliSleep(4);

				DeleteObject(region);
			}
			SetRoundShape(wndSize);

			m_PluginPanel->Show(true);
		}
		else {
			m_PluginPanel->Hide();

			int currHeight = wndHeight;
			while ( (currHeight -= step) > 0 ) {
				region = CreateRoundRectRgn(0, 0, wndWidth, currHeight, roundSize, roundSize);
				SetWindowRgn(wndHandle, region, true);

				wxApp::GetInstance()->Yield(true);

				DeleteObject(region);
			}
			SetWindowRgn(wndHandle, NULL, false);
			m_PluginPanel->Show();

			result = wxFrame::Show(show);
		}
		processed = true;
	#endif

	if ( ! processed ) {
		result = wxFrame::Show(show);
	}

	return result;
}


void	PopupPluginContainer::OnActivate(wxActivateEvent& event) {
	if ( ! event.GetActive() ) {
		wxPoint mousePos = wxGetMousePosition();
		wxRect wndRect = GetScreenRect();
		if ( ! wndRect.Contains(mousePos) ) {	// Mouse is outside of the plugin container
			wndRect = ShellKernel::GetKernel()->GetMainWindow()->GetScreenRect();
			if ( wndRect.Contains(mousePos) ) {		// Inside main window
				wxMouseState mouseState = ::wxGetMouseState();
				if ( mouseState.LeftIsDown() || mouseState.MiddleIsDown() || mouseState.RightIsDown() ) {
					// Hide popup container on mouse click
					Hide();
				}
			}
		}
	}

	event.Skip();
}


void	PopupPluginContainer::SetRoundShape	(wxSize wndSize) {
	int wndWidth = wndSize.GetWidth();
	int wndHeight = wndSize.GetHeight();

	int roundSize = wndHeight * ROUND_COEFF;

	#ifdef __WXMSW__
		HRGN region = CreateRoundRectRgn(0, 0, wndWidth, wndHeight, roundSize, roundSize);
		SetWindowRgn((HWND)GetHandle(), region, true);
		DeleteObject(region);
	#endif
}

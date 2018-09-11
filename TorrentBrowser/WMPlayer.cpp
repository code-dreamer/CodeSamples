#include "stdafx.h"
#include "WMPlayer.h"
#include "PlayerEvents.h"

const IID IID_IWMPPlayer = {0x6BF52A4F, 0x394A, 0x11D3, {0xB1, 0x53, 0x00, 0xC0, 0x4F, 0x79, 0xFA, 0xA6}};
const IID IID_IWMPPlayer2 = {0x0E6B01D1, 0xD407, 0x4C85, {0xBF, 0x5F, 0x1C, 0x01, 0xF6, 0x15, 0x02, 0x80}};

const int kPosUpdateMs{1000};

WMPlayer::WMPlayer(wxWindow* parent, wxWindowID id)
	: wxControl{parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE}
	, mPrevState{PlayerState::Undefined}
{
	mPosTimer.SetOwner(this);
	Bind(wxEVT_TIMER, &WMPlayer::OnPlayTimer, this, mPosTimer.GetId());

	HRESULT hr = CoCreateInstance(__uuidof(WindowsMediaPlayer), NULL, CLSCTX_INPROC_SERVER, IID_IWMPPlayer, (void**)&mWMPPlayer);
	if (FAILED(hr)) {
		wxLogSysError(_S("Could not create IWMPPlayer."));
		return;
	}
	
	hr = mWMPPlayer->get_settings(&mWMPSettings);
	if (FAILED(hr)) {
		mWMPPlayer->Release();
		wxLogSysError(_S("Could not obtain settings from WMP."));
		return;
	}

	hr = mWMPPlayer->get_controls(&mWMPControls);
	if (FAILED(hr)) {
		mWMPSettings->Release();
		mWMPPlayer->Release();
		wxLogSysError(_S("Could not obtain controls from WMP."));
		return;
	}

	mAX = new wxActiveXContainer(this, __uuidof(IWMPPlayer), mWMPPlayer);
	mAX->Bind(wxEVT_ACTIVEX, &WMPlayer::OnActiveX, this);

	IWMPPlayer2* pWMPPlayer2 = nullptr; // Only 2 has windowless video and stretchtofit
	hr = mWMPPlayer->QueryInterface(IID_IWMPPlayer2, (void**)&pWMPPlayer2);
	if (SUCCEEDED(hr) && pWMPPlayer2) {
		pWMPPlayer2->put_windowlessVideo(VARIANT_TRUE);
		pWMPPlayer2->put_stretchToFit(VARIANT_TRUE);
		pWMPPlayer2->Release();
	}

	mWMPSettings->put_enableErrorDialogs(VARIANT_FALSE);

	EnableAutoStart(true);

	SetVolume(50);

	// don't erase the background of our control window so that resizing is a
	// bit smoother
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);
}

WMPlayer::~WMPlayer()
{
	if (mWMPPlayer) {
		mAX->DissociateHandle();
		delete mAX;

		mWMPPlayer->Release();
		if (mWMPSettings)
			mWMPSettings->Release();
		if (mWMPControls)
			mWMPControls->Release();
	}
}

void WMPlayer::ShowPlayerControls(bool show)
{
	if (show) {
		mWMPPlayer->put_uiMode(wxBasicString(_S("full")).Get());
		mWMPPlayer->put_enabled(VARIANT_TRUE);
	}
	else {
		mWMPPlayer->put_enabled(VARIANT_FALSE);
		mWMPPlayer->put_uiMode(wxBasicString(_S("none")).Get());
	}
}

#ifdef DEBUG
void WMPlayer::Load(const wxString& fileName)
{
	DCHECK_WXSTRING(fileName);

	DoLoad(fileName);
}
#endif // DEBUG

void WMPlayer::Load(const wxURL& location)
{
	DCHECK(location.IsOk());

	DoLoad(location.BuildURI());
	//DoLoad(location.BuildUnescapedURI());
}

void WMPlayer::DoLoad(const wxString& location)
{
	HRESULT hr = mWMPPlayer->put_URL(wxBasicString(location).Get());
	if (FAILED(hr)) {
		wxLogError(_S("put_URL error. location: '%s'"), location);
		DFAIL;
		return;
	}
}

// Called when our media is about to play (a.k.a. wmposMediaOpen)
//
void WMPlayer::OnFileLoaded()
{
	IWMPMedia* pWMPMedia = nullptr;
	HRESULT hr = mWMPPlayer->get_currentMedia(&pWMPMedia);
	if (SUCCEEDED(hr) && pWMPMedia) {
		pWMPMedia->get_imageSourceWidth((long*)&mBestSize.x);
		pWMPMedia->get_imageSourceHeight((long*)&mBestSize.y);
		pWMPMedia->Release();
	}
	else {
		wxLogError(wxT("Could not get media"));
	}

	NotifyMovieSizeChanged();
}

void WMPlayer::Play()
{
	// Actually try to play the movie (will fail if not loaded completely)
	HRESULT hr = mWMPControls->play();
	if (FAILED(hr))
		wxLogError(_S("play error"));
}

void WMPlayer::Pause()
{
	HRESULT hr = mWMPControls->pause();
	if (FAILED(hr))
		wxLogError(_S("pause error"));
}

void WMPlayer::Stop()
{
	HRESULT hr = mWMPControls->stop();
	if (SUCCEEDED(hr)) {
		// Seek to beginning
		SetPlayPosition(0);
	}
	else {
		wxLogError(_S("stop error"));
	}
}

void WMPlayer::EnableAutoStart(bool enable)
{
	mWMPSettings->put_autoStart(enable ? VARIANT_TRUE : VARIANT_FALSE);
}

void WMPlayer::SetPlayPosition(double seconds)
{
	HRESULT hr = mWMPControls->put_currentPosition(seconds);
	if (FAILED(hr)) {
		wxLogError(_S("put_currentPosition failed"));
		return;
	}
}

double WMPlayer::GetPlayPosition() const
{
	double seconds;
	HRESULT hr = mWMPControls->get_currentPosition(&seconds);
	if (FAILED(hr)) {
		wxLogError(_S("get_currentPosition failed"));
		return 0;
	}

	return seconds;
}

wxString WMPlayer::GetPlayPositionString() const
{
	wxString result;

	BSTR str;
	HRESULT hr = mWMPControls->get_currentPositionString(&str);
	if (SUCCEEDED(hr)) {
		result = wxConvertStringFromOle(str);
	}
	else {
		wxLogError(wxT("get_currentPosition failed"));
	}

	return result;
}


// Volume 0-100
//
long WMPlayer::GetVolume()
{
	long volume{};
	HRESULT hr = mWMPSettings->get_volume(&volume);
	if (FAILED(hr)) {
		wxLogError(_S("get_volume failed"));
		return 0;
	}

	return volume;
}

// Volume 0-100
//
void WMPlayer::SetVolume(long volume)
{
	HRESULT hr = mWMPSettings->put_volume(volume);
	if (FAILED(hr)) {
		wxLogError(_S("put_volume failed"));
	}
}

double WMPlayer::GetDuration() const
{
	double seconds = -1.0;

	IWMPMedia* pWMPMedia{};
	HRESULT hr = mWMPPlayer->get_currentMedia(&pWMPMedia);
	if (SUCCEEDED(hr) && pWMPMedia) {
		hr = pWMPMedia->get_duration(&seconds);
		if (FAILED(hr)) {
			wxLogError(wxT("get_duration failed"));
		}
		pWMPMedia->Release();
	}
	
	return seconds;
}

wxString WMPlayer::GetDurationString() const
{
	wxString result;

	IWMPMedia* pWMPMedia{};
	HRESULT hr = mWMPPlayer->get_currentMedia(&pWMPMedia);
	if (SUCCEEDED(hr) && pWMPMedia) {
		BSTR str;
		hr = pWMPMedia->get_durationString(&str);
		if (SUCCEEDED(hr)) {
			result = wxConvertStringFromOle(str);
		}
		else {
			wxLogError(wxT("get_duration failed"));
		}
		pWMPMedia->Release();
	}

	return result;
}

PlayerState WMPlayer::GetState() const
{	
	WMPPlayState state;
	HRESULT hr = mWMPPlayer->get_playState(&state);
	if (FAILED(hr)) {
		wxLogError(wxT("get_playState failed"));
		DFAIL;
		return PlayerState::Undefined;
	}

	return ConvertWmpState(state);
}

// Handle events sent from our activex control (_WMPOCXEvents actually).
//
void WMPlayer::OnActiveX(wxActiveXEvent& event)
{
	const DISPID dispid = event.GetDispatchId();

	switch (dispid) {
		case DISPID_WMPCOREEVENT_PLAYSTATECHANGE:
			if (event.ParamCount() >= 1) {
				PlayerState playerState = ConvertWmpState((WMPPlayState)event[0].GetInteger());
				wxLogMessage(_S("PlayerState changed: '%s' --> '%s'"), PlayerStateToString(playerState), PlayerStateToString(mPrevState));
				DCHECK(playerState != PlayerState::Undefined);
				if (playerState != PlayerState::Undefined) {
					PlayerStateChangedEvent* event = new PlayerStateChangedEvent{GetId()};
					event->playerState = playerState;
					event->playerPrevState = mPrevState;
					event->SetEventObject(this);
					QueueEvent(event);

					mPrevState = playerState;

					if (!mPosTimer.IsRunning() && playerState == PlayerState::Playing)
						mPosTimer.Start(kPosUpdateMs);
					else if (mPosTimer.IsRunning() && playerState != PlayerState::Playing)
						mPosTimer.Stop();
				}
			}
			else
				event.Skip();
			break;

		case DISPID_WMPCOREEVENT_OPENSTATECHANGE:
			if (event.ParamCount() >= 1) {
				PlayerOpenState playerOpenState = ConvertWmpOpenState((WMPOpenState)event[0].GetInteger());
				DCHECK(playerOpenState != PlayerOpenState::Undefined);
				if (playerOpenState != PlayerOpenState::Undefined) {
					if (playerOpenState == PlayerOpenState::MediaOpen)
						OnFileLoaded();
					PlayerOpenStateChangedEvent* event = new PlayerOpenStateChangedEvent{playerOpenState, GetId()};
					event->SetEventObject(this);
					QueueEvent(event);
				}
			}
			else
				event.Skip();
			break;

		case DISPID_WMPOCXEVENT_MOUSEDOWN:
			SetFocus();
			break;

		case DISPID_WMPOCXEVENT_MOUSEMOVE: {
			wxActiveXEventNativeMSW* params = event.GetNativeParameters();
			DCHECK(params);
			//short nButton = params->pDispParams->rgvarg[3].iVal;
			//short nShiftState = params->pDispParams->rgvarg[2].iVal;
			long x = params->pDispParams->rgvarg[1].lVal;
			long y = params->pDispParams->rgvarg[0].lVal;
			wxMouseEvent event{wxEVT_MOTION};
			event.SetEventObject(this);
			event.SetPosition(wxPoint{x, y});
			ProcessEvent(event);
			break;
		}

		case DISPID_WMPCOREEVENT_POSITIONCHANGE:
			QueuePosChangedEvent();
			break;

		case DISPID_WMPCOREEVENT_ERROR: {
			wxString errorDescription;
			int errorCode = -1;

			IWMPError* error{};
			HRESULT hr = mWMPPlayer->get_error(&error);
			if (SUCCEEDED(hr) && error) {
				long errorCount{};
				error->get_errorCount(&errorCount);
				DCHECK(errorCount == 1);
				
				IWMPErrorItem* errorItem{};
				hr = error->get_item(0, &errorItem);
				if (SUCCEEDED(hr) && errorItem) {
					BSTR _errorDescription;
					hr = errorItem->get_errorDescription(&_errorDescription);
					if (SUCCEEDED(hr))
						errorDescription = wxConvertStringFromOle(_errorDescription);

					long _errorCode;
					hr = errorItem->get_errorCode(&_errorCode);
					if (SUCCEEDED(hr))
						errorCode = _errorCode;

					errorItem->Release();
				}

				error->clearErrorQueue();
				error->Release();

				PlayerErrorEvent event;
				event.errorCode = errorCode;
				event.errorString = errorDescription;
				if (!ProcessEvent(event))
					wxMessageBox(errorDescription, _("Error"), wxOK | wxICON_ERROR, GetParent());
			}

			break;
		}

		default:
			event.Skip();
			return;
	}
}

void WMPlayer::NotifyMovieSizeChanged()
{
	// our best size changed after opening a new file
	InvalidateBestSize();

	// if the parent of the control has a sizer ask it to refresh our size
	wxWindow* const parent = GetParent();
	if (parent->GetSizer()) {
		parent->Layout();
		parent->Refresh();
		parent->Update();
	}
}

wxSize WMPlayer::DoGetBestSize() const
{
	return mBestSize;
}

void WMPlayer::OnPlayTimer(wxTimerEvent& WXUNUSED(event))
{
	QueuePosChangedEvent();
}

void WMPlayer::QueuePosChangedEvent()
{
	PlayerPlayTimeChangedEvent* event = new PlayerPlayTimeChangedEvent{GetId()};
	event->SetEventObject(this);
	event->seconds = GetPlayPosition();
	event->timeString = GetPlayPositionString();
	QueueEvent(event);
}

bool WMPlayer::IsPlaying() const
{
	return (GetState() == PlayerState::Playing);
}

bool WMPlayer::IsPaused() const
{
	return (GetState() == PlayerState::Paused);
}

bool WMPlayer::IsStoped() const
{
	const PlayerState state = GetState();
	return (state == PlayerState::Stopped || state == PlayerState::Undefined);
}

bool WMPlayer::IsReadyToPlay() const
{
	const PlayerState state = GetState();
	return (state == PlayerState::ReadyToPlay || state == PlayerState::Paused) 
		&& GetDuration() > 0.0;
}

void WMPlayer::SetBufferingTime(long time)
{
	IWMPNetwork* net{};
	HRESULT hr = mWMPPlayer->get_network(&net);
	CHECK_RET(SUCCEEDED(hr));
	hr = net->put_bufferingTime(time);
	DCHECK(SUCCEEDED(hr));
}

long WMPlayer::GetBufferingTime() const
{
	long ret = -1;

	IWMPNetwork* net{};
	HRESULT hr = mWMPPlayer->get_network(&net);
	CHECK_RET_VAL(SUCCEEDED(hr), ret);
	hr = net->get_bufferingTime(&ret);
	DCHECK(SUCCEEDED(hr));

	return ret;
}

wxURL WMPlayer::GetSourceUrl() const
{
	wxURL ret;

	IWMPMedia* pWMPMedia = nullptr;
	HRESULT hr = mWMPPlayer->get_currentMedia(&pWMPMedia);
	if (SUCCEEDED(hr) && pWMPMedia) {
		BSTR location;
		pWMPMedia->get_sourceURL(&location);
		ret = wxConvertStringFromOle(location);
		DCHECK(ret.IsOk());
		pWMPMedia->Release();
	}

	return ret;
}

// Allow the user code to use wxFORCE_LINK_MODULE() to ensure that this object
// file is not discarded by the linker.
#include "wx/link.h"
wxFORCE_LINK_THIS_MODULE(wxmediabackend_wmp10)

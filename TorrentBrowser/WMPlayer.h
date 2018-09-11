#pragma once

enum class PlayerState;

class WMPlayer : public wxControl
{
	NO_COPY_CLASS(WMPlayer);

public:
	WMPlayer(wxWindow* parent, wxWindowID id);
	virtual ~WMPlayer();

	void ShowPlayerControls(bool show);

	void Play();
	bool IsPlaying() const;
	bool IsPaused() const;
	bool IsStoped() const;
	bool IsReadyToPlay() const;
	void Pause();
	void Stop();
	void EnableAutoStart(bool enable);

#ifdef DEBUG
	void Load(const wxString& fileName);
#endif
	void Load(const wxURL& location);

	wxURL GetSourceUrl() const;

	PlayerState GetState() const;

	void SetPlayPosition(double seconds);
	double GetPlayPosition() const;
	wxString GetPlayPositionString() const;
	double GetDuration() const;
	wxString GetDurationString() const;

	// Volume 0-100
	long GetVolume();
	void SetVolume(long volume);

	void SetBufferingTime(long time);
	long GetBufferingTime() const;
	
protected:
	virtual wxSize DoGetBestSize() const override;

private:
	void DoLoad(const wxString& location);
	void NotifyMovieSizeChanged();
	void QueuePosChangedEvent();

	void OnActiveX(wxActiveXEvent& event);

	void OnFileLoaded();
	void OnPlayTimer(wxTimerEvent& event);

private:
	wxActiveXContainer* mAX{};

	IWMPPlayer* mWMPPlayer{};       // Main activex interface
	IWMPSettings* mWMPSettings{};   // Settings such as volume
	IWMPControls* mWMPControls{};   // Control interface (play etc.)
	wxSize mBestSize;				// Actual movie size

	wxTimer mPosTimer;

	PlayerState mPrevState;
};

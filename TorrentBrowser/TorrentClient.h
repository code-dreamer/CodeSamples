#pragma once
#include "DownloadItem.h"

class TorrentDownloadItem;

class TorrentClient : public wxEvtHandler
{
	NO_COPY_CLASS(TorrentClient);
	wxDECLARE_EVENT_TABLE();

public:
	TorrentClient(const wxString& dataDir = wxEmptyString);
	virtual ~TorrentClient();

public:
	DownloadItem* FindTorrent(const wxMemoryBuffer& torrentFileData);
	DownloadItem* FindTorrent(const wxString& torrentFilePath);
	DownloadItem* FindMagnet(const wxString& magnetUrl);

	TorrentDownloadItem* AddTorrent(const wxString& torrentFilePath, const wxString& downFilesSaveDir, bool paused = false);
	TorrentDownloadItem* AddTorrent(const wxMemoryBuffer& torrentFileData, const wxString& downFilesSaveDir, bool paused = false);
	bool AddCachedTorrent(const wxString& torrentFilePath);
	bool AddCachedTorrent(const wxMemoryBuffer& torrentFileData);
	void ClearCachedDownloads();
	
	TorrentDownloadItem* AddMagnet(const wxString& magnetUrl, const wxString& name, const wxString& downFilesSaveDir, bool paused = false);
	bool AddCachedMagnet(const wxString& magnetUrl, const wxString& name);

	void PauseTorrent(const DownloadItem* torrent);
	void StartTorrent(const DownloadItem* torrent);
	void DeleteTorrent(const DownloadItem* torrent, bool withFiles = false);
	void SetDownloadLimit(const DownloadItem* item, wxInt32 bytes);

	wxInt32 GetMaxRememberedSpeed() const;
	void ResetMaxRememberedSpeed();

	void SetDownloadLimit(wxInt32 bytes);
	
	void EnableFullDownloadAllocation(bool enable = true);

	void LoadTorrentsQueue();
	void SaveTorrentsQueue();

	void DeleteAllTorrents(bool withFiles = false);
	void SyncDeleteAllTorrents(bool withFiles = false);

	libtorrent::session_settings GetSessionSettings() const;
	void SetSessionSettings(const libtorrent::session_settings& settings);

private:
	void OnAlertsTimer(wxTimerEvent& event);
	void OnFastResumeTimer(wxTimerEvent& event);

private:
	void InitSession();
	void ShutdownSession();
	
	wxString GetSessionFilePath() const;
	wxString GetResumeFilePath(const libtorrent::sha1_hash& hash) const;
	wxString GetTorrentFilePath(const wxString& hash) const;
	
	void HandleAlert(const libtorrent::alert* alert);

	bool LoadSessionState();
	bool SaveSessionState();
	void PauseAndSaveFastResume();

	void EnableUPnP();
	void EnableDht();

	bool SetPorts();

	TorrentDownloadItem* AddTorrentImpl(libtorrent::add_torrent_params& addParams, const wxString& downFilesSaveDir, bool paused);

	TorrentDownloadItem* TorrentFromHandle(const libtorrent::torrent_handle& handle);

private:
	libtorrent::session m_session;

	std::vector<std::unique_ptr<TorrentDownloadItem>> m_torrents;
	std::vector<libtorrent::torrent_handle> m_cachedTorrents;
	std::vector<libtorrent::torrent_handle> m_deletedTorrents;
	std::vector<libtorrent::torrent_handle> m_needToSaveTorrents;

	struct MagnetInfo
	{
		TorrentDownloadItem* torrent;
		wxString name;
		wxString url;
		wxString saveDir;
		bool paused;
	};
	std::vector<MagnetInfo> m_waitedMagnets;

	wxTimer m_alertsTimer;

	wxString m_cahedDir;

	wxInt32 m_maxRememberedSpeed = -1;
	wxInt32 m_currDownlaodLimit = 0;

	bool mFullAllocation{};

	wxString mDataDir;
	wxString mTorrentsDir;
};

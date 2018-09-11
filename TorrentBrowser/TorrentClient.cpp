#include "stdafx.h"
#include "TorrentClient.h"
#include "FilesystemTools.h"
#include "TorrentDownloadItem.h"
#include "TorrentTools.h"
#include "DownloadManager.h"
#include "DownloadEvents.h"
#include "Net/Utils.h"
#include "StringTools.h"
#include "AppCore.h"
#include "AppInfo.h"
#include "StandardPaths.h"

using namespace libtorrent;
using namespace FilesystemTools;

const uint32_t AlertTypes
	= alert::status_notification
	| alert::progress_notification
	| alert::storage_notification
	| alert::error_notification
	| alert::performance_warning
	| alert::peer_notification
	| alert::port_mapping_notification
	| alert::tracker_notification
	| alert::ip_block_notification
	| alert::dht_notification;

const wxInt32 AlertsTimerId = 100;
const wxInt32 AlertsTimerInterval = 500;

const int TorrentListeningPort = 6881;

const wxString QueueDumpFilename = wxS("Queue.dat");
const int kMinSpaceAllowed = 10 * 1024 * 1024;

struct DhtRouter
{
	const char* hostName;
	int port;
};
const DhtRouter DhtRouters[] =
{
	{"router.bittorrent.com", TorrentListeningPort},
	{"router.utorrent.com", TorrentListeningPort},
	{"router.bitcomet.com", TorrentListeningPort},
	{"dht.transmissionbt.com", TorrentListeningPort},
	{"dht.aelitis.com", TorrentListeningPort},
	{nullptr, -1}
};

const std::vector<std::string> kTrackers = 
{
	"http://tracker.openbittorrent.com/announce",
	"http://tracker.publicbt.com/announce",
	"http://bt.firebit.org:2710/announce",
	"http://pubt.net:2710/announce",
	"http://bt.rutor.org:2710/announce",
	"http://bt2.rutracker.org/ann?uk=0qfwBZUZVT ",
	"http://retracker.local/announce",
	"udp://tracker.openbittorrent.com:80/announce",
	"udp://tracker.ccc.de:80/announce",
	"udp://tracker.publicbt.com:80/announce"
};

namespace {
;
bool SaveResumeDataToFile(const wxString& filePath, const entry& resumeData)
{
	DCHECK_WXSTRING(filePath);
	DCHECK(resumeData.type() != entry::undefined_t);

	std::vector<char> encodedData;
	bencode(std::back_inserter(encodedData), resumeData);
	return FilesystemTools::SaveDataToFile(filePath, encodedData);
}

} // namespace

wxBEGIN_EVENT_TABLE(TorrentClient, wxEvtHandler)
	EVT_TIMER(AlertsTimerId, TorrentClient::OnAlertsTimer)
	//EVT_TIMER(FastResumeTimerId, TorrentClient::OnFastResumeTimer)
wxEND_EVENT_TABLE()

TorrentClient::TorrentClient(const wxString& dataDir)
	: m_session(fingerprint("TI", AppCore::GetAppInfo().GetProductVersion(), 0, 0, 0))
	, mDataDir{dataDir}
{
	if (mDataDir.IsEmpty()) {
		mDataDir = MergePath(AppCore::GetConfigDir(), _S("TorrentClient"));
		if (!wxDirExists(mDataDir)) {
			FilesystemTools::CreateDir(mDataDir, true);
		}
		mTorrentsDir = FilesystemTools::MergePath(mDataDir, _S("Torrents"));
		if (!wxDirExists(mTorrentsDir)) {
			FilesystemTools::CreateDir(mTorrentsDir);
		}
	}

	wxString tmpDir = StandardPaths::Get().GetTempDir();
	m_cahedDir = FilesystemTools::MergePath(tmpDir, wxApp::GetInstance()->GetAppName());
	m_cahedDir = FilesystemTools::MergePath(m_cahedDir, wxS("Cached"));
	if (wxDirExists(m_cahedDir))
		wxDir::Remove(m_cahedDir, wxPATH_RMDIR_RECURSIVE);
	FilesystemTools::CreateDir(m_cahedDir, true);
	
	InitSession();
	
	m_alertsTimer.SetOwner(this, AlertsTimerId);
	m_alertsTimer.StartOnce(AlertsTimerInterval);
	//m_fastResumeTimer.SetOwner(this, FastResumeTimerId);
	//m_fastResumeTimer.StartOnce(FastResumeInterval);
}

TorrentClient::~TorrentClient()
{
	ShutdownSession();

	if (wxDirExists(m_cahedDir))
		wxDir::Remove(m_cahedDir, wxPATH_RMDIR_RECURSIVE);
}

void TorrentClient::InitSession()
{
	LoadSessionState();
	m_session.set_alert_mask(AlertTypes);
	SetPorts();

	const AppInfo& appInfo = AppCore::GetAppInfo();
	
	session_settings settings = m_session.settings();
	settings.user_agent = wxString::Format("%s %s.0",
										   appInfo.GetAppName(),
										   appInfo.GetProductVersionString())
										   .ToStdString();

	settings.upnp_ignore_nonrouters = true;
	settings.use_dht_as_fallback = false;
	// disable support for SSL torrents
	settings.ssl_listen = 0;

	// to prevent ISPs from blocking seeding
	settings.lazy_bitfields = true;

	// speed up exit
	settings.stop_tracker_timeout = 1;

	settings.auto_scrape_interval = 1200; // 20 minutes

	settings.optimize_hashing_for_speed = true;
	settings.disk_cache_algorithm = session_settings::largest_contiguous;

	settings.ban_web_seeds = true;

	m_session.set_settings(settings);
	
	EnableUPnP();
	EnableDht();
}

session_settings TorrentClient::GetSessionSettings() const
{
	return m_session.settings();
}

void TorrentClient::SetSessionSettings(const libtorrent::session_settings& settings)
{
	m_session.set_settings(settings);
}

void TorrentClient::EnableDht()
{
	for (int i = 0; DhtRouters[i].hostName != nullptr; ++i) {
		m_session.add_dht_router(std::make_pair(std::string(DhtRouters[i].hostName), DhtRouters[i].port));
	}

	m_session.start_dht();
}

void TorrentClient::EnableUPnP()
{
	m_session.start_upnp();
	m_session.start_natpmp();
}

bool TorrentClient::SetPorts()
{
	error_code errorCode;
	std::pair<int, int> ports(TorrentListeningPort, TorrentListeningPort + 10);
	m_session.listen_on(ports, errorCode);
	return !errorCode;
}

void TorrentClient::ShutdownSession()
{
	SaveSessionState();
	PauseAndSaveFastResume();
}

void TorrentClient::PauseAndSaveFastResume()
{
	m_alertsTimer.Stop();
	m_alertsTimer.SetOwner(nullptr);

	std::vector<TorrentDownloadItem*> outdatedTorrents;
	for (auto it = m_torrents.begin(); it != m_torrents.end(); ++it) {
		const torrent_handle& handle = (*it)->GetHandle();
		torrent_status status = handle.status();

		if (handle.is_valid() && status.has_metadata && handle.need_save_resume_data() && !status.paused) {
			outdatedTorrents.push_back(it->get());
		}
	}

	m_session.pause();

	for (auto it = outdatedTorrents.begin(); it != outdatedTorrents.end(); ++it) {
		TorrentDownloadItem* torrent = *it;
		torrent->GetHandle().save_resume_data();
	}

	std::deque<libtorrent::alert*> alerts;
	while (!outdatedTorrents.empty()) {
		m_session.pop_alerts(&alerts);
		for (auto alertIt = alerts.cbegin(); alertIt != alerts.cend(); ++alertIt) {
			alert* alert = *alertIt;
			DCHECK(alert);
			HandleAlert(alert);

			save_resume_data_alert* saveResumeAlert = alert_cast<save_resume_data_alert>(alert);
			if (saveResumeAlert) {
				auto it = std::find_if(outdatedTorrents.begin(), outdatedTorrents.end(), [&](TorrentDownloadItem* torrent) {
					return torrent->GetHandle() == saveResumeAlert->handle;
				});
				DCHECK(it != outdatedTorrents.end());
				outdatedTorrents.erase(it);
			}

			delete alert;
		}

		alerts.clear();
		wxMilliSleep(30);
	}
}

bool TorrentClient::LoadSessionState()
{
	const wxString sessionSavePath = GetSessionFilePath();

	bool ok = false;
	
	if (wxFileExists(sessionSavePath)) {
		// Load state
		std::vector<char> inBuffer;
		if (!TorrentTools::LoadResumeDataFromFile(sessionSavePath, inBuffer)) {
			wxRemoveFile(sessionSavePath);
			return ok;
		}
		error_code errorCode;
		lazy_entry e;
		if (lazy_bdecode(&inBuffer[0], &inBuffer[0] + inBuffer.size(), e, errorCode) == 0) {
			if (!errorCode) {
				m_session.load_state(e);
				ok = true;
			}
		}
		
	}
	
	return ok;
}

bool TorrentClient::SaveSessionState()
{
	// saving session state
	entry session_state;
	m_session.save_state(session_state);

	std::vector<char> out;
	bencode(back_inserter(out), session_state);

	return SaveDataToFile(GetSessionFilePath(), out);
}

wxString TorrentClient::GetResumeFilePath(const libtorrent::sha1_hash& hash) const
{
	wxString hashStr = TorrentTools::HashToString(hash);
	return MergePath(mTorrentsDir, hashStr + wxS(".resume"));
}

wxString TorrentClient::GetSessionFilePath() const
{
	return MergePath(mDataDir, wxS("session.state"));
}

TorrentDownloadItem* TorrentClient::AddTorrent(const wxString& torrentFilePath, const wxString& downFilesSaveDir, bool paused)
{
	DCHECK_WXSTRING(torrentFilePath);
	DCHECK_WXSTRING(downFilesSaveDir);

	wxMemoryBuffer torrentFileBuff = FilesystemTools::ReadFile(torrentFilePath);
	if (torrentFileBuff.GetDataLen() <= 0) {
		wxLogError(_("Torrent load failed"));
		return false;
	}

	return AddTorrent(torrentFileBuff, downFilesSaveDir, paused);
}

TorrentDownloadItem* TorrentClient::AddTorrent(const wxMemoryBuffer& torrentFileData, const wxString& downFilesSaveDir, bool paused)
{
	DCHECK(torrentFileData.GetDataLen() > 0);
	DCHECK_WXSTRING(downFilesSaveDir);

	error_code ec;
	add_torrent_params addTorrentParams;
	boost::intrusive_ptr<torrent_info> ti(new torrent_info(torrentFileData, range_cast<int>(torrentFileData.GetDataLen()), ec));
	if (ec) {
		wxLogError(_("Invalid torrent file:\n") + ec.message().c_str());
		return false;
	}

	if (ti->num_files() == 0) {
		wxLogWarning(_("Torrent file is empty"));
		return false;
	}

	addTorrentParams.ti = ti;
	addTorrentParams.info_hash = ti->info_hash();

	return AddTorrentImpl(addTorrentParams, downFilesSaveDir, paused);
}

TorrentDownloadItem* TorrentClient::AddMagnet(const wxString& magnetUrl, const wxString& name, const wxString& downFilesSaveDir, bool paused)
{
	error_code ec;
	add_torrent_params addParams;
	std::string magnetUrlStd = magnetUrl.ToUTF8().data();
	parse_magnet_uri(magnetUrlStd, addParams, ec);
	if (ec) {
		wxLogError(_("Invalid magnet url:\n") + ec.message().c_str());
		return false;
	}

	// TODO: add url downloading fail processing
	addParams.url = magnetUrlStd;
	addParams.name = name;

	TorrentDownloadItem* torrent = AddTorrentImpl(addParams, downFilesSaveDir, paused);

	if (torrent) {
		MagnetInfo magnetInfo;
		magnetInfo.url = magnetUrl;
		magnetInfo.name = name;
		magnetInfo.saveDir = downFilesSaveDir;
		magnetInfo.paused = paused;
		magnetInfo.torrent = torrent;

		m_waitedMagnets.push_back(magnetInfo);
	}

	return torrent;
}

TorrentDownloadItem* TorrentClient::AddTorrentImpl(libtorrent::add_torrent_params& addParams, const wxString& downFilesSaveDir, bool paused)
{
	DCHECK(!addParams.info_hash.is_all_zeros());

	if (!wxDirExists(downFilesSaveDir)) {
		if (!wxMkdir(downFilesSaveDir)) {
			wxLogError("%s %s", _("Can't create dir"), downFilesSaveDir);
			return nullptr;
		}
	}
	
	auto it = std::find_if(m_cachedTorrents.cbegin(), m_cachedTorrents.cend(), [addParams](const torrent_handle& handle) {
		return (addParams.info_hash == handle.info_hash());
	});
	torrent_handle handle;
	if (it == m_cachedTorrents.cend()) {

		addParams.flags |=
			add_torrent_params::flag_paused
			| add_torrent_params::flag_auto_managed
			| add_torrent_params::flag_update_subscribe
			| add_torrent_params::flag_merge_resume_trackers;

		addParams.save_path = downFilesSaveDir.ToUTF8();
		addParams.storage_mode = mFullAllocation ? storage_mode_allocate : storage_mode_sparse;
		addParams.trackers = kTrackers;
		
		// Check for duplicate torrent
		const torrent_handle& existedTorrent = m_session.find_torrent(addParams.info_hash);
		if (existedTorrent.is_valid()) {
			TorrentDownloadItem* item = TorrentFromHandle(existedTorrent);
			if (item) {
				return TorrentFromHandle(existedTorrent);
			}
			DFAIL;
		}

		wxString resumeFilePath = GetResumeFilePath(addParams.info_hash);
		std::vector<char> resumeData;
		if (TorrentTools::LoadResumeDataFromFile(resumeFilePath, resumeData))
			addParams.resume_data = &resumeData;

		error_code ec;
		handle = m_session.add_torrent(addParams, ec);
		if (ec) {
			wxLogError(_("Invalid torrent file:\n") + ec.message().c_str());
			return nullptr;
		}
		DCHECK(handle.is_valid());

		m_needToSaveTorrents.push_back(handle);
	}
	// load from cache
	else {
		handle = *it;
		handle.move_storage(downFilesSaveDir.ToUTF8().data());
		m_cachedTorrents.erase(it);
	}

	DCHECK(handle.is_valid());

	TorrentDownloadItem* torrent = new TorrentDownloadItem(handle, *this, downFilesSaveDir);
	
	if (!addParams.name.empty())
		torrent->SetName(addParams.name);

	bool torrentPaused = handle.status().paused;
	if (torrentPaused && !paused)
		handle.resume();
	else if (!torrentPaused && paused)
		handle.pause();

	m_torrents.push_back(std::unique_ptr<TorrentDownloadItem>(torrent));

	m_torrents.back()->SetDownloadLimit(m_currDownlaodLimit);

	SaveTorrentsQueue();
	
	wxCommandEvent* evt = new wxCommandEvent(DOWNLOAD_ADDED);
	evt->SetEventObject(this);
	evt->SetClientData(m_torrents.back().get()); // TODO: void cast is unsafe, special event class will help
	QueueEvent(evt);

	return torrent;
}

DownloadItem* TorrentClient::FindTorrent(const wxMemoryBuffer& torrentFileData)
{
	error_code ec;
	add_torrent_params addTorrentParams;
	boost::intrusive_ptr<torrent_info> ti(new torrent_info(torrentFileData, range_cast<int>(torrentFileData.GetDataLen()), ec));
	if (ec) {
		return false;
	}

	torrent_handle handle = m_session.find_torrent(ti->info_hash());
	if (!handle.is_valid())
		return nullptr;

	return TorrentFromHandle(handle);
}

DownloadItem* TorrentClient::FindTorrent(const wxString& torrentFilePath)
{
	error_code ec;
	add_torrent_params addTorrentParams;
	boost::intrusive_ptr<torrent_info> ti(new torrent_info(torrentFilePath.ToStdString(), ec));
	if (ec) {
		return false;
	}

	torrent_handle handle = m_session.find_torrent(ti->info_hash());
	if (!handle.is_valid())
		return nullptr;

	return TorrentFromHandle(handle);
}

DownloadItem* TorrentClient::FindMagnet(const wxString& magnetUrl)
{
	error_code ec;
	add_torrent_params addTorrentParams;
	parse_magnet_uri(magnetUrl.ToUTF8().data(), addTorrentParams, ec);
	if (ec) {
		return false;
	}

	torrent_handle handle = m_session.find_torrent(addTorrentParams.info_hash);
	if (!handle.is_valid())
		return nullptr;

	return TorrentFromHandle(handle);
}

void TorrentClient::OnAlertsTimer(wxTimerEvent& WXUNUSED(event))
{
	// pop all alerts
	std::deque<libtorrent::alert*> alerts;
	m_session.pop_alerts(&alerts);
	auto it = alerts.begin();
	auto itEnd = alerts.end();
	for (; it != itEnd; ++it) {
		DCHECK(*it);
		HandleAlert(*it);
		delete *it;
	}

	m_session.post_torrent_updates();

	if (!m_alertsTimer.IsRunning())
		m_alertsTimer.StartOnce(AlertsTimerInterval);
}

void TorrentClient::HandleAlert(const alert* alert)
{
	DCHECK(alert);

	if (auto concreteAlert = alert_cast<torrent_finished_alert>(alert)) {
		const torrent_handle& handle = concreteAlert->handle;
		if (handle.is_valid() || !TorrentFromHandle(handle)) {
			if (handle.need_save_resume_data())
				handle.save_resume_data();

			if (handle.status().total_payload_download > 0) {
				TorrentDownloadItem* torrent = TorrentFromHandle(handle);
				DCHECK(torrent);

				//wxString msg = wxString::Format(wxS("Torrent finished: name = %s"), torrent->GetName());
				//wxLogMessage(msg);

				wxCommandEvent evt(DOWNLOAD_FINISHED);
				evt.SetEventObject(this);
				evt.SetClientData(torrent);
				ProcessEvent(evt);
			}
		}
	}
	else if (auto concreteAlert = alert_cast<torrent_paused_alert>(alert)) {
		const torrent_handle& handle = concreteAlert->handle;

		if (!TorrentFromHandle(handle))
			return;

		DCHECK(handle.is_valid());
		CHECK_RET(handle.is_valid());
		DCHECK(TorrentFromHandle(handle));

		// the alert handler for save_resume_data_alert
		// will save it to disk
		if (handle.need_save_resume_data())
			handle.save_resume_data();

		wxCommandEvent evt(DOWNLOAD_PAUSED);
		evt.SetEventObject(this);
		TorrentDownloadItem* torrent = TorrentFromHandle(handle);
		DCHECK(torrent);
		evt.SetClientData(torrent);
		ProcessEvent(evt);
	}
	else if (auto concreteAlert = alert_cast<metadata_failed_alert>(alert)) {
		DFAIL;
	}
	else if (auto concreteAlert = alert_cast<metadata_received_alert>(alert)) {
		const torrent_handle& handle = concreteAlert->handle;
		TorrentDownloadItem* torrent = TorrentFromHandle(handle);
		if (torrent) {
			wxLogVerbose(_S("Metadata received for '%s'"), torrent->GetName());

			wxString torrentFilepath = MergePath(mDataDir, torrent->GetHash() + wxS(".torrent"));
			if (!wxFileExists(torrentFilepath)) {
				if (torrent->IsPaused()) {
					// Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
					// and the torrent can be paused when metadata is received

					wxCommandEvent evt(DOWNLOAD_PAUSED);
					evt.SetEventObject(this);
					evt.SetClientData(torrent);
					ProcessEvent(evt);
				}
			}
		}
	}
	else if (auto concreteAlert = alert_cast<const torrent_deleted_alert>(alert)) { // signaled after torrent files was deleted
		m_deletedTorrents.push_back(concreteAlert->handle);
	}
	else if (auto concreteAlert = alert_cast<save_resume_data_alert>(alert)) {
		const torrent_handle& handle = concreteAlert->handle;
		if (handle.is_valid()) {
			
			if (!TorrentFromHandle(handle))
				return;

			if (concreteAlert->resume_data) {
				TorrentDownloadItem* torrent = TorrentFromHandle(handle);
				DCHECK(torrent);

				wxString resumeFilepath = GetResumeFilePath(torrent->GetHandle().info_hash());
				bool saved = SaveResumeDataToFile(resumeFilepath, *concreteAlert->resume_data);
				DCHECK(saved);
			}
		}
	}
	else if (auto concreteAlert = alert_cast<state_update_alert>(alert)) {
		const std::vector<torrent_status>& status = concreteAlert->status;
		for (const torrent_status& currStatus : status) {
			torrent_handle handle = currStatus.handle;
			auto it = std::find(m_cachedTorrents.cbegin(), m_cachedTorrents.cend(), handle);
			if (it != m_cachedTorrents.end()) {
				if (!handle.status().paused) {
					wxDiskspaceSize_t freeBytes = 0;
					//torrent_status status =  handle.status();
					bool ok = wxGetDiskSpace(handle.save_path(), nullptr, &freeBytes);
					if (!ok || freeBytes <= kMinSpaceAllowed) {
						handle.auto_managed(false);
						handle.pause();
					}
				}

				continue;
			}

			TorrentDownloadItem* torrent = TorrentFromHandle(handle);
			if (torrent) {
				DCHECK(torrent && torrent->IsValid());

				if (torrent->HasMetadata()) {
					auto it = std::find(m_needToSaveTorrents.cbegin(), m_needToSaveTorrents.cend(), handle);
					if (it != m_needToSaveTorrents.cend()) {
						wxString torrentFilepath = GetTorrentFilePath(torrent->GetHash());
						if (torrent->Save(torrentFilepath))
							m_needToSaveTorrents.erase(it);
					}

					auto magnetIt = std::find_if(m_waitedMagnets.cbegin(), m_waitedMagnets.cend(), [torrent](const MagnetInfo& info) {
						return info.torrent == torrent;
					});
					if (magnetIt != m_waitedMagnets.cend())
						m_waitedMagnets.erase(magnetIt);
				}

				if (m_currDownlaodLimit > 0 && m_currDownlaodLimit != torrent->GetDownloadLimit())
					torrent->SetDownloadLimit(m_currDownlaodLimit);

				m_maxRememberedSpeed = std::max(m_maxRememberedSpeed, torrent->GetDownloadSpeedBytes());

				wxCommandEvent evt(DOWNLOAD_UPDATED);
				evt.SetEventObject(this);
				evt.SetClientData(torrent);
				ProcessEvent(evt);

				
				if (torrent->GetDownloadSpeedBytes() > 0) {
					wxString msg = wxString::Format(wxS(" name = %s progress = %d dnspeed = %s"),
													torrent->GetName(), torrent->GetProgressPercents(), torrent->GetDownloadSpeedBytesString());
					//int h = 0;
					//wxLogMessage(msg);
				}
			}
		}
	}
	else if (auto concreteAlert = alert_cast<state_changed_alert>(alert)) {
		TorrentDownloadItem* torrent = TorrentFromHandle(concreteAlert->handle);
		if (torrent) {
			DCHECK(torrent && torrent->IsValid());
			DownloadItem::State prevState = ConvertLibtorrentState(concreteAlert->prev_state);
			DownloadItem::State currState = ConvertLibtorrentState(concreteAlert->state);

			wxLogMessage(_S("Torrent state: '%s (%s)' -> '%s (%s)'. Name = %s progress = %d dnspeed = %s size = %s"),
						 DownloadStateToString(prevState), ConvertLibtorrentStateToString(concreteAlert->prev_state), 
						 DownloadStateToString(currState), ConvertLibtorrentStateToString(concreteAlert->state),
						 torrent->GetName(), torrent->GetProgressPercents(), torrent->GetDownloadSpeedBytesString(), torrent->GetTotalBytesString());
		}
	}
	else if (auto concreteAlert = alert_cast<const torrent_delete_failed_alert>(alert)) {
		TorrentDownloadItem* torrent = TorrentFromHandle(concreteAlert->handle);
		if (torrent) {
			wxLogWarning(_S("Torrent delete failed '%s' %s"), torrent->GetName(), torrent->GetHash());
		}
	}
	else if (auto concreteAlert = alert_cast<const save_resume_data_failed_alert>(alert)) {
		TorrentDownloadItem* torrent = TorrentFromHandle(concreteAlert->handle);
		if (torrent) {
			wxLogWarning(_S("Save resume data failed for '%s' %s"), torrent->GetName(), torrent->GetHash());
		}
		DFAIL;
	}
	else if (auto concreteAlert = alert_cast<const fastresume_rejected_alert>(alert)) {
		TorrentDownloadItem* torrent = TorrentFromHandle(concreteAlert->handle);
		if (torrent) {
			wxLogWarning("Fastresume rejected for name = %s hash = %s. '%s'", torrent->GetName(), torrent->GetHash(), concreteAlert->message());
		}
	}
	else if (auto concreteAlert = alert_cast<const read_piece_alert>(alert)) {
		int p = concreteAlert->piece;
		wxLogWarning(_S("read_piece_alert: piece = %d"), p);
	}
	else if (auto concreteAlert = alert_cast<const piece_finished_alert>(alert)) {
		/*TorrentDownloadItem* torrent = TorrentFromHandle(concreteAlert->handle);
		if (torrent) {
			wxLogVerbose(_S("Piece %d finished. Name = %s"), concreteAlert->piece_index, torrent->GetName());
		}*/
	}
}

void TorrentClient::SaveTorrentsQueue()
{
	wxString savePath = MergePath(mDataDir, _S("Torrents"));
	savePath = MergePath(savePath, QueueDumpFilename);
	bool droppped = ClearFile(savePath);
	CHECK_RET(droppped);

	wxFileConfig config{wxEmptyString, wxEmptyString, savePath, wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH};
	for (const std::unique_ptr<TorrentDownloadItem>& torrent : m_torrents) {
		wxString hash = torrent->GetHash();
		config.SetPath(hash);

		config.Write(_S("Magnet"), torrent->GetMagnet());
		config.Write(_S("Name"), torrent->GetName());
		config.Write(_S("SaveDir"), torrent->GetSaveDir());
		config.Write(_S("Paused"), torrent->IsPaused());
		config.Write(_S("MaxRememberedSpeed"), GetMaxRememberedSpeed());

		int speedLimit = torrent->GetDownloadLimit();
		config.Write(_S("SpeedLimit"), speedLimit);

		wxString ticks = wxString::Format(_S("%lld"), torrent->GetAddedTime().GetTicks());
		config.Write(_S("AddedTime"), ticks);

		config.SetPath(_S("/"));
	}

	for (const MagnetInfo& magnetInfo : m_waitedMagnets) {
		wxString hash = TorrentTools::MagnetUrlToHashString(magnetInfo.url);
		config.SetPath(hash);

		config.Write(_S("Magnet"), magnetInfo.url);
		config.Write(_S("Name"), magnetInfo.name);
		config.Write(_S("SaveDir"), magnetInfo.saveDir);
		config.Write(_S("Paused"), magnetInfo.paused);
		config.Write(_S("MaxRememberedSpeed"), -1);
		config.Write(_S("SpeedLimit"), -1);
		config.Write(_S("AddedTime"), 0);

		config.SetPath(_S("/"));
	}

	config.Flush();
}

void TorrentClient::LoadTorrentsQueue()
{
	wxString savePath = MergePath(mDataDir, _S("Torrents"));
	savePath = MergePath(savePath, QueueDumpFilename);
	if (!wxFileExists(savePath) || wxFileName::GetSize(savePath) == 0)
		return;

	wxFileConfig config{wxEmptyString, wxEmptyString, savePath, wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH};
	long grIndex;
	wxString torrentHash;
	bool hasEntry = config.GetFirstGroup(torrentHash, grIndex);
	while (hasEntry) {
		config.SetPath(torrentHash);
		wxString magnet = config.Read(_S("Magnet"));
		wxString name = config.Read(_S("Name"));
		wxString saveDir = config.Read(_S("SaveDir"));
		bool paused;
		config.Read(_S("Paused"), &paused, false);

		int maxRememberedSpeed;
		config.Read(_S("MaxRememberedSpeed"), &maxRememberedSpeed, -1);

		int speedLimit;
		config.Read(_S("SpeedLimit"), &speedLimit, -1);

		wxDateTime addedTime;
		wxString ticksStr = config.Read(_S("AddedTime"));
		if (!ticksStr.IsEmpty()) {
			long long ticks;
			bool ok = ticksStr.ToLongLong(&ticks);
			if (ok) {
				addedTime.Set(ticks);
			}
		}

		TorrentDownloadItem* torrent{};
		wxString torrentFilePath = GetTorrentFilePath(torrentHash);
		if (wxFileExists(torrentFilePath)) {
			torrent = AddTorrent(torrentFilePath, saveDir, (paused == 1));
		}
		else if (!magnet.IsEmpty()) {
			torrent = AddMagnet(magnet, name, saveDir, (paused == 1));
		}
		else {
			DFAIL;
			continue;
		}

		if (!torrent) {
			DFAIL;
			continue;
		}

		torrent->SetDownloadLimit(speedLimit);
		if (maxRememberedSpeed > 0) {
			m_maxRememberedSpeed = std::max(m_maxRememberedSpeed, maxRememberedSpeed);
		}
		if (addedTime.IsValid()) {
			torrent->SetAddedTime(addedTime);
		}

		config.SetPath(_S("/"));

		hasEntry = config.GetNextGroup(torrentHash, grIndex);
	}
}

TorrentDownloadItem* TorrentClient::TorrentFromHandle(const torrent_handle& handle)
{
	if (!handle.is_valid())
		return nullptr;
	
	auto it = std::find_if(m_torrents.begin(), m_torrents.end(), [&](const std::unique_ptr<TorrentDownloadItem>& torrent) {
		return torrent->GetHandle() == handle;
	});

	return it == m_torrents.end() ? nullptr : (*it).get();
}

void TorrentClient::PauseTorrent(const DownloadItem* torrent)
{
	DCHECK(torrent);

	auto it = std::find_if(m_torrents.begin(), m_torrents.end(), [&](const std::unique_ptr<TorrentDownloadItem>& currTorrent) {
		return currTorrent->GetHash() == torrent->GetHash();
	});
	CHECK_RET(it != m_torrents.end());

	TorrentDownloadItem* currTorrent = (TorrentDownloadItem*)it->get();
	currTorrent->Pause();
}

void TorrentClient::StartTorrent(const DownloadItem* torrent)
{
	DCHECK(torrent);

	auto it = std::find_if(m_torrents.begin(), m_torrents.end(), [&](const std::unique_ptr<TorrentDownloadItem>& currTorrent) {
		return currTorrent->GetHash() == torrent->GetHash();
	});
	CHECK_RET(it != m_torrents.end());

	TorrentDownloadItem* currTorrent = (TorrentDownloadItem*)it->get();
	currTorrent->Start();
}

void TorrentClient::DeleteTorrent(const DownloadItem* torrent, bool withFiles /*= false*/)
{
	DCHECK(torrent);
	
	auto it = std::find_if(m_torrents.begin(), m_torrents.end(), [&](const std::unique_ptr<TorrentDownloadItem>& currTorrent) {
		return currTorrent->GetHash() == torrent->GetHash();
	});
	DCHECK(it != m_torrents.end());

	std::unique_ptr<TorrentDownloadItem>& currTorrent = *it;

	torrent_handle handle = currTorrent->GetHandle();
	DCHECK(handle.is_valid());

	wxCommandEvent evt(DOWNLOAD_DELETING);
	evt.SetEventObject(this);
	evt.SetClientData((TorrentDownloadItem*)currTorrent.get()); // TODO: remove this hack
	ProcessEvent(evt);

	wxString resumePath = GetResumeFilePath(handle.info_hash());
	if (wxFileExists(resumePath))
		wxRemoveFile(resumePath);

	it = std::find_if(m_torrents.begin(), m_torrents.end(), [&](const std::unique_ptr<TorrentDownloadItem>& currTorrent) {
		return (currTorrent->GetHash().CmpNoCase(torrent->GetHash()) == 0);
	});
	DCHECK(it != m_torrents.end());
	
	wxString torrentFilePath = GetTorrentFilePath(it->get()->GetHash());
	wxRemoveFile(torrentFilePath);
	wxString resumeFilepath = GetResumeFilePath(it->get()->GetHandle().info_hash());
	wxRemoveFile(resumeFilepath);

	auto magnetIt = std::find_if(m_waitedMagnets.cbegin(), m_waitedMagnets.cend(), [torrent](const MagnetInfo& info) {
		return info.torrent == torrent;
	});
	if (magnetIt != m_waitedMagnets.cend())
		m_waitedMagnets.erase(magnetIt);

	m_torrents.erase(it);

	m_session.remove_torrent(handle, withFiles ? session::delete_files : 0);

	SaveTorrentsQueue();
}

void TorrentClient::SetDownloadLimit(const DownloadItem* item, wxInt32 bytes)
{
	DCHECK(item);

	auto it = std::find_if(m_torrents.begin(), m_torrents.end(), [&](const std::unique_ptr<TorrentDownloadItem>& currTorrent) {
		return currTorrent->GetHash() == item->GetHash();
	});
	DCHECK(it != m_torrents.end());

	std::unique_ptr<TorrentDownloadItem>& currTorrent = *it;

	torrent_handle handle = currTorrent->GetHandle();
	DCHECK(handle.is_valid());

	handle.set_download_limit(bytes);
}

void TorrentClient::SetDownloadLimit(wxInt32 bytes)
{
	m_currDownlaodLimit = bytes;

	for (std::unique_ptr<TorrentDownloadItem>& torrent : m_torrents) {
		torrent->SetDownloadLimit(m_currDownlaodLimit);
	}
}

bool TorrentClient::AddCachedTorrent(const wxString& torrentFilePath)
{
	wxMemoryBuffer torrentFileBuff = FilesystemTools::ReadFile(torrentFilePath);
	if (torrentFileBuff.IsEmpty()) {
		wxLogError(_S("Torrent load failed"));
		return false;
	}

	return AddCachedTorrent(torrentFileBuff);
}

bool TorrentClient::AddCachedTorrent(const wxMemoryBuffer& torrentFileData)
{
	error_code ec;
	add_torrent_params addTorrentParams;
	boost::intrusive_ptr<torrent_info> ti(new torrent_info(torrentFileData, range_cast<int>(torrentFileData.GetDataLen()), ec));
	if (ec) {
		wxLogError(_("Invalid torrent file:\n") + ec.message().c_str());
		return false;
	}

	if (ti->num_files() == 0) {
		wxLogWarning(_("Torrent file is empty"));
		return false;
	}

	addTorrentParams.ti = ti;
	addTorrentParams.info_hash = ti->info_hash();

	// Check for duplicate torrent
	const torrent_handle& existedTorrent = m_session.find_torrent(addTorrentParams.info_hash);
	if (existedTorrent.is_valid())
		return true;

	addTorrentParams.flags = add_torrent_params::default_flags;
	addTorrentParams.flags |=
		add_torrent_params::flag_paused
		| add_torrent_params::flag_auto_managed
		| add_torrent_params::flag_update_subscribe;

	addTorrentParams.save_path = m_cahedDir.ToUTF8();
	addTorrentParams.storage_mode = storage_mode_sparse;

	torrent_handle handle = m_session.add_torrent(addTorrentParams, ec);
	if (ec) {
		wxLogError(_("Invalid torrent file:\n") + ec.message().c_str());
		return nullptr;
	}
	DCHECK(handle.is_valid());

	bool torrentPaused = handle.status().paused;
	if (torrentPaused)
		handle.resume();

	m_cachedTorrents.push_back(handle);

	return true;
}

bool TorrentClient::AddCachedMagnet(const wxString& magnetUrl, const wxString& name)
{
	error_code ec;
	add_torrent_params addTorrentParams;
	parse_magnet_uri(magnetUrl.ToUTF8().data(), addTorrentParams, ec);
	if (ec) {
		wxLogError(_("Invalid magnet url:\n") + ec.message().c_str());
		return false;
	}

	// TODO: add url downloading fail processing
	addTorrentParams.url = magnetUrl;
	addTorrentParams.name = name;
	
	// Check for duplicate torrent
	const torrent_handle& existedTorrent = m_session.find_torrent(addTorrentParams.info_hash);
	if (existedTorrent.is_valid())
		return true;

	addTorrentParams.flags = add_torrent_params::default_flags;
	addTorrentParams.flags |=
		add_torrent_params::flag_paused
		| add_torrent_params::flag_auto_managed
		| add_torrent_params::flag_update_subscribe;

	FilesystemTools::CreateDir(m_cahedDir);

	addTorrentParams.save_path = m_cahedDir.ToUTF8();
	addTorrentParams.storage_mode = storage_mode_sparse;

	torrent_handle handle = m_session.add_torrent(addTorrentParams, ec);
	if (ec) {
		wxLogError(_("Invalid torrent file:\n") + ec.message().c_str());
		return nullptr;
	}
	DCHECK(handle.is_valid());

	bool torrentPaused = handle.status().paused;
	if (torrentPaused)
		handle.resume();

	m_cachedTorrents.push_back(handle);

	return true;
}

void TorrentClient::ClearCachedDownloads()
{
	for (torrent_handle& handle : m_cachedTorrents) {
		m_session.remove_torrent(handle, session::delete_files);
	}
	m_cachedTorrents.clear();
}

wxInt32 TorrentClient::GetMaxRememberedSpeed() const
{
	return m_maxRememberedSpeed;
}

void TorrentClient::ResetMaxRememberedSpeed()
{
	m_maxRememberedSpeed = -1;
}

void TorrentClient::EnableFullDownloadAllocation(bool enable)
{
	mFullAllocation = enable;
}

void TorrentClient::DeleteAllTorrents(bool withFiles /*= false*/)
{
	auto it = m_torrents.begin();
	while (!m_torrents.empty()) {
		DeleteTorrent(m_torrents.begin()->get(), withFiles);
	}

	m_torrents.clear();
}

void TorrentClient::SyncDeleteAllTorrents(bool withFiles /*= false*/)
{
	std::vector<torrent_handle> torrentsToDelete = m_session.get_torrents();
	DeleteAllTorrents(withFiles);

	auto it = torrentsToDelete.cbegin();
	while (it != torrentsToDelete.cend()) {
		auto tit = std::find(m_deletedTorrents.cbegin(), m_deletedTorrents.cend(), *it);
		if (tit == m_deletedTorrents.cend()) {
			wxMilliSleep(20);
		}
		else {
			++it;
		}
	}
}

wxString TorrentClient::GetTorrentFilePath(const wxString& hash) const
{
	wxString torrentFilepath = hash + _S(".torrent");
	return MergePath(mTorrentsDir, torrentFilepath);
}

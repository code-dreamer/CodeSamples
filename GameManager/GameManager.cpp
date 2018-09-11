#include "stdafx.h"

#include <boost/cast.hpp>

#include "GameManager.h"
#include "GamesContainer.h"
#include "Game.h"
#include "MainModuleKeeper.h"
#include "GamesAdditionalData.h"
#include "ServerManagerInterface.h"
#include "FileSystemTools.h"
#include "GameManagerSettings.h"
#include "StringTools.h"
#include "ConfigPlaces.h"
#include "UserAuthData.h"
#include "GUIManagerInterface.h"
#include "ModulesFactoryInterface.h"
#include "DownloaderInterface.h"
#include "MainModuleSettingsInterface.h"
#include "GameParsers.h"
#include "AsyncImageDownloader.h"
#include "MainModuleKeeper.h"
#include "ExceptionInterface.h"
#include "ExceptionHandler.h"
#include "GamesFinder.h"
#include "GameTools.h"
#include "MainModuleKeeper.h"

const QString s_gamesDumpFilename = STR("Games.dat");
const QString s_versionGuarder = STR("{7CAD5789-9101-4F46-B873-B66AF7321523}");
const int s_downloaderInforFetchTimeMs = 200;
const QString s_translationsDir = STR(":/GameManagement/Translations");
const int s_gameCoversDownloadTimeoutMs = 1000;

std::unique_ptr<GamesAdditionalData> GameManager::s_gamesAdditionalData( new GamesAdditionalData() );

GameManager::GameManager(QObject* const parent)
	: QObject(parent)
	, downloader_(nullptr)
	, gamesContainer_(nullptr)
	, wasShutdown_(false)
	, initComplete_(false)
	, settings_(nullptr)
	, imageDownloader_(nullptr)
	, coverDownloadingTimer_(nullptr)
{
	downloader_ = MainModuleKeeper::GetMainModule()->GetModulesFactory()->CreateDownloader(this);
	CHECKED_CONNECT(downloader_->ToQObject(), SIGNAL(ErrorMessageReady(QString, ErrorSeverity)), this, SLOT(OnErrorMessageReady(QString, ErrorSeverity)));
	downloader_->SetUpdateTimeout(s_downloaderInforFetchTimeMs);
	
	gamesContainer_ = new GamesContainer(this, this);
	imageDownloader_ = new AsyncImageDownloader(this);
	CHECKED_CONNECT(imageDownloader_, SIGNAL(ImageReady(QUrl, QImage)), this, SLOT(OnCoverImageDownloaded(QUrl, QImage)));
	
	coverDownloadingTimer_ = new QTimer(this);
	CHECKED_CONNECT(coverDownloadingTimer_, SIGNAL(timeout()), this, SLOT(OnCoverDownloadingTimer()));
}

GameManager::~GameManager()
{
	G_ASSERT(wasShutdown_);
	if (!wasShutdown_) {
		Shutdown();
	}
}

void GameManager::Init()
{	
	wasShutdown_ = false;
	s_gamesAdditionalData->LoadData();
	settings_ = new GameManagerSettings(this);
	LoadTranslation();
	gamesContainer_->InitGameGroups();
	downloader_->Init();
	initComplete_ = true;
}

void GameManager::Shutdown()
{
	downloader_->Shutdown();
	wasShutdown_ = true;
}

void GameManager::OnErrorMessageReady(QString message, ErrorSeverity severity) const
{
#ifdef DEBUG
	ErrorSeverity fromSeverity = WarningErrorSeverity;
#else
	ErrorSeverity fromSeverity = CriticalErrorSeverity;
#endif // DEBUG

	if (severity >= fromSeverity) {
		Q_EMIT ErrorMessageReady(message, severity);
	}
}

void GameManager::InitUserSession()
{
	const QString userId = MainModuleKeeper::GetMainModule()->GetServerManager()->GetUserAuthData().GetCurrentUserId();
	settings_->GetConfigPlaces()->AddUserPaths(userId);

	downloader_->InitUserSession();

	LoadGames();

	if (MainModuleKeeper::GetMainModule()->IsTestModeEnabled()) {
		AddTestGames();
	}

	CheckIfGamesInstalled();
}

void GameManager::ShutdownUserSession()
{
	SaveGames();

	downloader_->ShutdownUserSession();
	settings_->GetConfigPlaces()->RemoveCurrentUserPaths();
}

void GameManager::SaveGames()
{
	const QString dataDir = GameManagerSettings::GetSettings()->GetConfigPlaces()->GetUserConfigDataDir();

	QFile file( FileSystemTools::MergePath(dataDir, s_gamesDumpFilename) );
	bool isOpened = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
	if (!isOpened) {
		FAKE_THROW("Can't save games");
	}

	QDataStream dataStream(&file);
	dataStream << s_versionGuarder;
	GetGamesContainer()->Store(dataStream);
}

void GameManager::LoadGames()
{
	const QString dataDir = GameManagerSettings::GetSettings()->GetConfigPlaces()->GetUserConfigDataDir();

	QFile file( FileSystemTools::MergePath(dataDir, s_gamesDumpFilename) );
	if (file.open(QIODevice::ReadOnly)) {
		QDataStream dataStream(&file);

		QString versionGuid;
		dataStream >> versionGuid;
		G_ASSERT(versionGuid == s_versionGuarder);
		if (versionGuid == s_versionGuarder) {
			GetGamesContainer()->Load(dataStream);
		}
	}
}

inline GameManagerSettingsInterface* GameManager::GetSettings() const
{
	CHECK_PTR(settings_);
	return settings_;
}

inline GamesContainerInterface* GameManager::GetGamesContainer() const
{
	CHECK_PTR(gamesContainer_);
	return gamesContainer_;
}

inline DownloaderInterface* GameManager::GetDownloader() const
{
	CHECK_PTR(downloader_);
	return downloader_;
}

void GameManager::AddTestGames()
{
	QFileInfo fileInfo(STR(":/GameManagement/101pet_Penguin.exe.torrent"));
	QString torrentfilePath = QString(STR("%1\\%2")).arg(GameManagerSettings::GetSettings()->GetConfigPlaces()->GetConfigTempDir(), 
		fileInfo.fileName());
	if ( !QFileInfo(torrentfilePath).exists() ) {
		if (!QFile::copy(fileInfo.canonicalFilePath(), torrentfilePath)) {
			qWarning() << "Can't copy " << fileInfo.canonicalFilePath() << "to" << torrentfilePath;
			return;
		}
	}

	QString gameId = STR("{9F4987E4-EAA3-4823-8615-BDC105F6FE5F}");
	GameInterface* const existedGame = gamesContainer_->GetGame(gameId);
	if (existedGame == nullptr) {
		Game* const game = new Game();
		game->SetId( STR("{9F4987E4-EAA3-4823-8615-BDC105F6FE5F}") );
		game->SetName( STR("101 Pet Penguin") );
		game->SetPlayTime( QTime(0,0) );
		game->SetState(GameStateInterface::SubscribedState);

		QUrl downloadUrl = QUrl::fromLocalFile(torrentfilePath);
		game->SetDownloadUrl(downloadUrl);

		game->SetDemoFlag(false);

		game->SetPublisher( StringTools::CreateQString("Руссобит-М") );
		game->SetDeveloper( STR("PlayPets & Selectsoft Games ") );

		game->SetRunFilename(STR("PP2_Penguin.exe"));

		GetGamesContainer()->AddGame(game);
	}
}

void GameManager::UpdateGames(const QDomElement& xmlDescription)
{
	const QList<GameInterface*> parsedGames = GameParsers::ParseFromXml(xmlDescription);
	UpdateGames(parsedGames);
}

void GameManager::OnCoverImageDownloaded(const QUrl url, const QImage cover)
{
	G_ASSERT(url.isValid());

	Game* game = nullptr;
	
	const int gamesCount = gamesContainer_->GetCount();
	for (int i = 0; i < gamesCount; ++i) {
		GameInterface* const currGame = gamesContainer_->GetGame(i);
		if ( currGame->GetCoverUrl() == url ) {
			game = boost::polymorphic_downcast<Game*>(currGame);
			break;
		}
	}
	CHECK_PTR(game);

	if ( cover.isNull() ) { // try to download again
		if ( !pendingGamesForUpdateCovers_.contains(game) ) {
			pendingGamesForUpdateCovers_.push_back(game);
		}
	}
	else {
		const int ind = pendingGamesForUpdateCovers_.indexOf(game);
		if (ind != -1) {
			pendingGamesForUpdateCovers_.removeAt(ind);
		}

		game->SetCoverImage(cover);
	}
}

void GameManager::OnCoverDownloadingTimer()
{
	const int gamesCount = gamesContainer_->GetCount();
	bool allCoversDownloaded = true;
	for (int i = 0; i < gamesCount; ++i) {
		GameInterface* const game = gamesContainer_->GetGame(i);
		if ( game->GetCoverUrl().isValid() && game->IsDefaultCoverImageSetted() ) {
			allCoversDownloaded = false;
			break;
		}
	}
	if (allCoversDownloaded) {
		coverDownloadingTimer_->stop();
	}
	else {
		Q_FOREACH(Game* const game, pendingGamesForUpdateCovers_) {
			const QUrl& coverUrl = game->GetCoverUrl();
			if (coverUrl.isValid() && !imageDownloader_->IsAsyncDownloadInProcess(coverUrl)) {
				imageDownloader_->AsyncDownloadImage(coverUrl);
			}
		}
	}
}

void GameManager::UpdateGames(const QList<GameInterface*>& games)
{
	QList<GameInterface*> addedGames;

	Q_FOREACH(GameInterface* const game, games) {
		gamesContainer_->AddGame(game);
	}

	QList<Game*> containerGames = gamesContainer_->GetGames();
	Q_FOREACH(GameInterface* const game, containerGames) {
		if ( game->GetCoverUrl().isValid() && game->IsDefaultCoverImageSetted() ) {
			pendingGamesForUpdateCovers_.push_back( boost::polymorphic_downcast<Game*>(game) );
		}
	}
	if (!pendingGamesForUpdateCovers_.isEmpty()) {
		QTimer::singleShot(0, this, SLOT(OnCoverDownloadingTimer()));
		coverDownloadingTimer_->start(s_gameCoversDownloadTimeoutMs);
	}
}

inline QObject* GameManager::ToQObject()
{
	return this;
}

inline const QObject* GameManager::ToQObject() const
{
	return this;
}

Game* GameManager::CreateGame(QObject* const parent)
{
	Game* const game = new Game(parent);
	game->SetId(QUuid::createUuid().toString());
	game->SetState(GameStateInterface::SubscribedState);
	game->SetPlayTime( QTime(0,0) );

	return game;
}

const GamesAdditionalData* GameManager::GetGamesAdditionalData()
{
	CHECK_PTR(s_gamesAdditionalData);
	return s_gamesAdditionalData.get();
}

void GameManager::LoadTranslation()
{
	const QString localeName = MainModuleKeeper::GetMainModule()->GetSettings()->GetCurrentLocale().name();

	if (!moduleTranslator_.isEmpty()) {
		QApplication::removeTranslator(&moduleTranslator_);
	}

	QString translationFilename = QString(STR("%1%2%3")).arg(GameManagerSettings::GetModuleName(), STR("_"), localeName);
	const bool loaded = moduleTranslator_.load(translationFilename, s_translationsDir);
	G_ASSERT(loaded);

	QApplication::installTranslator(&moduleTranslator_);
}

QList<GameInterface*> GameManager::SearchGames()
{
	QList<GameInterface*> foundedGames;

	QFileInfoList drives = QDir::drives();
	Q_FOREACH(const QFileInfo& drive, drives) {
		const std::wstring drivePathStd = StringTools::CreateStdWString(drive.canonicalPath());
		if (GetDriveType(drivePathStd.c_str()) == DRIVE_FIXED) {
			foundedGames << SearchGamesInFolder(drive.canonicalPath());
		}
	}

	return foundedGames;
}

QList<GameInterface*> GameManager::SearchGamesInFolder(const QString& folderPath)
{
	QSTRING_NOT_EMPTY(folderPath);

	const GamesFinder finder;
	return finder.FindGamesInFolder(folderPath);
}

QList<GameInterface*> GameManager::SearchGamesInRegister()
{
	GamesFinder finder;
	return finder.FindGamesByRegKeys();
}

void GameManager::CheckIfGamesInstalled()
{
	CHECK_PTR(gamesContainer_);

	for (int i = 0, count = gamesContainer_->GetCount(); i < count; ++i) {
		GameInterface* game = gamesContainer_->GetGame(i);
		if ( game->GetState()->GetStateInfo()->GetStateId() == GameStateInterface::InstalledState ) {
			if ( GameTools::GameWasDeleted(game) ) {
				game->SetState(GameStateInterface::DownloadedState);
			}
		}
	}
}

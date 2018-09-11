#pragma once

#include "GameManagerInterface.h"

class GamesContainer;
class DownloadFrameworkInterface;
class GameManagerSettingsInterface;
class GameManagerSettings;
class Game;
class GamesAdditionalData;
class AsyncImageDownloader;

class GAME_MANAGEMENT_API GameManager : public QObject, public GameManagerInterface
{
	Q_OBJECT

public:
	static Game* CreateGame(QObject* parent = nullptr);
	static const GamesAdditionalData* GetGamesAdditionalData();

	explicit GameManager(QObject* parent = nullptr);
	virtual ~GameManager();

// GameManagerInterface implementation
public:
	virtual GamesContainerInterface* GetGamesContainer() const override;
	virtual DownloaderInterface* GetDownloader() const override;
	virtual void UpdateGames(const QDomElement& xmlDescription) override;
	virtual void UpdateGames(const QList<GameInterface*>& games) override;
	virtual QList<GameInterface*> SearchGames() override;
	virtual QList<GameInterface*> SearchGamesInFolder(const QString& folderPath) override;
	virtual QList<GameInterface*> SearchGamesInRegister() override;

//////////////////////////////////////////////////////////////////////////
Q_SIGNALS:
	void ErrorMessageReady(QString message, ErrorSeverity severity) const override;
//////////////////////////////////////////////////////////////////////////

// ModuleInterface implementation
public:
	virtual void Init() override;
	virtual void InitUserSession() override;
	virtual void ShutdownUserSession() override;
	virtual GameManagerSettingsInterface* GetSettings() const override;

public Q_SLOTS:
	virtual void Shutdown() override;

// ToQObjectConvertableInterface implementation
public:
	virtual QObject* ToQObject() override;
	virtual const QObject* ToQObject() const override;
//////////////////////////////////////////////////////////////////////////

private:
	void LoadTranslation();
	void LoadGames();
	void SaveGames();
	void AddTestGames();
	void CheckIfGamesInstalled();
	
private Q_SLOTS:
	void OnCoverDownloadingTimer();
	void OnCoverImageDownloaded(QUrl url, QImage cover);
	void OnErrorMessageReady(QString message, ErrorSeverity severity) const;

private:
#pragma warning(push)
#pragma warning(disable : 4251)
	static std::unique_ptr<GamesAdditionalData> s_gamesAdditionalData;
	QList<Game*> pendingGamesForUpdateCovers_;
#pragma warning(pop)

	GameManagerSettings* settings_;
	DownloaderInterface* downloader_;
	GamesContainer* gamesContainer_;
	bool wasShutdown_;
	bool initComplete_;
	QTranslator moduleTranslator_;
	AsyncImageDownloader* imageDownloader_;
	QTimer* coverDownloadingTimer_;
};

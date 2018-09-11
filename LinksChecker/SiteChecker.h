#pragma once

class Site;

class SiteChecker : public QObject
{
	Q_OBJECT

public:
	SiteChecker(QObject* parent = nullptr);
	virtual ~SiteChecker() {}

signals:
	void searchProgressChanged(int value);

public:
	void addSite(Site* site);
	void startChecking();
	void stopChecking();

private slots:
		void authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
		void replyFinished(QNetworkReply* reply);
		void timeoutWatcher();
#ifndef QT_NO_OPENSSL
		void sslErrors(QNetworkReply* reply, QList<QSslError> errors);
#endif

private:
	Site* siteFromReply(QNetworkReply* reply) const;
	qint32 finishedSites();
	struct SiteProcessInfo;

private:
	struct SiteProcessInfo
	{
		SiteProcessInfo() : visitedLinksCount(0) {}
		QList<QNetworkReply*> replies;
		qint32 visitedLinksCount;
		QTime startedTime;
	};
	QHash<Site*, SiteProcessInfo> pendingSites_;
	int initialSitesCount_;

	bool inShutdown_;
	bool replyBusyNow_;
	QNetworkAccessManager networkManager_;

	QTimer* timeoutWatcher_;
};

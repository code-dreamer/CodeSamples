#include "stdafx.h"
#include "SiteChecker.h"
#include "Site.h"
#include "Tools/QtHelpers.h"

const int kMaxLinksOnPage = 10;
const int kSiteTimeoutMs = 30 * 1000; // 10 sec.
const int kSiteTimeoutCheckMs = 20;

namespace {

QString prepareUrlForSearch(const QUrl& url)
{
	DCHECK(url.isValid());

	QString result;

	if (url.isValid()) {
		result = url.toString(QUrl::RemoveScheme | QUrl::RemovePassword | QUrl::RemoveUserInfo | QUrl::RemovePort | QUrl::RemoveQuery 
			| QUrl::RemoveFragment | QUrl::StripTrailingSlash);

		QString str = _S("//");
		if (result.startsWith(str)) {
			result.remove(0, str.length());
		}
	}

	CHECK_QSTRING(result);
	return result;
}
} // namespace

SiteChecker::SiteChecker(QObject* parent)
	: QObject(parent)
	, initialSitesCount_(0)
	, replyBusyNow_(false)
	, inShutdown_(false)
	, timeoutWatcher_(nullptr)
{
	using namespace Tools::QtHelpers;
	checkedConnect(&networkManager_, &QNetworkAccessManager::authenticationRequired, this, &SiteChecker::authenticationRequired);
	checkedConnect(&networkManager_, &QNetworkAccessManager::finished, this, &SiteChecker::replyFinished);
#	ifndef QT_NO_OPENSSL
	checkedConnect(&networkManager_, &QNetworkAccessManager::sslErrors, this, &SiteChecker::sslErrors);
#	endif

	timeoutWatcher_ = new QTimer(this);
	checkedConnect(timeoutWatcher_, &QTimer::timeout, this, &SiteChecker::timeoutWatcher);
}

void SiteChecker::addSite(Site* site)
{
	DCHECK_PTR(site);

	DCHECK( !pendingSites_.contains(site) );
	pendingSites_.insert(site, SiteProcessInfo());
}

void SiteChecker::startChecking()
{
	inShutdown_ = false;
	initialSitesCount_ = pendingSites_.count();

	for (auto it = pendingSites_.begin(); it != pendingSites_.end(); ++it) {
		Site* site = it.key();
		site->setCheckResult(CheckResult::notChecked);
		site->setLastCheckDate(QDateTime::currentDateTime());

		QNetworkRequest request(site->linkedUrl());
		request.setHeader(QNetworkRequest::UserAgentHeader, _S("Mozilla/5.0 (Windows NT 6.1; WOW64; rv:19.0) Gecko/20100101 Firefox/19.0"));

		QNetworkReply* reply = networkManager_.get(request);
		CHECK_PTR(reply);
		if (reply == nullptr) {
			APP_LOG_WARNING() << "Failed to create reply for site" << site->linkedUrl().toString();
			site->setCheckResult(CheckResult::searchFailed);
		}
		else {
			it.value().startedTime.start();
			it.value().replies << reply;
			it.value().visitedLinksCount++;
			site->setCheckResult(CheckResult::inProgress);
		}
	}

	timeoutWatcher_->start(kSiteTimeoutCheckMs);
}

void SiteChecker::timeoutWatcher()
{
	if (replyBusyNow_) {
		return;
	}

	QHash<Site*, SiteProcessInfo> timeoutedSites;

	for (auto it = pendingSites_.begin(); it != pendingSites_.end(); ++it) {
		const QTime& startedTime =  it.value().startedTime;
		if (!startedTime.isValid()) {
			APP_LOG_WARNING() << "Started time is invalid";
			continue;
		}
		if (startedTime.elapsed() > kSiteTimeoutMs) {
			timeoutedSites[it.key()] = it.value();
		}
	}

		for (auto it = timeoutedSites.begin(); it != timeoutedSites.end(); ++it) {
			SiteProcessInfo& info = it.value();
			foreach(QNetworkReply* reply, info.replies) {
				reply->abort();
			}

			pendingSites_.remove(it.key());
		}
}

void SiteChecker::stopChecking()
{
	inShutdown_ = true;
	SCOPE_EXIT {
		inShutdown_ = false;
	};
	while (replyBusyNow_) {
		qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
		::Sleep(20);
	}
	for (auto it = pendingSites_.begin(); it != pendingSites_.end(); ++it) 	{
		Site* site = it.key();
		if (site->checkResult() == CheckResult::inProgress)
			site->setCheckResult(CheckResult::notChecked);
		SiteProcessInfo& info = it.value();
		foreach(QNetworkReply* reply, info.replies) {
			reply->abort();
		}
	}

	pendingSites_.clear();
	initialSitesCount_ = 0;
	emit searchProgressChanged(0);
}

void SiteChecker::authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
	CHECK_PTR(reply);
	CHECK_PTR(authenticator);

	DCHECK(false); // not implemented
	APP_LOG_WARNING() << "Authentication requested for" << reply->url().toString();
}

#ifndef QT_NO_OPENSSL
void SiteChecker::sslErrors(QNetworkReply* reply, QList<QSslError> errors)
{
	CHECK_PTR(reply);

	QString errorString;
	foreach(const QSslError& error, errors) {
		APP_LOG_WARNING() << "SSL error:" << error;

		if (!errorString.isEmpty()) {
			errorString += _S(", ");
		}
		errorString += error.errorString();
	}

	if (QMessageBox::warning(qApp->activeWindow(), _S("HTTP"), tr("One or more SSL errors has occurred:\n%1").arg(errorString),
		QMessageBox::Ignore | QMessageBox::Abort) == QMessageBox::Ignore) 
	{
		reply->ignoreSslErrors();
	}
}
#endif

void SiteChecker::replyFinished(QNetworkReply* reply)
{
	CHECK_PTR(reply);

	if (inShutdown_) {
		return;
	}

	replyBusyNow_ = true;

	Site* site = nullptr;
	SiteProcessInfo* siteProcessInfo = nullptr;
	for (auto it = pendingSites_.begin(); it != pendingSites_.end(); ++it) {
		if (it.value().replies.contains(reply)) {
			site = it.key();
			siteProcessInfo = &(it.value());
			break;
		}
	}
	if (site == nullptr || siteProcessInfo == nullptr) {
		DCHECK(false);
		return;
	}

	SCOPE_EXIT {
		siteProcessInfo->replies.removeOne(reply);
		reply->deleteLater();
		if (siteProcessInfo->replies.isEmpty()) {
			CheckResult checkResult = site->checkResult();
			if (checkResult == CheckResult::inProgress ||checkResult == CheckResult::notChecked)
				site->setCheckResult(CheckResult::linkNotFound);
			int removedCount = pendingSites_.remove(site);
			DCHECK(removedCount == 1);
			if (pendingSites_.empty()) {
				replyBusyNow_ = false;
				stopChecking();
				return;
			}

			double val = (double)pendingSites_.count()/(double)initialSitesCount_;
			val = 1.0 - val;
			int percVal = qRound(val*100);
			emit searchProgressChanged(percVal);
		}
		replyBusyNow_ = false;
	};

	if (reply->error() != QNetworkReply::NoError) {
		reply->abort();
		if (reply->url() == site->linkedUrl())
			site->setCheckResult(CheckResult::searchFailed);
		APP_LOG_WARNING() << reply->url().toString() << "failed:" << reply->errorString();
		return;
	}

	QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
	if (!redirectionTarget.isNull()) {
		QUrl newUrl = reply->url().resolved(redirectionTarget.toUrl());
		if (newUrl.isValid()) {
			QNetworkRequest request(newUrl);
			request.setHeader(QNetworkRequest::UserAgentHeader, _S("Mozilla/5.0 (Windows NT 6.1; WOW64; rv:19.0) Gecko/20100101 Firefox/19.0"));
			QNetworkReply* newReply = networkManager_.get(request);
			CHECK_PTR(newReply);
			if (reply == nullptr) {
				APP_LOG_WARNING() << "Failed to create reply for" << newUrl.toString();
			}
			else {
				siteProcessInfo->visitedLinksCount++;
				siteProcessInfo->replies.push_back(newReply);
			}
		}
		else {
			APP_LOG_WARNING() << "Invalid redirection target. Current url:" << reply->url().toString() 
				<< "Redirected url:" << newUrl.toString();
		}
		
		return;
	}

	QByteArray pageHtml = reply->readAll();

	QUrl searchedUrl = site->urlOnLinkedSite();
	DCHECK(searchedUrl.isValid());
	QString searchedUrlStr = prepareUrlForSearch(searchedUrl);

	bool isLinkPresent = pageHtml.contains(searchedUrlStr.toLatin1());
	if (isLinkPresent) {
		site->setCheckResult(CheckResult::linkPresent);
	}
	else if (site->checkType() == CheckType::siteWide && siteProcessInfo->visitedLinksCount < kMaxLinksOnPage)	{
		QString pageHtmlStr = QString::fromLatin1(pageHtml.constData());

		const QRegExp linkPattern( _S("href=[\'\"]?([^\'\" >]+)") );

		QList<QUrl> linksOnPage;

		QStringList pageExtenstions;
		pageExtenstions << _S("htm") << _S("html") << _S("xml") << _S("aspx");

		int pos = 0;
		while ( (pos = linkPattern.indexIn(pageHtmlStr, pos)) != -1 ) {
			int captureCount = linkPattern.captureCount();
			DCHECK(captureCount == 1);
			if (captureCount == 1) {
				QString urlStr = linkPattern.cap(1);
				QUrl url(urlStr, QUrl::TolerantMode);
				if (url.isValid()) {
					const QUrl siteUrl = reply->url();
					if (url.isRelative())
						url = siteUrl.resolved(url);

					QString siteUrlHost = siteUrl.host();
					const QString prefix = _S("www.");
					if (siteUrlHost.startsWith(prefix)) {
						siteUrlHost.remove(0, prefix.length());
					}
					QString urlHost = url.host();
					if (urlHost.startsWith(prefix)) {
						urlHost.remove(0, prefix.length());
					}

					if (url != siteUrl && QString::compare(urlHost, siteUrlHost, Qt::CaseInsensitive) == 0)	{
						QFileInfo fileInfo( url.path() );
						QString suffix = fileInfo.suffix().toLower();
						if (suffix.isEmpty() || pageExtenstions.contains(suffix) )
							linksOnPage.push_back(url);	
					}
				}
			}

			pos += linkPattern.matchedLength();
		}

		for (int i = 0; i < linksOnPage.count() && siteProcessInfo->replies.count() < kMaxLinksOnPage; ++i) {
			const QUrl& link = linksOnPage[i];
			DCHECK(link.isValid());

			auto it = std::find_if(siteProcessInfo->replies.cbegin(), siteProcessInfo->replies.cend(), [&link](const QNetworkReply* waitedReply) {
				return (waitedReply->url() == link);
			});
			if (it == siteProcessInfo->replies.cend()) {
				QNetworkRequest request(link);
				request.setHeader(QNetworkRequest::UserAgentHeader, _S("Mozilla/5.0 (Windows NT 6.1; WOW64; rv:19.0) Gecko/20100101 Firefox/19.0"));
				QNetworkReply* newReply = networkManager_.get(request);
				CHECK_PTR(newReply);
				if (newReply == nullptr) {
					APP_LOG_WARNING() << "Failed to create reply for" << newReply->url().toString();
				}
				else {
					siteProcessInfo->visitedLinksCount++;
					siteProcessInfo->replies.push_back(newReply);
				}
			}
		}
	}
	else {
		site->setCheckResult(CheckResult::linkNotFound);
	}
}

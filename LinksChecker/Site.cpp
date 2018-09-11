#include "stdafx.h"
#include "Site.h"
#include "ParseError.h"
#include "Tools/Cpp.h"

#pragma warning(push)
#pragma warning (disable : 4510 4610)
const struct 
{
	const char* name;
	const qint32 propMinWidth;
} 
kProps[] = 
{
	{ "My site",  170},
	{ "His link on my site", 170 },
	{ "His site", 170 },
	{ "My link on his site", 170 },
	{ "Search method", 100 },
	{ "Last checked", 110 },
	{ "Link Present", 80 }
};
#pragma warning(pop)

const qint32 kPropsCount = STATIC_ARRAY_SIZE(kProps);

qint32 propIndex(const char* name)
{
	DCHECK_CSTRING(name);

	qint32 result = -1;
	for (qint32 i = 0; i < kPropsCount; ++i) {
		if ( qstrcmp(kProps[i].name, name) == 0) {
			result = i;
			break;
		}
	}

	DCHECK(result != -1);
	return result;
}

Site::Site(QObject* parent)
	: QObject(parent)
	, checkResult_(CheckResult::notChecked)
	, checkType_(CheckType::onePage)
	//: QObject(parent)
{}

Site* Site::createFromString(const QString& csvValuesString, QObject* parent)
{
	DCHECK_QSTRING(csvValuesString);

	using Bugs::makeExProp;

	bool ok = false;
	Site* site = new Site(parent);
	SCOPE_EXIT {
		if (!ok) 
			delete site;
	};

	QStringList values = csvValuesString.split(_S(";"));
	int valuesCount = values.count();
	if (valuesCount > kPropsCount) {
		THROW( ParseError(qApp->translate("Site", "Invalid csv values count")) 
			<< makeExProp("line", csvValuesString) 
			<< makeExProp("current values count", valuesCount)
			<< makeExProp("max values count", kPropsCount) 
			);
	}

	std::for_each(values.begin(), values.end(), [](QString& value) {
		value = value.trimmed();
	});

	int i = 0;
	QUrl url = values[i];
	if (!url.isValid()) {
		THROW( ParseError(qApp->translate("Site", "Url '%1' is invalid").arg(values[i])) 
			<< makeExProp("line", csvValuesString) 
			);
	}
	site->setUrl(url);

	++i;
	url = values[i];
	if (!url.isValid()) {
		THROW( ParseError(qApp->translate("Site", "Url '%1' is invalid").arg(values[i])) 
			<< makeExProp("line", csvValuesString) 
			);
	}
	site->setLinkedUrl(url);

	++i;
	url = values[i];
	if (!url.isValid()) {
		THROW( ParseError(qApp->translate("Site", "Url '%1' is invalid").arg(values[i]))
			<< makeExProp("line", csvValuesString) 
			);
	}
	site->setLinkedSite(url);

	++i;
	url = values[i];
	if (!url.isValid()) {
		THROW( ParseError(qApp->translate("Site", "Url '%1' is invalid").arg(values[i])) 
			<< makeExProp("line", csvValuesString)
			);
	}
	site->setUrlOnLinkedSite(url);

	++i;
	for (; i < values.count(); ++i) {
		const QString& val = values[i];

		CheckResult checkResult = CheckResult::undefined;
		if ( QString::compare(val, _S("sitewide"), Qt::CaseInsensitive) == 0 ) {
			site->setCheckType(CheckType::siteWide);
		}
		else if ( QString::compare(val, _S("onepage"), Qt::CaseInsensitive) == 0 ) {
			site->setCheckType(CheckType::onePage);
		}
		else if ((checkResult = stringToCheckResult(val)) != CheckResult::undefined) {
			site->setCheckResult(checkResult);
		}
		else {
			QDateTime lastChecked = QDateTime::fromString(val, _S("d,M,yyyy"));
			if (!lastChecked.isValid()) {
				lastChecked = QDateTime::fromString(val, _S("d,M,yyyy hh:mm:ss"));
			}
			if (!lastChecked.isValid()) {
				THROW( ParseError(qApp->translate("Site", "Date '%1' is invalid").arg(val)) 
					<< makeExProp("line", csvValuesString) 
					<< makeExProp("url", values[i])
					);
			}
			site->setLastCheckDate(lastChecked);
		}
	}
	
	ok = true;
	return site;
}

QString Site::toString() const
{
	QString result;
	QTextStream out(&result);

	out << url().toString();
	out << _C(';') << linkedUrl().toString();
	out << _C(';') << linkedSite().toString();
	out << _C(';') << urlOnLinkedSite().toString();

	CheckType checkType = this->checkType();
	switch (checkType) {
	case CheckType::onePage:
		out << _C(';') << _S("onepage");
		break;
	case CheckType::siteWide:
		out << _C(';') << _S("sitewide");
		break;
	default:
		DNOT_REACHABLE();
		break;
	}

	QDateTime lastChecked =  lastCheckDate();
	if (lastChecked.isValid())
		out << _C(';') << lastChecked.toString(_S("d,M,yyyy hh:mm:ss"));

	QString checkResultStr = lastCheckResultString();
	out << _C(';') << checkResultStr;

	if (!result.isEmpty()) {
		out << endl;
	}

	return result;
}

qint32 Site::propsCount()
{
	return kPropsCount;
}

QString Site::propName(qint32 index)
{
	DCHECK(0 <= index && index < propsCount());
	return QString::fromLatin1(kProps[index].name);
}

qint32 Site::propMinWidth(qint32 index)
{
	DCHECK(0 <= index && index < propsCount());
	return kProps[index].propMinWidth;
}

CheckResult Site::checkResult() const
{
	return checkResult_;
}

void Site::setCheckResult(CheckResult checkResult)
{
	checkResult_ = checkResult;
	emit propChanged(propIndex("Link Present"));
}

QString Site::checkResultToString(CheckResult checkResult)
{
	QString result;

	switch (checkResult) {
	case CheckResult::notChecked:
		result = _S("not checked");
		break;
	case CheckResult::inProgress:
		result = _S("checking...");
		break;
	case CheckResult::linkPresent:
		result = _S("true");
		break;
	case CheckResult::linkNotFound:
		result = _S("false");
		break;
	case CheckResult::searchFailed:
		result =_S("failed, check your internet connection");
		break;
	case CheckResult::undefined:
		result = _S("undefined");
		break;
	default:
		DNOT_REACHABLE();
	}

	DCHECK_QSTRING(result);
	return result;
}

CheckResult Site::stringToCheckResult(const QString& checkResultStr)
{
	DCHECK_QSTRING(checkResultStr);

	CheckResult checkResult = CheckResult::undefined;

	if (checkResultStr == _S("not checked")) {
		checkResult = CheckResult::notChecked;
	}
	else if (checkResultStr == _S("checking...")) {
		checkResult = CheckResult::inProgress;
	}
	else if (checkResultStr == _S("true")) {
		checkResult = CheckResult::linkPresent;
	}
	else if (checkResultStr == _S("false")) {
		checkResult = CheckResult::linkNotFound;
	}
	else if (checkResultStr == _S("failed, check your internet connection")) {
		checkResult = CheckResult::searchFailed;
	}
	else if (checkResultStr == _S("undefined")) {
		checkResult = CheckResult::undefined;
	}

	return checkResult;
}

QString Site::lastCheckResultString() const
{
	CheckResult lastCheck = checkResult();
	return checkResultToString(lastCheck);
}

const QDateTime& Site::lastCheckDate() const
{
	return lastChecked_;
}

void Site::setLastCheckDate(const QDateTime& date)
{
	DCHECK(date.isValid());

	lastChecked_ = date;
	emit propChanged(propIndex("Last checked"));
}

const QUrl& Site::url() const
{
	DCHECK(url_.isValid());
	return url_;
}

void Site::setUrl(const QUrl& url)
{
	DCHECK(url.isValid());
	url_ = url;
	emit propChanged(propIndex("My site"));
}

const QUrl& Site::linkedUrl() const
{
	DCHECK(linkedUrl_.isValid());
	return linkedUrl_;
}

void Site::setLinkedUrl(const QUrl& url)
{
	DCHECK(url.isValid());
	linkedUrl_ = url;
	emit propChanged(propIndex("His link on my site"));
}

const QUrl& Site::linkedSite() const
{
	DCHECK(linkedSite_.isValid());
	return linkedSite_;
}

void Site::setLinkedSite(const QUrl& url)
{
	DCHECK(url.isValid());
	linkedSite_ = url;
	emit propChanged(propIndex("His site"));
}

const QUrl& Site::urlOnLinkedSite() const
{
	DCHECK(urlOnLinkedSite_.isValid());
	return urlOnLinkedSite_;
}

void Site::setUrlOnLinkedSite(const QUrl& url)
{
	DCHECK(url.isValid());
	urlOnLinkedSite_ = url;
	emit propChanged(propIndex("My link on his site"));
}

CheckType Site::checkType() const
{
	return checkType_;
}

void Site::setCheckType(CheckType checkType)
{
	checkType_ = checkType;
	emit propChanged(propIndex("Search method"));
}

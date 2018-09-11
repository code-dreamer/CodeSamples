#include "stdafx.h"
#include "SitesDataModel.h"
#include "SiteContainer.h"
#include "Site.h"
#include "FileOpenError.h"
#include "SiteChecker.h"
#include "FileParseError.h"
#include "Bugs/ExceptionHandler.h"
#include "ParseError.h"
#include "Tools/QtHelpers.h"
#include "Tools/Filesystem.h"
#include "DownloadManager.h"

SitesDataModel::SitesDataModel(SiteContainer* siteContainer, SiteChecker* siteChecker, QObject* parent)
	: QAbstractTableModel(parent)
	, siteContainer_(siteContainer)
	, siteChecker_(siteChecker)
{
	DCHECK_PTR(siteContainer);
	DCHECK_PTR(siteChecker);

	Tools::QtHelpers::checkedConnect(siteContainer, &SiteContainer::sitePropChanged, this, &SitesDataModel::tableCellChanged);
	Tools::QtHelpers::checkedConnect(siteChecker, &SiteChecker::searchProgressChanged, this, &SitesDataModel::checkProgressChanged);
}

SitesDataModel::~SitesDataModel()
{}

int SitesDataModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
	DCHECK(parent == QModelIndex()); // model must be non-hierarchical

	return siteContainer_->count();
}

int SitesDataModel::columnCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
	DCHECK(parent == QModelIndex()); // model must be non-hierarchical

	return SiteContainer::sitePropsCount();
}

QVariant SitesDataModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
{
	auto checkParams = [&]() -> bool
	{
		bool ok = index.isValid();
		if (ok) {
			int row = index.row();
			int column = index.column();

			ok = (0 <= row && row < rowCount())
				&& (0 <= column && column < columnCount());
		}

		return ok;
	};
	DCHECK(checkParams());

	QVariant dataValue;
	
	if (role == Qt::DisplayRole) {
		dataValue = dataImpl(index.row(), index.column());
	}
	else if (role == Qt::BackgroundRole) {
		QColor bkgColor = backgroundColor(index.row());
		if (bkgColor.isValid()) {
			dataValue = bkgColor;
		}
	}

	return dataValue;
}

QVariant SitesDataModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
{
	QVariant result;

	if (role == Qt::DisplayRole) {
		if (orientation == Qt::Vertical) {
			result = QVariant(section + 1);
		}
		else {
			result = columnName(section);
		}
	}
	else if (role == Qt::SizeHintRole && orientation == Qt::Horizontal) {
		result = QSize(SiteContainer::sitePropMinWidth(section), 23);
	}

	return result;
}

Qt::ItemFlags SitesDataModel::flags(const QModelIndex& index) const
{
	return QAbstractTableModel::flags(index);
}

QVariant SitesDataModel::dataImpl(int row, int column) const
{
	const Site* site = siteContainer_->site(row);

	QVariant dataValue;

	switch (column)	{
	case 0:
		dataValue = site->url();// info.mySite;
		break;
	case 1:
		dataValue = site->linkedUrl();// info.hisLinkOnMySite;
		break;
	case 2:
		dataValue = site->linkedSite();// info.hisSite;
		break;
	case 3:
		dataValue = site->urlOnLinkedSite();// info.myLinkOnHisSite;
		break;
	case 4:
		dataValue = site->checkType() == CheckType::onePage ? _S("one page") : _S("sitewide");
		break;
	case 5: 
		dataValue = site->lastCheckDate();
		break;
	case 6:
		dataValue = site->lastCheckResultString();
		break;
	default:
		DNOT_REACHABLE();
	}

	return dataValue;
}

void SitesDataModel::tableCellChanged(qint32 row, qint32 column)
{
	DCHECK(0 <= row && row < rowCount());
	DCHECK(0 <= column && column < columnCount());

	emit dataChanged(index(row, 0), index(row, column));
}

QString SitesDataModel::columnName(int column) const
{
	return SiteContainer::sitePropName(column);
}

QColor SitesDataModel::backgroundColor(int row) const
{
	DCHECK(0 <= row && row < siteContainer_->count());

	const CheckResult lastCheck = siteContainer_->site(row)->checkResult();

	QColor color;

	switch (lastCheck) {
	case CheckResult::notChecked:
	case CheckResult::inProgress:
		break;
	case CheckResult::linkPresent:
		color = QColor(225,255,225);
		break;
	case CheckResult::linkNotFound:
		color = QColor(255,225,225);
		break;
	case CheckResult::searchFailed:
		color = QColor(255,255,225);
		break;
	case CheckResult::undefined:
	default:
		DNOT_REACHABLE();
	}

	return color;
}

void SitesDataModel::loadFromCSV(const QString& path)
{
	DCHECK( Tools::Filesystem::checkExistedPath(path) );

	QFile file(path);
	if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) ) 
		THROW(FileOpenError(path, file));

	QAbstractTableModel::beginResetModel();
	SCOPE_EXIT {
		QAbstractTableModel::endResetModel();
	};

	siteChecker_->stopChecking();
	siteContainer_->clear();
	int lineNumber = 0;
	while ( !file.atEnd() )	{
		QString line = QString::fromLatin1(file.readLine().constData());
		if ( !line.isEmpty() ) {
			try {
				siteContainer_->addFromString(line);
			}
			catch (ParseError& e) {
				using namespace Bugs;
				e << makeExProp("lineNumber", lineNumber);
				e << makeExProp("file path", path);
				ExceptionHandler::reportError(e);
				QMessageBox::StandardButton answer = QMessageBox::question(QApplication::activeWindow(), tr("File processing"), tr("Stop file processing?"), 
					QMessageBox::Yes | QMessageBox::No);
				if (answer == QMessageBox::Yes) {
					break;
				}
			}
		}

		++lineNumber;
	}

	openedFilepath_ = path;
}

void SitesDataModel::saveToOpenedCSV()
{
	saveToCSV(openedFilepath_);
}

void SitesDataModel::saveToCSV(const QString& path)
{
	DCHECK( Tools::Filesystem::checkPath(path, true) );

	QFile file(path);
	if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) ) 
		THROW(FileOpenError(path, file));

	QTextStream out(&file);

	const qint32 count = siteContainer_->count();
	for (qint32 i = 0; i < count; ++i) {
		QString siteStr = siteContainer_->site(i)->toString();
		out << siteStr;
	}
}

bool SitesDataModel::isLoaded() const
{
	return (siteContainer_->count() != 0);
}

void SitesDataModel::checkSites(const QModelIndexList& indexes)
{
	DCHECK(!indexes.isEmpty());

	emit checkProgressChanged(0);

	foreach(const QModelIndex& index, indexes) {
		const int row = index.row();
		DCHECK(0 <= row && row < rowCount());
		Site* site = siteContainer_->site(row);
		siteChecker_->addSite(site);
	}

	siteChecker_->startChecking();
}

void SitesDataModel::checkAllSites()
{
	QModelIndexList indexes;
	for (int i = 0, rowCount = this->rowCount(); i < rowCount; ++i ) {
		indexes.push_back(QAbstractTableModel::createIndex(i, 0));
	}

	checkSites(indexes);
}

void SitesDataModel::stopChecking()
{
	siteChecker_->stopChecking();
}
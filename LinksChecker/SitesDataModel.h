#pragma once

class SiteContainer;
class SiteChecker;

class SitesDataModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	SitesDataModel(SiteContainer* siteContainer, SiteChecker* siteChecker, QObject* parent = nullptr);
	virtual ~SitesDataModel();

signals:
	void checkProgressChanged(qint32 value);

// Qt model implementation
public:
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
////////////////////////////////////////////////////////////////////////

public:
	void loadFromCSV(const QString& path);
	void saveToOpenedCSV();
	void saveToCSV(const QString& path);
	bool isLoaded() const;
	void checkSelectedSites();
	void checkSites(const QModelIndexList& indexes);
	void checkAllSites();
	void stopChecking();

private slots:
	void tableCellChanged(qint32 row, qint32 column);

private:
	QColor backgroundColor(int row) const;
	QVariant dataImpl(int row, int column) const;
	QString columnName(int column) const;

	void checkSite(int row);

private:
	SiteContainer* siteContainer_;
	SiteChecker* siteChecker_;
	QString openedFilepath_;
};

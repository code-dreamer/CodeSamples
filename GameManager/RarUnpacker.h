#pragma once

class KERNEL_API RarUnpacker : public QObject
{
	Q_OBJECT

	friend static int CALLBACK RarCallBackProc(UINT msg, LONG userData, LONG p1, LONG p2);

public:
	struct ArchiveInfo
	{
		bool isCorrectArchive;
		bool isVolumeArchive;
		bool isFirstVolume;
	};

public:
	RarUnpacker(QObject* parent = nullptr);
	virtual ~RarUnpacker();

Q_SIGNALS:
	void BytesProcessed(qint64 bytes) const;

public:
	static qint64 GetArchiveSize(const QString& archiveFilePath);
	static qint64 GetUnpackedArchiveSize(const QString& archiveFilePath);
	static ArchiveInfo ReadArchiveInfo(const QString& archiveFilePath);
	
	void Unpack(const QString& archiveFilePath, const QString& unpackDirPath, bool deleteArchiveFiles = false, const QString& password = EMPTY_STR);

private:
	void SetPackedBytesProcessed(int bytes);

private:
	QString password_;
	qint64 bytesProcessedStep_;
	qint64 bytesProcessed_;
	qint64 lastEmittedBytesProcessed_;

#pragma warning(push)
#pragma warning(disable : 4251)	
	QStringList volumePathes_;
#pragma warning(pop)
};


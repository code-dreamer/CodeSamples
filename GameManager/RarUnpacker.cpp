#include "StdAfx.h"

#include <dll.hpp>

#include "RarUnpacker.h"
#include "RarUnpackerException.h"
#include "FileSystemTools.h"
#include "StringTools.h"

using namespace KernelExceptions;

namespace
{

qint64 GetArchiveSizeImpl(const QString& archiveFilePath, bool packedSize)
{
	if( !FileSystemTools::IsPathExist(archiveFilePath) ) {
		throw RarUnpackerException(QCoreApplication::translate("RarUnpacker", "Can't find '%1' archive").arg(archiveFilePath));
	}

	const std::wstring archivePathStd = StringTools::CreateStdWString(archiveFilePath);
	char rarBuf[16*1024];
	struct RAROpenArchiveDataEx openArchiveData;
	::ZeroMemory(&openArchiveData, sizeof(openArchiveData));
	memset(&openArchiveData.Reserved, 0, sizeof(openArchiveData.Reserved));
	openArchiveData.ArcNameW = const_cast<wchar_t*>(archivePathStd.c_str());
	openArchiveData.CmtBuf  = rarBuf;
	openArchiveData.CmtBufSize = sizeof(rarBuf);
	openArchiveData.OpenMode = RAR_OM_LIST_INCSPLIT;
	openArchiveData.Callback = nullptr;

	HANDLE arcDataHandleRaw = RAROpenArchiveEx(&openArchiveData);
	if (openArchiveData.OpenResult != 0) {
		throw RarUnpackerException(QCoreApplication::translate("RarUnpacker", "Can't open archive '%1'").arg(archiveFilePath), openArchiveData.OpenResult);
	}
	std::unique_ptr<void, decltype(std::ptr_fun(&RARCloseArchive))> archHandle(arcDataHandleRaw, std::ptr_fun(&::RARCloseArchive));

	struct RARHeaderData headerData;
	::ZeroMemory(&headerData, sizeof(headerData));
	headerData.CmtBuf = nullptr;
	int readHeaderResult = -1;
	int processFileResult = -1;
	qint64 totalSize = 0;
	while( (readHeaderResult = RARReadHeader(arcDataHandleRaw, &headerData)) == 0 ) {
		processFileResult = RARProcessFileW(arcDataHandleRaw, RAR_SKIP, nullptr, nullptr);
		if(processFileResult != 0) {
			throw RarUnpackerException(QCoreApplication::translate("RarUnpacker", "Can't read archive '%1'").arg(archiveFilePath), processFileResult);
		}

		totalSize += packedSize ? headerData.PackSize : headerData.UnpSize;
	}

	if (readHeaderResult != ERAR_END_ARCHIVE) {
		throw RarUnpackerException(QCoreApplication::translate("RarUnpacker", "Can't read archive '%1'").arg(archiveFilePath), processFileResult);
	}

	return totalSize;
}

} // namespace

static int CALLBACK RarCallBackProc(UINT msg, LONG userData, LONG p1, LONG p2)
{
	if (msg == UCM_NEEDPASSWORD) {
		RarUnpacker* unpacker = (RarUnpacker*)userData;
		lstrcpynA((char *)p1, (char *)unpacker->password_.toLatin1().constData(), p2);
	}
	else if (msg == UCM_CHANGEVOLUMEW) {
		if (p2 == RAR_VOL_NOTIFY) {
			const wchar_t* nextVolumeName = (const wchar_t*)p1;
			RarUnpacker* unpacker = (RarUnpacker*)userData;
			unpacker->volumePathes_.push_back( StringTools::CreateQString(nextVolumeName) );
		}
	}
	else if (msg == UCM_PROCESSDATA) {
		RarUnpacker* unpacker = (RarUnpacker*)userData;
		unpacker->SetPackedBytesProcessed((int)p2);
	}

	return 0;
}

RarUnpacker::RarUnpacker(QObject* const parent)
	: QObject(parent)
	, bytesProcessedStep_(0)
	, bytesProcessed_(0)
	, lastEmittedBytesProcessed_(0)
{
}

RarUnpacker::~RarUnpacker()
{
}

qint64 RarUnpacker::GetArchiveSize(const QString& archiveFilePath)
{
	return GetArchiveSizeImpl(archiveFilePath, true);
}

qint64 RarUnpacker::GetUnpackedArchiveSize(const QString& archiveFilePath)
{
	return GetArchiveSizeImpl(archiveFilePath, false);
}

void RarUnpacker::Unpack(const QString& archiveFilePath, const QString& unpackDirPath, bool deleteArchiveFiles, const QString& password)
{
	QSTRING_NOT_EMPTY(archiveFilePath);
	QSTRING_NOT_EMPTY(unpackDirPath);

	if ( !FileSystemTools::IsPathExist(archiveFilePath) ) {
		throw RarUnpackerException(tr("Archive file '%1' doesn't exist.").arg(archiveFilePath));
	}

	FileSystemTools::CreateDirIfNeeded(unpackDirPath);

	password_ = password;

	const std::wstring archivePathStd = StringTools::CreateStdWString(archiveFilePath);
	const std::wstring unpackDirPathStd = StringTools::CreateStdWString(unpackDirPath);

	qint64 archiveSize = GetArchiveSize(archiveFilePath);
	bytesProcessedStep_ = qRound64((double)archiveSize/100.0);

	char rarBuf[16*1024];
	struct RAROpenArchiveDataEx openArchiveData;
	::ZeroMemory(&openArchiveData, sizeof(openArchiveData));
	memset(&openArchiveData.Reserved, 0, sizeof(openArchiveData.Reserved));
	openArchiveData.ArcNameW = const_cast<wchar_t*>(archivePathStd.c_str());
	openArchiveData.CmtBuf  = rarBuf;
	openArchiveData.CmtBufSize = sizeof(rarBuf);
	openArchiveData.OpenMode = RAR_OM_EXTRACT;
	openArchiveData.Callback = nullptr;

	HANDLE arcDataHandleRaw = RAROpenArchiveEx(&openArchiveData);
	if (openArchiveData.OpenResult != 0) {
		throw RarUnpackerException(tr("Can't open archive '%1'").arg(archiveFilePath), openArchiveData.OpenResult);
	}

	std::unique_ptr<void, decltype(std::ptr_fun(&RARCloseArchive))> archHandle(arcDataHandleRaw, std::ptr_fun(&::RARCloseArchive));

	RARSetCallback(arcDataHandleRaw, RarCallBackProc, (LONG)this);

	struct RARHeaderData headerData;
	::ZeroMemory(&headerData, sizeof(headerData));
	headerData.CmtBuf = nullptr;
	int readHeaderResult = -1;
	int processFileResult = -1;
	bytesProcessed_ = 0;

	volumePathes_.push_back(archiveFilePath);

	while( (readHeaderResult = RARReadHeader(arcDataHandleRaw, &headerData)) == 0 ) {
		processFileResult = RARProcessFileW(arcDataHandleRaw, RAR_EXTRACT, const_cast<wchar_t*>(unpackDirPathStd.c_str()), nullptr);
		if(processFileResult != 0) {
			throw RarUnpackerException(tr("Unpacking failed.\nArchive file: '%1'\nExtract directory:'%2'").arg(archiveFilePath).arg(unpackDirPath), 
				processFileResult);
		}
	}

	if (readHeaderResult != ERAR_END_ARCHIVE) {
		throw RarUnpackerException(tr("Archive '%1' is corrupted").arg(archiveFilePath), processFileResult);
	}

	// remove all archive files after unpacking
	if (deleteArchiveFiles) {
		archHandle.reset();
		FileSystemTools::RemoveFiles(volumePathes_);
	}
}

RarUnpacker::ArchiveInfo RarUnpacker::ReadArchiveInfo(const QString& archiveFilePath)
{
	QSTRING_NOT_EMPTY(archiveFilePath);
	if ( !FileSystemTools::IsPathExist(archiveFilePath) ) {
		throw RarUnpackerException(tr("Archive file '%1' doesn't exist.").arg(archiveFilePath));
	}

	const std::wstring archivePathStd = StringTools::CreateStdWString(archiveFilePath);
	char rarBuf[16*1024];
	struct RAROpenArchiveDataEx openArchiveData;
	::ZeroMemory(&openArchiveData, sizeof(openArchiveData));
	memset(&openArchiveData.Reserved, 0, sizeof(openArchiveData.Reserved));
	openArchiveData.ArcNameW = const_cast<wchar_t*>(archivePathStd.c_str());
	openArchiveData.CmtBuf  = rarBuf;
	openArchiveData.CmtBufSize = sizeof(rarBuf);
	openArchiveData.OpenMode = RAR_OM_LIST;
	openArchiveData.Callback = nullptr;
	openArchiveData.Flags = 0;
	
	std::unique_ptr<void, decltype(std::ptr_fun(&RARCloseArchive))> archHandle(RAROpenArchiveEx(&openArchiveData), std::ptr_fun(&::RARCloseArchive));

	ArchiveInfo archInfo;
	::ZeroMemory(&archInfo, sizeof(archInfo));
	archInfo.isCorrectArchive = (openArchiveData.OpenResult == 0);
	if (archInfo.isCorrectArchive) {
		// defines from headers.hpp in unrar library
#define  MHD_VOLUME         0x0001U
#define  MHD_FIRSTVOLUME    0x0100U

		archInfo.isVolumeArchive = (openArchiveData.Flags & MHD_VOLUME);
		if (archInfo.isVolumeArchive) {
			archInfo.isFirstVolume = (openArchiveData.Flags & MHD_FIRSTVOLUME) != 0;
		}

#undef MHD_VOLUME
#undef MHD_FIRSTVOLUME
	}

	return archInfo;
}

void RarUnpacker::SetPackedBytesProcessed(int bytes)
{
	if (bytes > 0) {
		bytesProcessed_ += bytes;

		if (bytesProcessed_-lastEmittedBytesProcessed_ >= bytesProcessedStep_) {
			lastEmittedBytesProcessed_ = bytesProcessed_;
			Q_EMIT BytesProcessed(bytesProcessed_);
		}
	}
}

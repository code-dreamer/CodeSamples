#include "stdafx.h"

#include "ImManager.h"
#include "ImManagerSettings.h"
#include "ConfigPlaces.h"
#include "MainModuleKeeper.h"
#include "ServerManagerInterface.h"
#include "UserAuthData.h"
#include "MainModuleSettingsInterface.h"
#include "XmppClient.h"
#include "CppTools.h"
#include "ContactList.h"
#include "ContactItem.h"
#include "XmppTools.h"
#include "LogWriter.h"
#include "FileSystemTools.h"
#include "Cryptor.h"

const QString s_translationsDir = STR(":/ImManagement/Translations");
const ContactItemInterface::ContactItemPresence s_invalidPendingPrecense = ContactItemInterface::Invalid;

const QString s_serviceUserName = STR("AwLvoJY8JOd5PlWY4U1Dlw0AbL32QkaaE1QwtvNFVQ=="); //service_user#dymmymail.com
const QString s_serviceUserPassword = STR("AwIhfLFeZeJMUGj473c7yxQOP9bEJR7tRlhogZAPMMUZB0ynuA=="); //6F6F2081A1A749A48FBC08A7FBD40CC9

namespace
{

static QList< QPair<QString, QString> > s_presenceInfo;

void FillPresenceInfo()
{
	G_ASSERT(s_presenceInfo.empty());
	s_presenceInfo.clear();

	s_presenceInfo.push_back(qMakePair(QCoreApplication::translate("ImManager", "Online"),
		QCoreApplication::translate("ImManager", "I'm online")));
	s_presenceInfo.push_back(qMakePair(QCoreApplication::translate("ImManager", "Ready to chat"),
		QCoreApplication::translate("ImManager", "I'm ready to chat!")));
	s_presenceInfo.push_back(qMakePair(QCoreApplication::translate("ImManager", "Away"),
		QCoreApplication::translate("ImManager", "I'm away")));
	s_presenceInfo.push_back(qMakePair(QCoreApplication::translate("ImManager", "Do not disturb"),
		QCoreApplication::translate("ImManager", "Please, don't disturb me")));
	s_presenceInfo.push_back(qMakePair(QCoreApplication::translate("ImManager", "Not available"),
		QCoreApplication::translate("ImManager", "I'm unavailable")));
	s_presenceInfo.push_back(qMakePair(QCoreApplication::translate("ImManager", "Offline"),
		QCoreApplication::translate("ImManager", "I'm is offline")));
	s_presenceInfo.push_back(qMakePair(QString(STR("Probe")), EMPTY_STR));
	s_presenceInfo.push_back(qMakePair(QString(STR("Error")), EMPTY_STR));
	s_presenceInfo.push_back(qMakePair(QString(STR("Invalid")), EMPTY_STR));
}

}

ImManager::ImManager(QObject* const parent)
	: QObject(parent)
	, settings_(nullptr)
	, xmppClient_(nullptr)
	, pendingPresence_(ContactItemInterface::Invalid)
	, reconnect_(false)
	, logWriter_(nullptr)
	, connectingState_(ServiceUserConnect)
{
}

ImManager::~ImManager()
{
}

void ImManager::Init()
{
	settings_ = new ImManagerSettings(this);
	LoadTranslation();

	FillPresenceInfo();

	xmppClient_ = new XmppClient(this, this);
	if (!xmppClient_->isRunning()) {
		xmppClient_->AsyncStart();
	}

	InitLog();
	CHECKED_CONNECT(xmppClient_, SIGNAL(LogMessageReady(QString)), this, SLOT(OnLogMessageReady(QString)));

	CHECKED_CONNECT(xmppClient_, SIGNAL(Connected()), this, SIGNAL(Connected()));
	CHECKED_CONNECT(xmppClient_, SIGNAL(Disconnected(bool, QString)), this, SLOT(OnDisconnected(bool, QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(AuthenticationCompleted()), this, SLOT(OnAuthenticationCompleted()));
	CHECKED_CONNECT(xmppClient_, SIGNAL(RegistrationCompleted(QString)), this, SLOT(OnRegistrationCompleted(QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(SelfPresenceReceived(ContactItemInterface::ContactItemPresence, QString)), this, SLOT(OnSelfPresenceReceived(ContactItemInterface::ContactItemPresence, QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(ContactListReceived()), this, SIGNAL(ContactListReceived()));
	CHECKED_CONNECT(xmppClient_, SIGNAL(PresenceReceived(QString, ContactItemInterface::ContactItemPresence, QString)), this, SIGNAL(PresenceReceived(QString, ContactItemInterface::ContactItemPresence, QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(MessageReceived(QString, QString)), this, SIGNAL(MessageReceived(QString, QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(AuthorizationRequestReceived(QString, QString)), this, SIGNAL(AuthorizationRequestReceived(QString, QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(UserProfileReceived(QString)), this, SIGNAL(UserProfileReceived(QString)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(SelfProfileReceived()), this, SIGNAL(SelfProfileReceived()));
	CHECKED_CONNECT(xmppClient_, SIGNAL(ContactItemAdded(ContactItemInterface*)), this, SIGNAL(ContactItemAdded(ContactItemInterface*)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(ContactItemUpdated(ContactItemInterface*)), this, SIGNAL(ContactItemUpdated(ContactItemInterface*)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(ContactItemRemoved(QString, bool)), this, SIGNAL(ContactItemRemoved(QString, bool)));
	CHECKED_CONNECT(xmppClient_, SIGNAL(SearchContactsReceived(QStringList)), this, SIGNAL(SearchContactsReceived(QStringList)));
}

void ImManager::EstablishConnection()
{
	G_ASSERT( !IsConnected() );

	EstablishConnectionImpl();
}

void ImManager::EstablishConnectionImpl()
{
	CHECK_PTR(xmppClient_);

	if (connectingState_ == UserRegistered) {
		const UserAuthData& userAuthData = MainModuleKeeper::GetMainModule()->GetServerManager()->GetUserAuthData();

		xmppClient_->SetUserName( XmppTools::G2IDToXmppID(userAuthData.GetLogin()) );
		xmppClient_->SetUserPassword( userAuthData.GetPassword() );
	}
	else {
		Cryptor* cryptor = MainModuleKeeper::GetMainModule()->GetCryptor();

		QString serviceUsername = cryptor->Decrypt(s_serviceUserName);
		QSTRING_NOT_EMPTY(serviceUsername);
		QString serviceUserPassword = cryptor->Decrypt(s_serviceUserPassword);
		QSTRING_NOT_EMPTY(serviceUserPassword);

		xmppClient_->SetUserName(serviceUsername);
		xmppClient_->SetUserPassword(serviceUserPassword);

		G_ASSERT( !xmppClient_->IsConnected() );
		connectingState_ = ServiceUserConnect;
	}

	xmppClient_->ConnectToServer();
}

void ImManager::OnRegistrationCompleted(const QString error)
{
	if (error.isEmpty()) {
		connectingState_ = UserRegistered;
		Reconnect();
	}
	else  {
		Q_EMIT SplashTextReady(tr("Can't register user. The reason: '%1'.\nPlease try to relogin").arg(error));
	}
}

void ImManager::OnAuthenticationCompleted()
{
	if (connectingState_ == ServiceUserConnect) {
		const UserAuthData& userAuthData = MainModuleKeeper::GetMainModule()->GetServerManager()->GetUserAuthData();
		connectingState_ = UserRegistered;
		xmppClient_->SetUserName( XmppTools::G2IDToXmppID(userAuthData.GetLogin()) );
		xmppClient_->SetUserPassword( userAuthData.GetPassword() );
		xmppClient_->RegisterUser();
		
		return;
	}

	xmppClient_->RequestSelfProfile();
	
	Q_EMIT AuthenticationCompleted();

	if (pendingPresence_ != s_invalidPendingPrecense) {
		xmppClient_->SetPresence(pendingPresence_, pendingPresenceMessage_);
	}
}

bool ImManager::IsConnected() const
{
	return xmppClient_->IsConnected();
}

void ImManager::CloseConnection()
{
	G_ASSERT( IsConnected() );

	if (xmppClient_->IsConnected()) {
		xmppClient_->DisconnectFromServer();
	}
}

void ImManager::SetPresence(ContactItemInterface::ContactItemPresence presence, const QString& presenceMessage)
{
	static_assert(ContactItemInterface::ContactItemPresenceCount == 9, "Unsupported items count in enum ContactItemInterface::ContactItemPresence");
	if (pendingPresence_ == presence) {
		return;
	};

	if ( !IsConnected() ) {
		if (ContactItemInterface::Available <= presence && presence <= ContactItemInterface::XA) {
			G_ASSERT(pendingPresence_ == s_invalidPendingPrecense);

			pendingPresence_ = presence;
			pendingPresenceMessage_ = presenceMessage;

			EstablishConnection();
		}
	}
	else {
		xmppClient_->SetPresence(presence, presenceMessage);
	}
}

void ImManager::Shutdown()
{
	xmppClient_->SyncStop();
}

void ImManager::InitUserSession()
{
	const QString userId = MainModuleKeeper::GetMainModule()->GetServerManager()->GetUserAuthData().GetCurrentUserId();
	ImManagerSettings::GetSettings()->GetConfigPlaces()->AddUserPaths(userId);
}

void ImManager::ShutdownUserSession()
{
	if (xmppClient_->IsConnected()) {
		CloseConnection();
	}
	ImManagerSettings::GetSettings()->GetConfigPlaces()->RemoveCurrentUserPaths();
}

inline ImManagerSettingsInterface* ImManager::GetSettings() const
{
	CHECK_PTR(settings_);
	return settings_;
}

inline QObject* ImManager::ToQObject()
{
	return this;
}

inline const QObject* ImManager::ToQObject() const
{
	return this;
}

void ImManager::LoadTranslation()
{
	const QString localeName = MainModuleKeeper::GetMainModule()->GetSettings()->GetCurrentLocale().name();

	if (!moduleTranslator_.isEmpty()) {
		QApplication::removeTranslator(&moduleTranslator_);
	}

	QString translationFilename = QString(STR("%1%2%3")).arg(ImManagerSettings::GetModuleName(), STR("_"), localeName);
	const bool loaded = moduleTranslator_.load(translationFilename, s_translationsDir);
	G_ASSERT(loaded);

	QApplication::installTranslator(&moduleTranslator_);
}

void ImManager::Reconnect()
{
	G_ASSERT(!reconnect_);
	G_ASSERT( IsConnected() );

	reconnect_ = true;
	CloseConnection();
}

void ImManager::OnDisconnected(bool anotherInstanceConnected, QString reason)
{
	if (!reconnect_) {
		pendingPresence_ = s_invalidPendingPrecense;
		pendingPresenceMessage_ = EMPTY_STR;
		Q_EMIT Disconnected(anotherInstanceConnected, reason);
	}

	if (reconnect_) {
		reconnect_ = false;
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		QMetaObject::invokeMethod(this, "EstablishConnectionImpl", Qt::QueuedConnection);
	}
}

inline const ContactListInterface* ImManager::GetContactList() const
{  
	CHECK_PTR(xmppClient_);
	return xmppClient_->GetContactList();
}

void ImManager::SendMessage(const QString& contactId, const QString& message)
{
	QSTRING_NOT_EMPTY(contactId);
	QSTRING_NOT_EMPTY(message);
	CHECK_PTR(xmppClient_);
	
	xmppClient_->SendMessage(contactId, message);
}

void ImManager::SendAuthorizationRequest(const QString& contactId, const QString& authMessage)
{
	QSTRING_NOT_EMPTY(contactId);
	G_ASSERT( XmppTools::IsXmppID(contactId) );

	xmppClient_->AddSubscribtionToRoaster(contactId, authMessage);
}

void ImManager::RemoveContactFromList(const QString& contactId)
{
	QSTRING_NOT_EMPTY(contactId);
	G_ASSERT( XmppTools::IsXmppID(contactId) );

	xmppClient_->RemoveContactFromList(contactId);
}

void ImManager::UnsubscribeContact(const QString& contactId)
{
	QSTRING_NOT_EMPTY(contactId);
	G_ASSERT( XmppTools::IsXmppID(contactId) );

	xmppClient_->UnsibscribeContact(contactId);
}

void ImManager::AllowContactAuthorization(const QString& contactItemId, const bool allow) const
{
	xmppClient_->AllowContactAuthorization(contactItemId, allow);
}

ContactItemInterface* ImManager::GetSelfContact()
{
	CHECK_PTR(xmppClient_);
	return xmppClient_->GetSelfContact();
}

void ImManager::SaveContactOnServer(const QString& contactId)
{
	QSTRING_NOT_EMPTY(contactId);

	xmppClient_->SaveContactOnServer(contactId);
}

void ImManager::SearchContactsOnServer(const QString& searchString)
{
	QSTRING_NOT_EMPTY(searchString);

	QString localSearchString = searchString;
	localSearchString.replace(CHAR('@'), CHAR('#'));

	xmppClient_->SearchContactsOnServer(localSearchString);
}

void ImManager::OnSelfPresenceReceived(ContactItemInterface::ContactItemPresence precense, QString precenseMessage)
{
	pendingPresence_ = s_invalidPendingPrecense;
	pendingPresenceMessage_ = EMPTY_STR;

	Q_EMIT SelfPresenceReceived(precense, precenseMessage);

	if (precense == ContactItemInterface::Unavailable) {
		CloseConnection();
	}
}

QString ImManager::PrecenseToString(ContactItemInterface::ContactItemPresence precense) const
{
	static_assert(ContactItemInterface::ContactItemPresenceCount == 9, "Unsupported items count in enum ContactItemInterface::ContactItemPresence");
	G_ASSERT(ContactItemInterface::Available <= precense && precense < ContactItemInterface::ContactItemPresenceCount);

	return s_presenceInfo[precense].first;
}

QString ImManager::PrecenseToDefaultStatus(ContactItemInterface::ContactItemPresence precense) const
{
	static_assert(ContactItemInterface::ContactItemPresenceCount == 9, "Unsupported items count in enum ContactItemInterface::ContactItemPresence");
	G_ASSERT(ContactItemInterface::Available <= precense && precense < ContactItemInterface::ContactItemPresenceCount);

	return s_presenceInfo[precense].second;
}

void ImManager::InitLog()
{
	const QString& moduleName = ImManagerSettings::GetSettings()->GetModuleName();
	const QString& logDir = ImManagerSettings::GetSettings()->GetConfigPlaces()->GetLogDir();
	const QString logFilePath = FileSystemTools::MergePath(logDir, moduleName + STR("Log.txt"));

	logWriter_ = new LogWriter(moduleName, logFilePath, this);
}

void ImManager::OnLogMessageReady(QString message)
{
	QSTRING_NOT_EMPTY(message);
	CHECK_PTR(logWriter_);

	logWriter_->WriteEntry(message);
}

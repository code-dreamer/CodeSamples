#pragma once

#include "ImManagerInterface.h"

class ImManagerSettings;
class XmppClient;
class LogWriter;

class IM_MANAGEMENT_API ImManager : public QObject, public ImManagerInterface
{
	Q_OBJECT

public:
	explicit ImManager(QObject* parent = nullptr);
	virtual ~ImManager();

// ImManagerInterface implementation
public:
	virtual void EstablishConnection() override;
	virtual bool IsConnected() const override;
	virtual void CloseConnection() override;

	virtual void SetPresence(ContactItemInterface::ContactItemPresence presence, const QString& presenceMessage) override;
	virtual void SendMessage(const QString& contactId, const QString& message) override;
	virtual void SendAuthorizationRequest(const QString& contactId, const QString& authMessage) override;
	virtual void UnsubscribeContact(const QString& contactId) override;
	virtual void RemoveContactFromList(const QString& contactG2Id) override;
	virtual void AllowContactAuthorization(const QString& contactItemId, bool allow) const override;

	virtual const ContactListInterface* GetContactList() const override;
	virtual ContactItemInterface* GetSelfContact() override;
	virtual void SaveContactOnServer(const QString& contactId) override;

	virtual QString PrecenseToString(ContactItemInterface::ContactItemPresence precense) const override;
	virtual QString PrecenseToDefaultStatus(ContactItemInterface::ContactItemPresence precense) const override;

	virtual void SearchContactsOnServer(const QString& searchString) override;

Q_SIGNALS:
	void StatusTextReady(QString text) const override;
	void SplashTextReady(QString text) const override;

	void Connected() const override; // physically connected
	void AuthenticationCompleted() const override;
	void Disconnected(bool anotherInstanceConnected, QString error) const override;
	void SelfPresenceReceived(ContactItemInterface::ContactItemPresence presence, QString statusMessage) const override;
	void ContactListReceived() const override;
	void PresenceReceived(QString itemId, ContactItemInterface::ContactItemPresence presence, QString statusMessage) const override;
	void MessageReceived(QString itemId, QString text) const override;
	void AuthorizationRequestReceived(QString contactItemId, QString contactItemMessage) const override;
	void UserProfileReceived(QString itemId) const override;
	void SelfProfileReceived() const override;
	void ContactItemAdded(ContactItemInterface* item) const override;
	void ContactItemUpdated(ContactItemInterface* item) const override;
	void ContactItemRemoved(QString itemId, bool external) const override;

	void SearchContactsReceived(QStringList contactNames) const override;
//////////////////////////////////////////////////////////////////////////

// ModuleInterface implementation
public:
	virtual void Init() override;
	virtual void InitUserSession() override;
	virtual void ShutdownUserSession() override;

	virtual ImManagerSettingsInterface* GetSettings() const override;

public Q_SLOTS:
	virtual void Shutdown() override;
//////////////////////////////////////////////////////////////////////////

// ToQObjectConvertableInterface implementation
public:
	virtual QObject* ToQObject() override;
	virtual const QObject* ToQObject() const override;
//////////////////////////////////////////////////////////////////////////

private Q_SLOTS:
	void OnDisconnected(bool anotherInstanceConnected, QString error);
	void OnRegistrationCompleted(QString error);
	void OnAuthenticationCompleted();
	void OnSelfPresenceReceived(ContactItemInterface::ContactItemPresence precense, QString precenseMessage);
	void OnLogMessageReady(QString message);

	void EstablishConnectionImpl();

private:
	void LoadTranslation();
	void Reconnect();
	void InitLog();

private:
	enum ConnectingState
	{
		  ServiceUserConnect
		, UserRegistered
	} connectingState_;

private:
	ImManagerSettings* settings_;
	QTranslator moduleTranslator_;
	XmppClient* xmppClient_;

	ContactItemInterface::ContactItemPresence pendingPresence_;
	QString pendingPresenceMessage_;

	bool reconnect_;

	LogWriter* logWriter_;
};

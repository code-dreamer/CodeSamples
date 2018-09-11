#pragma once

#include "ContactItemInterface.h"
#include "ContactList.h"

class ContactItem : public QObject, public ContactItemInterface
{
	Q_OBJECT

public:
	ContactItem(QObject* parent = nullptr);
	virtual ~ContactItem();

public:
	static bool IsValidItemId(const QString& id);

// ContactItemInterface implementation
public:
	virtual const QString& GetId() const override;
	virtual ContactItemInterface::ContactItemPresence GetPresence() const override;
	virtual const QString& GetStatusMessage() const override;
	virtual QString GetFullName() const override;
	virtual const QString& GetName() const override;
	virtual const QPixmap& GetAvatar() override;
	virtual void SetAvatar(const QPixmap& avatar) override;
	
	virtual void SetStatusMessage(const QString& statusMessage) override;
	virtual void ReturnPreviousStatusMessage() override;

	virtual bool IsBelongToGroup(const QString& groupName) const override;
	virtual bool IsOnline() const override;
//////////////////////////////////////////////////////////////////////////

public:
	void SetId(const QString& id);
	void SetFullName(const QString& name);
	
	void AddToGroup(const QString& group);
	const QList<QString>& GetGroups() const;

	void SetPresence(ContactItemInterface::ContactItemPresence presence, const QString& statusMessage = EMPTY_STR);

private:
	QString id_;
	QString fullName_;
	QString name_;
	QPixmap avatar_;
	QList<QString> linkedGroups_;
	ContactItemInterface::ContactItemPresence presence_;
	QString statusMessage_;
	QString prevStatusMessage_;
};

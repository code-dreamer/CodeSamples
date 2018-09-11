#include "stdafx.h"
#include "ContactItem.h"
#include "XmppTools.h"
#include "XmppClient.h"
#include "ImManagerSettings.h"

namespace
{

QString PrecenseToDefaultStatusText(ContactItemInterface::ContactItemPresence presence)
{
	static_assert(ContactItemInterface::ContactItemPresenceCount == 9, "Unsupported items count in enum ContactItemInterface::ContactItemPresence");

	QString text;

	switch(presence) {
	case ContactItemInterface::Available:
		text = QCoreApplication::translate("ContactItem", "I'm online");
		break;

	case ContactItemInterface::Chat:
		text = QCoreApplication::translate("ContactItem", "I'm ready to chat!");
		break;
	
	case ContactItemInterface::Away:
		text = QCoreApplication::translate("ContactItem", "I'm is away");
		break;

	case ContactItemInterface::DND:
		text = QCoreApplication::translate("ContactItem", "Please, don't disturb Me");
		break;

	case ContactItemInterface::XA:
		text = QCoreApplication::translate("ContactItem", "I'm is unavailable");
		break;

	case ContactItemInterface::Unavailable:
		text = QCoreApplication::translate("ContactItem", "I'm is offline");
		break;

	case ContactItemInterface::Probe:
	case ContactItemInterface::Error:
	case ContactItemInterface::Invalid:
	case ContactItemInterface::ContactItemPresenceCount:
	default:
		G_ASSERT(false);
	};

	return text;
}

}

ContactItem::ContactItem(QObject* parent)
	: QObject(parent)
	, presence_(ContactItemInterface::Unavailable)
{

}

ContactItem::~ContactItem()
{

}

bool ContactItem::IsValidItemId(const QString& id)
{
	QSTRING_NOT_EMPTY(id);

	if (id.isEmpty()) {
		return false;
	}

	if (id.count(CHAR('@')) != 1 || id.count(CHAR('#')) != 1) {
		return false;
	}

	QString localId = id;
	int ind = localId.indexOf(STR("@") + ImManagerSettings::GetSettings()->GetXmppServerName());
	if (ind == -1) {
		return false;
	}

	localId.remove(ind, localId.length()-ind);
	ind = localId.indexOf(CHAR('#'));
	if (ind == -1) {
		return false;
	}

	return true;
}

bool ContactItem::IsBelongToGroup(const QString& groupName) const
{
	return (linkedGroups_.indexOf(groupName, 0) != -1);
}

void ContactItem::SetId(const QString& id)
{
	QSTRING_NOT_EMPTY(id);
	G_ASSERT( IsValidItemId(id) );

	id_ = id;
	QStringList splits = id_.split(CHAR('@'));
	G_ASSERT(splits.count() == 2);
	SetFullName( XmppTools::XmppIDToG2ID(splits[0]) );
}

void ContactItem::AddToGroup(const QString& group)
{
	QSTRING_NOT_EMPTY(group);
	G_ASSERT(!IsBelongToGroup(group));

	linkedGroups_.push_back(group);
}

const QString& ContactItem::GetId() const
{
	QSTRING_NOT_EMPTY(id_);
	return id_;
}

const QList<QString>& ContactItem::GetGroups() const
{
	return linkedGroups_;
}

ContactItemInterface::ContactItemPresence ContactItem::GetPresence() const
{
	G_ASSERT(0 <= presence_ && presence_ < ContactItemInterface::ContactItemPresenceCount);
	return presence_;
}

QString ContactItem::GetFullName() const
{
	QSTRING_NOT_EMPTY(fullName_);
	return fullName_;
}

const QString& ContactItem::GetName() const
{
	QSTRING_NOT_EMPTY(name_);
	return name_;
}

bool ContactItem::IsOnline() const
{
	G_ASSERT(0 <= presence_ && presence_ < ContactItemInterface::ContactItemPresenceCount);
	return (ContactItemInterface::Available <= presence_ && presence_ <= ContactItemInterface::XA);
}

void ContactItem::SetPresence(ContactItemInterface::ContactItemPresence presence, const QString& statusMessage)
{
	G_ASSERT(0 <= presence && presence < ContactItemInterface::ContactItemPresenceCount);
	
	presence_ = presence;

	QString currStatus = statusMessage.isEmpty() ? PrecenseToDefaultStatusText(presence) : statusMessage;
	SetStatusMessage(currStatus);
}

const QPixmap& ContactItem::GetAvatar()
{
	return avatar_;
}

const QString& ContactItem::GetStatusMessage() const
{
	return statusMessage_;
}

void ContactItem::SetAvatar(const QPixmap& avatar)
{
	avatar_ = avatar;
}

void ContactItem::SetStatusMessage(const QString& statusMessage)
{
	if(  statusMessage == statusMessage_ ){
		return;
	}
	prevStatusMessage_ = statusMessage_;
	statusMessage_ = statusMessage;
}

void ContactItem::SetFullName(const QString& fullname)
{
	QSTRING_NOT_EMPTY(fullname);
	G_ASSERT( fullname.contains(CHAR('@')) && !fullname.contains(CHAR('#')) );

	if (fullName_ != fullname) {
		QStringList splits = fullname.split(CHAR('@'));
		G_ASSERT(splits.count() == 2);
		name_ = splits[0];

		fullName_ = fullname;
	}
}

void ContactItem::ReturnPreviousStatusMessage()
{
	statusMessage_ = prevStatusMessage_;
}

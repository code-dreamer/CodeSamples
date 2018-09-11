#include "stdafx.h"

#include "GameParsers.h"
#include "GameManager.h"
#include "Game.h"

namespace
{
const QString s_gameTag = STR("game");
const QString s_gameNameTag = STR("name");
const QString s_gameOriginalNameTag = STR("original-name");
const QString s_gameAliasTag = STR("alias");
const QString s_gameIdTag = STR("id");
const QString s_gameGenreTag = STR("genre");
const QString s_gameVersionTag = STR("version");
const QString s_gameDemoTag = STR("demo");
const QString s_gameKeyAutoinstallSupportedTag = STR("key-autoinstall-supported");
const QString s_gameInstallCommandTag = STR("install-command");
const QString s_gameLaunchCommandTag = STR("launch-command");
const QString s_gamePublisherNameTag = STR("publisher-name");
const QString s_gameDeveloperNameTag = STR("developer-name");
const QString s_gameIconTag = STR("icon");
const QString s_gameDownloadTag = STR("download");
const QString s_gameKeysTag = STR("keys");
const QString s_gameKeyTag = STR("key");
const QString s_gameProductsTag = STR("products");

void SetGameProperty(GameInterface* game, const QString& propName, const QString& propValue)
{
	CHECK_PTR(game);
	QSTRING_NOT_EMPTY(propName);
	QSTRING_NOT_EMPTY(propValue);

	if (propName == s_gameNameTag) {
		game->SetName(propValue);
	}
	else if (propName == s_gameIdTag) {
		game->SetId(propValue);
	}
	else if (propName == s_gameGenreTag) {
		G_ASSERT(false);
	}
	else if (propName == s_gameVersionTag) {
		G_ASSERT(false);
	}
	else if (propName == s_gameDemoTag) {
		game->SetDemoFlag(propValue == STR("true") ? true : false);
	}
	else if (propName == s_gameKeyAutoinstallSupportedTag) {
		G_ASSERT(false);
	}
	else if (propName == s_gameInstallCommandTag) {
		G_ASSERT(false);
		m_currentGame->setInstallCommand(propValue);
	}
	else if (propName == s_gameLaunchCommandTag) {
		m_currentGame->setInstallCommand(propValue);
		G_ASSERT(false);
	}
	else if (propName == s_gamePublisherNameTag) {
		game->SetPublisher(propValue);
	}
	else if (propName == s_gameDeveloperNameTag) {
		game->SetDeveloper(propValue);
	}
	else if (propName == s_gameIconTag) {
		QUrl url;
		url.setEncodedUrl(propValue.toAscii());
		if (url.isValid()) {
			game->SetCoverUrl(url);
		}
	}
	else if (propName == s_gameDownloadTag) {
		QUrl url;
		url.setEncodedUrl(propValue.toAscii());
		game->SetDownloadUrl(url);
	}
	else if (propName == s_gameKeysTag) {
		G_ASSERT(false);
	}
	else if (propName == s_gameKeyTag) {
		G_ASSERT(false);
	}
	else if (propName == s_gameProductsTag) {
	}
	else if (propName == s_gameOriginalNameTag) {
		game->SetOriginalName(propValue);
	}
	else if (propName == s_gameAliasTag) {
		game->SetAliasName(propValue);
	}
	else {
		G_ASSERT(false);
	}
}

void ParseGameElement(GameInterface* gameToUpdate, const QDomElement& element)
{
	CHECK_PTR(gameToUpdate);
	G_ASSERT( !element.isNull() );

	for (QDomNode childNode = element.firstChild(); !childNode.isNull(); childNode = childNode.nextSibling()) {
		const QDomNode::NodeType childType = childNode.nodeType();
		if (childType == QDomNode::ElementNode) {
			const QDomElement element = childNode.toElement();
			ParseGameElement(gameToUpdate, element);
		}
		else if (childType == QDomNode::TextNode) {
			const QString elementName = element.tagName();
			const QString elementText = element.text();
			SetGameProperty(gameToUpdate, elementName, elementText);
		}
		else {
			G_ASSERT(false);
		}
	}
}

void ParseGameEntry(GameInterface* gameToUpdate, const QDomElement& gameEntry)
{
	CHECK_PTR(gameToUpdate);
	G_ASSERT( !gameEntry.isNull() );

	QDomNamedNodeMap attributes = gameEntry.attributes();
	const int attrCount = attributes.count();
	for (int i = 0; i < attrCount; ++i) {
		const QDomNode currNode = attributes.item(i);
		G_ASSERT(currNode.isAttr());
		if (currNode.isAttr()) {
			const QDomAttr currAttr = currNode.toAttr();
			const QString attrName = currAttr.name();
			const QString attrText = currAttr.value();
			SetGameProperty(gameToUpdate, attrName, attrText);
		}
	}
	
	ParseGameElement(gameToUpdate, gameEntry);
}

} // namespace

QList<GameInterface*> GameParsers::ParseFromXml(const QDomElement& domElement)
{
#ifdef DEBUG
	QString text = domElement.text();
#endif

	QList<GameInterface*> parsedGames;
	for (QDomNode childNode = domElement.firstChild(); !childNode.isNull(); childNode = childNode.nextSibling()) {
		G_ASSERT(childNode.isElement());
		if (childNode.isElement()) {
			const QDomElement element = childNode.toElement();
			G_ASSERT( element.tagName() == s_gameTag );
			if (element.tagName() == s_gameTag) {
				GameInterface* const game = GameManager::CreateGame();
				ParseGameEntry(game, element);
				parsedGames.push_back(game);
			}
		}
	}

	return parsedGames;
}

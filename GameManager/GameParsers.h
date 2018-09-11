#pragma once

class GameInterface;

namespace GameParsers
{
	QList<GameInterface*> ParseFromXml(const QDomElement& domElement);
}
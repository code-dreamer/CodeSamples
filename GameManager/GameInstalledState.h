#pragma once

#include "GameStateInterface.h"

class Game;
class GameStateInfo;
class ProcessManager;

class GAME_MANAGEMENT_API GameInstalledState : public QObject, public GameStateInterface
{
	Q_OBJECT

public:
	GameInstalledState(Game* game, QObject* parent = nullptr);

// GameStateInterface implementation
Q_SIGNALS:
	void StateInfoUpdated() const override;
public:
	virtual void DoAction() override;
	virtual const GameStateInfoInterface* GetStateInfo() const override;
//////////////////////////////////////////////////////////////////////////

// StorableObjectInterface implementation
	virtual void Store(QDataStream& dataStream) const override;
	virtual void Load(QDataStream& dataStream) override;
//////////////////////////////////////////////////////////////////////////

// ToQObjectConvertableInterface implementation
public:
	virtual QObject* ToQObject() override;
	virtual const QObject* ToQObject() const override;
//////////////////////////////////////////////////////////////////////////	

private Q_SLOTS:
	void OnGameStarted(QString gameRunFilePath);
	void OnGameFinished(QString gameRunFilePath, bool normalExit);

private:
	enum Substates
	{
		InitSubstate = 0,
		GameStartingSubstate,
		GameStartedSubstate,
		SubstatesCount = 3
	};

private:
	Game* game_;
	ProcessManager* processManager_;

	mutable GameStateInfo* stateInfo_;
	Substates currentSubstate_;
};
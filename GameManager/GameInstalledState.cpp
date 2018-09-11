#include "stdafx.h"
#include "Game.h"
#include "GameInstalledState.h"
#include "FileSystemTools.h"
#include "GameStateInfo.h"
#include "ProcessTools.h"
#include "ProcessManager.h"
#include "GameTools.h"
#include "MainModuleKeeper.h"
#include "MainModuleSettingsInterface.h"

GameInstalledState::GameInstalledState(Game* const game, QObject* const parent)
	: QObject(parent)
	, game_(game)
	, processManager_(nullptr)
	, stateInfo_(nullptr)
	, currentSubstate_(InitSubstate)
{
	CHECK_PTR(game_);

	// TODO: make one place switching to substate
	stateInfo_ = new GameStateInfo(this);
	stateInfo_->SetStateId(InstalledState);
	stateInfo_->SetStateName(tr("Ready to run"));
	stateInfo_->SetStateActionName(tr("Run game"));
	stateInfo_->SetStateInProcess(false);
	stateInfo_->SetStateAction(GameStateInfoInterface::StartAction);

	processManager_ = new ProcessManager(this);
	CHECKED_CONNECT(processManager_, SIGNAL(ProcessStarted(QString)), this, SLOT(OnGameStarted(QString)));
	CHECKED_CONNECT(processManager_, SIGNAL(ProcessFinished(QString, bool)), this, SLOT(OnGameFinished(QString, bool)));
}

QObject* GameInstalledState::ToQObject() // move to parent class
{
	return this;
}

const QObject* GameInstalledState::ToQObject() const // move to parent class
{
	return this;
}

void GameInstalledState::DoAction()
{
	static_assert(GameInstalledState::SubstatesCount == 3, "Unsupported items count in enum GameInstalledState::Substates");

	if (currentSubstate_ == InitSubstate) {
		CHECK_PTR(game_);

		currentSubstate_ = GameStartingSubstate;
		stateInfo_->SetStateName(tr("Run"));
		stateInfo_->SetStateActionName(tr("Running game..."));
		stateInfo_->SetStateInProcess(false);
		stateInfo_->SetStateAction(GameStateInfoInterface::DisabledAction);
		Q_EMIT StateInfoUpdated();

		bool gameReadyForRun = GameTools::IsGameReadyForRun(game_);
		if (!gameReadyForRun) {
			gameReadyForRun = GameTools::PrepareGameForRun(game_);
			if (gameReadyForRun) {
				QLocale currentLocale = MainModuleKeeper::GetMainModule()->GetSettings()->GetCurrentLocale();
				QString message = tr("Game %1 was founded.\nDo you want to run it now?").arg(currentLocale.quoteString(game_->GetName()));
				QMessageBox::StandardButton answer = QMessageBox::question(qApp->activeWindow(), tr("Game running"), message, 
					QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
				gameReadyForRun = (answer == QMessageBox::Yes);
			}
			else {
				QMessageBox::warning(qApp->activeWindow(), tr("Unfinished adjustment"), tr("Game adjustment not finished.\nIt's must be done before running."));
			}
		}

		if (gameReadyForRun) {
			CHECK_PTR(processManager_);
			const QString runFilePath = game_->GetRunFilePath();
			processManager_->StartProcess(runFilePath);
			return;
		}

		stateInfo_->SetStateId(InstalledState);
		stateInfo_->SetStateName(tr("Ready to run"));
		stateInfo_->SetStateActionName(tr("Run game"));
		stateInfo_->SetStateInProcess(false);
		stateInfo_->SetStateAction(GameStateInfoInterface::StartAction);
		currentSubstate_ = InitSubstate;
		Q_EMIT StateInfoUpdated();		
	}
	else if (currentSubstate_ == GameStartingSubstate) {
		// TODO: terminate game if running?
		currentSubstate_ = GameStartedSubstate;

		stateInfo_->SetStateId(InstalledState);
		stateInfo_->SetStateName(tr("Waiting for game ending..."));
		stateInfo_->SetStateActionName(tr("Waiting for game ending..."));
		stateInfo_->SetStateInProcess(false);
		stateInfo_->SetStateAction(GameStateInfoInterface::DisabledAction);

		Q_EMIT StateInfoUpdated();
	}
	else if (currentSubstate_ == GameStartedSubstate) {
		// TODO: terminate game if running?
		currentSubstate_ = InitSubstate;
		
		stateInfo_->SetStateId(InstalledState);
		stateInfo_->SetStateName(tr("Ready to run"));
		stateInfo_->SetStateActionName(tr("Run game"));
		stateInfo_->SetStateInProcess(false);
		stateInfo_->SetStateAction(GameStateInfoInterface::StartAction);

		Q_EMIT StateInfoUpdated();
	}
	else {
		G_ASSERT(false);
	}
}

const GameStateInfoInterface* GameInstalledState::GetStateInfo() const
{
	CHECK_PTR(stateInfo_);
	return stateInfo_;
}

void GameInstalledState::OnGameStarted(QString gameRunFilePath)
{
	G_ASSERT(currentSubstate_ == GameStartingSubstate);
	G_ASSERT(game_->GetRunFilePath().compare(gameRunFilePath, Qt::CaseInsensitive) == 0);

	DoAction();
}

void GameInstalledState::OnGameFinished(QString gameRunFilePath, bool G_UNUSED(normalExit))
{
	G_ASSERT(currentSubstate_ == GameStartedSubstate);
	G_ASSERT(game_->GetRunFilePath().compare(gameRunFilePath, Qt::CaseInsensitive) == 0);
	
	DoAction();
}

void GameInstalledState::Store(QDataStream& G_UNUSED(dataStream)) const
{
}

void GameInstalledState::Load(QDataStream& G_UNUSED(dataStream))
{
}

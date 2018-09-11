#include "stdafx.h"

#include "ExceptionHandler.h"
#include "WinApiTools.h"
#include "MainModuleKeeper.h"
#include "ErrorReporter.h"
#include "StringTools.h"

static DWORD ExceptionSeverityToWinApiIcon(const ErrorSeverity& severity);

void ExceptionHandler::NotifyUser(const ExceptionInterface& exception, const QWidget* parentWidget /*= nullptr*/)
{
	const ErrorSeverity exceptionSeverity = exception.GetSeverity();
	QString errorText = exception.GetUserDescription();
	DWORD flags = ExceptionSeverityToWinApiIcon(exceptionSeverity) | MB_TOPMOST;

	if (exceptionSeverity > CriticalErrorSeverity) {
		errorText += QCoreApplication::translate("ExceptionHandler", "\n\nThis is serious error. You can help us to fix it, by sending automated report. Do it now?");
		flags |= MB_YESNO;
	}
	else {
		flags |= MB_OK;
	}

	int result = WinApiTools::ShowWinMessageBox(parentWidget, QCoreApplication::translate("ExceptionHandler", "An error occurred"), errorText, flags);
	if (result == IDYES) {
		SendExceptionToDevelopers(exception);
	}
}

void ExceptionHandler::NotifyUser(const std::exception& exception, const QWidget* parentWidget /*= nullptr*/)
{
	const ErrorSeverity exceptionSeverity = CriticalErrorSeverity;
	QString errorText = StringTools::CreateQString(exception.what());
	DWORD flags = ExceptionSeverityToWinApiIcon(exceptionSeverity) | MB_TOPMOST | MB_OK;

	WinApiTools::ShowWinMessageBox(parentWidget, QCoreApplication::translate("ExceptionHandler", "An error occurred"), errorText, flags);
}

void ExceptionHandler::ShowError(const QString& errorMessage, ErrorSeverity exceptionSeverity, QWidget* const parentWidget)
{
	const QString title = QCoreApplication::translate("ExceptionHandler", "Attention!");
	switch(exceptionSeverity) {
	case InformationErrorSeverity:
		QMessageBox::information(parentWidget, title, errorMessage);
		break;

	case WarningErrorSeverity:
		QMessageBox::warning(parentWidget, title, errorMessage);
		break;

	case CriticalErrorSeverity:
		QMessageBox::critical(parentWidget, title, errorMessage);
		break;

	default:
		G_ASSERT(false);
	}
}

void ExceptionHandler::SendExceptionToDevelopers(const ExceptionInterface& exception)
{
	try {
		MainModuleKeeper::GetMainModule()->GetErrorReporter()->SendExceptionToDevelopers(exception);
	}
	catch (ExceptionInterface& exception) {
		WinApiTools::ShowMessageBox( nullptr, QCoreApplication::translate("ExceptionHandler", "Error reporter error"), 
										exception.GetUserDescription(), QMessageBox::Critical );
	}
}

DWORD ExceptionSeverityToWinApiIcon(const ErrorSeverity& severity)
{
	DWORD icon = 0;

	switch (severity) {
	case InformationErrorSeverity:
		icon = MB_ICONINFORMATION;
		break;

	case WarningErrorSeverity:
		icon = MB_ICONWARNING;
		break;

	case CriticalErrorSeverity:
		icon = MB_ICONERROR;
		break;

	default:
		G_ASSERT(false);
	}

	G_ASSERT(icon != 0);
	return icon;
}

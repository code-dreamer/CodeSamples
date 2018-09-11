#pragma once

#include "NonCopyable.h"
#include "ExceptionInterface.h"

class ExceptionInterface;

class KERNEL_API ExceptionHandler : private NonCopyable
{
private:
	ExceptionHandler();
	~ExceptionHandler();

public:
	static void NotifyUser(const ExceptionInterface& exception, const QWidget* parentWidget = nullptr);
	static void NotifyUser(const std::exception& exception, const QWidget* parentWidget = nullptr);
	
	static void ShowError(const QString& errorMessage, ErrorSeverity exceptionSeverity, QWidget* parentWidget = nullptr);
	static void SendExceptionToDevelopers(const ExceptionInterface& exception);
};
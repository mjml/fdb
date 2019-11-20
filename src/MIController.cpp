#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "MIController.h"

MIController::MIController(PMainWindow _ui)
  : ui(_ui)
{

}

MIController::~MIController()
{
  ui.reset();
}

void MIController::parse_exec_line(QString &qs)
{

}

void MIController::parse_notify_line(QString &qs)
{

}

void MIController::parse_status_line(QString &qs)
{

}

void MIController::parse_console_line(QString &qs)
{

}

void MIController::parse_target_line(QString &qs)
{

}

void MIController::parse_log_line(QString &qs)
{

}

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "util/log.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include "FactorioProcess.h"
#include <QMainWindow>
#include <QPlainTextEdit>



namespace Ui {
class MainWindow;
class SettingsDialog;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  void initialize_actions ();

private:
  Ui::MainWindow*      ui;
  Ui::SettingsDialog*  settings;

  GDBProcess gdb;
  FactorioProcess factorio;

public slots:
  void start_factorio();

  void kill_factorio();

  void restart_gdb();

  void kill_gdb();
};

#endif // MAINWINDOW_H

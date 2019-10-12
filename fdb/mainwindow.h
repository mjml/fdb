#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include "util/log.hpp"
#include "gui/QTerminalDock.h"


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

  void initialize_factorio();
  void start_factorio();
  void kill_factorio();

  void initialize_gdb();
  void start_gdb();
  void restart_gdb();
  void kill_gdb();

private:
  Ui::MainWindow *ui;
  Ui::SettingsDialog *settings;


  // Designer doesn't do a great job with custom QDockWidgets
  QTerminalDock* gdbDock;
  QTerminalDock* gmiDock;
  QTerminalDock* factorioDock;

  QProcess* factorioProc;
  QProcess* gdbProc;

};

#endif // MAINWINDOW_H

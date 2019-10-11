#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <gui/QTerminalProcess.h>
#include <gui/QTerminalWidget.h>


// hacked working directory

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  void initialize_process_window(QTerminalProcess& proc, QTerminalWidget& textEdit, bool is_controlling);

private:
  Ui::MainWindow *ui;
  QProcess* gdbProc;

};

#endif // MAINWINDOW_H

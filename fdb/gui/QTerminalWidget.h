#ifndef QTERMINALWIDGET_H
#define QTERMINALWIDGET_H

#include <QWidget>
#include <QProcess>
#include <sys/ioctl.h>

struct QTerminalWidget : public QWidget
{
  Q_OBJECT
public:
  explicit QTerminalWidget(QWidget *parent = nullptr);
  virtual ~QTerminalWidget();

  struct winsize getTerminalDimensions ();

public slots:
  void onErrorOccurred (QProcess::ProcessError error);
  void onFinished (int exitcode, QProcess::ExitStatus status);
  void onStateChanged (QProcess::ProcessState newState);

protected:
  virtual bool event (QEvent* event) override;

};

#endif // QTERMINALWIDGET_H

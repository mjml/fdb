#ifndef QTERMINALWIDGET_H
#define QTERMINALWIDGET_H

#include <QWidget>
#include <sys/ioctl.h>

class QTerminalWidget : public QWidget
{
  Q_OBJECT
public:
  explicit QTerminalWidget(QWidget *parent = nullptr);

signals:

public slots:
  struct winsize getTerminalDimensions ();

};

#endif // QTERMINALWIDGET_H

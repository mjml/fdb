#include <QLayout>
#include <QPlainTextEdit>
#include <QProcess>
#include <sys/ioctl.h>

#include "gui/QTerminalWidget.h"
#include "util/exceptions.hpp"


QTerminalWidget::QTerminalWidget (QWidget *parent)
  : QWidget(parent)
{
}

QTerminalWidget::~QTerminalWidget ()
{
}

struct winsize QTerminalWidget::getTerminalDimensions ()
{
  QPlainTextEdit* first_edit = nullptr;
  QLayout* lyt = this->layout();

  for (int i=0; i < lyt->count(); i++) {
    QWidget* editctl = lyt->itemAt(i)->widget();
    if (!editctl) continue;
    first_edit = dynamic_cast<QPlainTextEdit*>(editctl);
    if (first_edit) break;
  }
  if (!first_edit) {
    Throw(std::logic_error,"Expected to find a QPlainTextEdit inside this QTerminalWidget");
  }

  auto chfmt = first_edit->currentCharFormat();
  auto fnt = chfmt.font();
  auto metrics = QFontMetrics(fnt);

  struct winsize win;  memset(&win, 0, sizeof(winsize));
  win.ws_col = static_cast<unsigned short>(first_edit->width() / metrics.horizontalAdvance(QChar(' ')));
  win.ws_row = static_cast<unsigned short>(first_edit->height() / metrics.height());
  win.ws_xpixel = static_cast<unsigned short>(metrics.horizontalAdvance(QChar(' ')));
  win.ws_ypixel = static_cast<unsigned short>(metrics.height());

  return win;

}


void QTerminalWidget::onErrorOccurred (QProcess::ProcessError error)
{

}


void QTerminalWidget::onFinished (int exitcode, QProcess::ExitStatus status)
{

}


void QTerminalWidget::onStateChanged (QProcess::ProcessState newState)
{

}


bool QTerminalWidget::event (QEvent *event)
{
  return QWidget::event(event);
}


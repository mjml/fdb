#include <QBoxLayout>
#include <QPushButton>
#include "gui/QOptionsDock.h"


QOptionsTitleBar::QOptionsTitleBar ()
{
  auto lyt = new QHBoxLayout();
  label = new QLabel();
  label->setText("test");
  label->setFont(QFont(QStringLiteral("Lucida Console"),11));
  setLayout(lyt);
  lyt->addWidget(label);
}


QOptionsTitleBar::~QOptionsTitleBar ()
{
}


void QOptionsTitleBar::add (QWidget *w)
{
  QFont myfont = QFont(QStringLiteral("Deja Vu Sans"),7);
  w->setFont(myfont);

}


QOptionsDock::QOptionsDock ()
  : QDockWidget(), titleBar(nullptr)
{
  titleBar = new QOptionsTitleBar();
  setTitleBarWidget(titleBar);

}


QOptionsDock::QOptionsDock (QWidget* parent)
  : QDockWidget(parent), titleBar(nullptr)
{
  titleBar = new QOptionsTitleBar();
  setTitleBarWidget(titleBar);
}


QOptionsDock::~QOptionsDock ()
{

}

void QOptionsDock::setWindowTitle (const QString &title)
{
  auto label = titleBar->label;
  QDockWidget::setWindowTitle(title);
  label->setText(title);
}


void QOptionsDock::resizeEvent (QResizeEvent *event)
{
  QSize szcopy = event->size();
  szcopy.rwidth() -= 4;
  titleBar->resize(szcopy);
  titleBar->move(2,2);
}


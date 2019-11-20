#include <QBoxLayout>

#include "gui/QOptionsDock.h"


QOptionsTitleBar::QOptionsTitleBar ()
  : QWidget(), label(nullptr)
{
  QFont heading = QFont(QStringLiteral("Monospace"),6);

  label = new QLabel();
  label->setText("test");
  label->setTextFormat(Qt::PlainText);
  label->setFont(heading);
  label->setStyleSheet("color:black");

  QHBoxLayout* lyt = new QHBoxLayout();
  lyt->setMargin(1);
  lyt->setSpacing(1);
  setLayout(lyt);
  lyt->addSpacing(6);
  lyt->addWidget(label);
}


QOptionsTitleBar::~QOptionsTitleBar ()
{
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


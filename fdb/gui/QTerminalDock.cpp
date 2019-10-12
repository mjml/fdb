#include <QPushButton>
#include <QBoxLayout>
#include "gui/QTerminalDock.h"


QTerminalDock::QTerminalDock()
  : QOptionsDock (), lszbtn(nullptr), lszedit(nullptr), freezeChk(nullptr),_process(nullptr)
{
  QFont mini(QStringLiteral("Monospace"),5);

  lszbtn = new QPushButton(this);
  lszbtn->setFont(mini);
  lszbtn->setStyleSheet("background-color: #808080");
  lszbtn->setText(QStringLiteral("inf"));
  lszbtn->setMaximumWidth(48);
  lszbtn->setCheckable(true);
  lszbtn->setChecked(true);

  QObject::connect (lszbtn,    &QPushButton::clicked,
                    this,      &QTerminalDock::logSizeLabelClicked);

  lszedit = new QLineEdit(this);
  lszedit->setFont(mini);
  lszedit->setMaximumWidth(48);
  lszedit->setMaximumHeight(20);
  lszedit->setMaxLength(7);
  lszedit->setPlaceholderText(QStringLiteral("numlines"));
  lszedit->setInputMask("0000000;");
  lszedit->setAlignment(Qt::AlignCenter);
  lszedit->setStyleSheet("background:grey");
  lszedit->hide();

  QObject::connect (lszedit,   &QLineEdit::editingFinished,
                    this,      &QTerminalDock::logSizeEdited);


  freezeChk = new QCheckBox(this);
  freezeChk->setFont(mini);
  freezeChk->setText("lock");

  auto lyt = dynamic_cast<QHBoxLayout*>(titleBar->layout());
  lyt->addWidget(lszbtn);
  lyt->addWidget(lszedit);
  lyt->addWidget(freezeChk);
  lyt->addStretch(1);

  this->setMaximumHeight(40);

}

QTerminalDock::QTerminalDock (QWidget *parent)
  : QTerminalDock()
{
  this->setParent(parent);
}

QTerminalDock::~QTerminalDock ()
{

}



void QTerminalDock::logSizeLabelClicked (bool checked)
{
  if (checked) {
    lszedit->hide();
    lszbtn->show();
  } else {
    lszbtn->hide();
    lszedit->setText("");
    lszedit->setCursorPosition(0);
    lszedit->show();
    lszedit->setFocus();
  }
}

void QTerminalDock::logSizeEdited()
{
  QString qs = lszedit->text();

  // TODO: store this size somewhere, or modify the edit controls
  auto sz = qs.toInt();
  if (sz < 10) {
    sz = 0;
    lszedit->hide();
    lszbtn->setChecked(true);
    lszbtn->show();
  } else {
    lszedit->setStyleSheet("background-color: #808080");
    this->setFocus();
  }

}

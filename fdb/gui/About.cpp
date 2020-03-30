#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include "gui/About.h"
#include "ui_about.h"

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
  ui->setupUi(this);

  QGraphicsScene* scene = new QGraphicsScene();
  QGraphicsView* view = new QGraphicsView(scene);
  QPixmap pixmap = QPixmap::fromImage(":/resources/link-splash.png");
  QGraphicsPixmapItem* item = new QGraphicsPixmapItem();
  scene->addItem(item);
  view->show();
}

About::~About()
{
  delete ui;
}

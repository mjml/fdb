#ifndef QOPTIONSDOCK_H
#define QOPTIONSDOCK_H

#include <QDockWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QCheckBox>

template <typename T>
struct widget_traits {};

struct QOptionsTitleBar : public QWidget
{
public:
  QOptionsTitleBar();
  ~QOptionsTitleBar();

  void add (QWidget* w);

  /*  -- not sure all this specialization is rly necessary
  void add (QLabel* lbl);

  void add (QPlainTextEdit* edit);

  void add (QCheckBox* cb);
  */

  QLabel* label;
};


class QOptionsDock : public QDockWidget
{

public:
  QOptionsDock();
  QOptionsDock(QWidget* parent);
  virtual ~QOptionsDock();

  QOptionsTitleBar* titleBar;

public:
  virtual void setWindowTitle (const QString& title);

protected:
  virtual void resizeEvent (QResizeEvent* event);


};

#endif // QOPTIONSDOCK_H

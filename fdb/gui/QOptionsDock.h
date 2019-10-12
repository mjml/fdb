#ifndef QOPTIONSDOCK_H
#define QOPTIONSDOCK_H

#include <QDockWidget>
#include <QLabel>
#include <QLabel>

template <typename T>
struct widget_traits {};

struct QOptionsTitleBar : public QWidget
{
public:
  QOptionsTitleBar();
  ~QOptionsTitleBar();

  QLabel* label;

};


/**
 *   Provides an augmented QDockWidget with the ability to add widgets into a titlebar with a QHBoxLayout layout.
 * Not to be used with the vertical title bar flag.
 *
 * @brief Augments QDockWidget with a titlebar that serves as a container, in addition to a label.
 */
class QOptionsDock : public QDockWidget
{

public:
  QOptionsDock();
  QOptionsDock(QWidget* parent);
  virtual ~QOptionsDock();

  QOptionsTitleBar* titleBar;

public:
  virtual void setWindowTitle (const QString& title);

};

#endif // QOPTIONSDOCK_H

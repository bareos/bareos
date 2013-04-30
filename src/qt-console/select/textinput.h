
#ifndef _TEXTENTRY_H_
#define _TEXTENTRY_H_

#include <QtGui>
#include "ui_textinput.h"
#include "console.h"

class textInputDialog : public QDialog, public Ui::textInputForm
{
   Q_OBJECT 

public:
   textInputDialog(Console *console, int conn);

public slots:
   void accept();
   void reject();

private:
   Console *m_console;
   int m_conn;
};

#endif /* _TEXTENTRY_H_ */

#ifndef MONITORTAB_H
#define MONITORTAB_H

class MonitorTab : public QObject
{
public:
   MonitorTab(QString tabRefString, QObject* parent = 0)
       : QObject(parent)
       , tab(new QWidget)
       , textEdit(new QPlainTextEdit(tab))
   {
      QVBoxLayout *vLayout = new QVBoxLayout(tab);
      textEdit->setObjectName(tabRefString);
      textEdit->setReadOnly(true);
      textEdit->setFont(QFont("courier"));
      vLayout->addWidget(textEdit);
   }

   ~MonitorTab()
   {
      // do not delete widgets since they
      // all have a parent that does the work
   }

   QWidget* getTabWidget() const { return tab; }
   QPlainTextEdit* getTextEdit() const { return textEdit; }

private:
   QWidget* tab;
   QPlainTextEdit *textEdit;
};

#endif // MONITORTAB_H

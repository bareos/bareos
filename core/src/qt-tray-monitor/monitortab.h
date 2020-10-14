/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#ifndef MONITORTAB_H
#define MONITORTAB_H

class MonitorTab : public QObject {
 public:
  MonitorTab(QString tabRefString, QObject* parent = 0)
      : QObject(parent), tab(new QWidget), textEdit(new QPlainTextEdit(tab))
  {
    QVBoxLayout* vLayout = new QVBoxLayout(tab);
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
  QPlainTextEdit* textEdit;
};

#endif  // MONITORTAB_H

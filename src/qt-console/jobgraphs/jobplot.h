#ifndef _JOBPLOT_H_
#define _JOBPLOT_H_
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Version $Id$
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include "pages.h"
#include "ui_jobplotcontrols.h"
#include <qwt_data.h>
#include <qwt_legend.h>
#include <qwt_plot_curve.h>
#include <qwt_plot.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <qwt_symbol.h>
#include <qwt_scale_map.h>
#include <qwt_scale_draw.h>
#include <qwt_text.h>

/*
 * Structure to hold data items of jobs when and how much.
 * If I worked at it I could eliminate this.  It's just the way it evolved.
 */
struct PlotJobData
{
   double files;
   double bytes;
   QDateTime dt;
};

/*
 * Class for the purpose of having a single object to pass data to the JobPlot
 * Constructor.  The other option was a constructor with this many passed 
 * values or some sort of code to parse a list.  I liked this best at the time.
 */
class JobPlotPass
{
public:
   JobPlotPass();
   JobPlotPass& operator=(const JobPlotPass&);
   bool use;
   Qt::CheckState recordLimitCheck;
   Qt::CheckState daysLimitCheck;
   int recordLimitSpin;
   int daysLimitSpin;
   QString jobCombo;
   QString clientCombo;
   QString volumeCombo;
   QString fileSetCombo;
   QString purgedCombo;
   QString levelCombo;
   QString statusCombo;
};

/*
 * Class to Change the display of the time scale to display dates.
 */
class DateTimeScaleDraw : public QwtScaleDraw
{
public:
   virtual QwtText label(double v) const
   {
      QDateTime dtlabel(QDateTime::fromTime_t((uint)v));
      return dtlabel.toString("M-d-yy");
   }
};

/*
 * These are the user interface control widgets as a separate class.
 * Separately for the purpos of having the controls in a Scroll Area.
 */
class JobPlotControls : public QWidget, public Ui::JobPlotControlsForm
{
   Q_OBJECT

public:
   JobPlotControls();
};

/*
 * The main class
 */
class JobPlot : public Pages
{
   Q_OBJECT 

public:
   JobPlot(QTreeWidgetItem *parentTreeWidgetItem, JobPlotPass &);
   ~JobPlot();
   virtual void currentStackItem();

private slots:
   void setPlotType(QString);
   void setFileSymbolType(int);
   void setByteSymbolType(int);
   void fileCheckChanged(int);
   void byteCheckChanged(int);
   void reGraph();

private:
   void fillSymbolCombo(QComboBox *q);
   void setSymbolType(int, int type);
   void addCurve();
   void writeSettings();
   void readSplitterSettings();
   void readControlSettings();
   void setupControls();
   void runQuery();
   bool m_drawn;
   JobPlotPass m_pass;
   JobPlotControls* controls;
   QList<PlotJobData *> m_pjd;
   QwtPlotCurve *m_fileCurve;
   QwtPlotCurve *m_byteCurve;
   /* from the user interface before using scroll area */
   void setupUserInterface();
   QGridLayout *m_gridLayout;
   QSplitter *m_splitter;
   QwtPlot *m_jobPlot;
};

#endif /* _JOBPLOT_H_ */

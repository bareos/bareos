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
 *  JobPlots Class
 *
 *   Dirk Bartley, March 2007
 *
 */ 

#include "bat.h"
#include <QtGui>
#include "util/comboutil.h"
#include "jobgraphs/jobplot.h"


JobPlotPass::JobPlotPass()
{
   use = false;
}

JobPlotPass& JobPlotPass::operator=(const JobPlotPass &cp)
{
   use = cp.use;
   recordLimitCheck = cp.recordLimitCheck;
   daysLimitCheck = cp.daysLimitCheck;
   recordLimitSpin = cp.recordLimitSpin;
   daysLimitSpin = cp.daysLimitSpin;
   jobCombo = cp.jobCombo;
   clientCombo = cp.clientCombo;
   volumeCombo = cp.volumeCombo;
   fileSetCombo = cp.fileSetCombo;
   purgedCombo = cp.purgedCombo;
   levelCombo = cp.levelCombo;
   statusCombo = cp.statusCombo;
   return *this;
}

/*
 * Constructor for the controls class which inherits QScrollArea and a ui header
 */
JobPlotControls::JobPlotControls()
{
   setupUi(this);
}

/*
 * Constructor, this class does not inherit anything but pages.
 */
JobPlot::JobPlot(QTreeWidgetItem *parentTreeWidgetItem, JobPlotPass &passVals)
   : Pages()
{
   setupUserInterface();
   pgInitialize(tr("JobPlot"), parentTreeWidgetItem);
   readSplitterSettings();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/applications-graphics.png")));
   m_drawn = false;

   /* this invokes the pass values = operator function */
   m_pass = passVals;
   dockPage();
   /* If the values of the controls are predetermined (from joblist), then set
    * this class as current window at the front of the stack */
   if (m_pass.use)
      setCurrent();
   m_jobPlot->replot();
}

/*
 * Kill, crush Destroy
 */
JobPlot::~JobPlot()
{
   if (m_drawn)
      writeSettings();
   m_pjd.clear();
}

/*
 * This is called when the page selector has this page selected
 */
void JobPlot::currentStackItem()
{
   if (!m_drawn) {
      setupControls();
      reGraph();
      m_drawn=true;
   }
}

/*
 * Slot for the refresh push button, also called from constructor.
 */
void JobPlot::reGraph()
{
   /* clear m_pjd */
   m_pjd.clear();
   runQuery();
   m_jobPlot->clear();
   addCurve();
   m_jobPlot->replot();
}

/*
 * Setup the control widgets for the graph, this are the objects from JobPlotControls
 */
void JobPlot::setupControls()
{
   QStringList graphType = QStringList() << /* tr("Fitted") <<*/ tr("Sticks")
                                         << tr("Lines") << tr("Steps") << tr("None");
   controls->plotTypeCombo->addItems(graphType);

   fillSymbolCombo(controls->fileSymbolTypeCombo);
   fillSymbolCombo(controls->byteSymbolTypeCombo);

   readControlSettings();

   controls->fileCheck->setCheckState(Qt::Checked);
   controls->byteCheck->setCheckState(Qt::Checked);
   connect(controls->plotTypeCombo, SIGNAL(currentIndexChanged(QString)), this, SLOT(setPlotType(QString)));
   connect(controls->fileSymbolTypeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setFileSymbolType(int)));
   connect(controls->byteSymbolTypeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setByteSymbolType(int)));
   connect(controls->fileCheck, SIGNAL(stateChanged(int)), this, SLOT(fileCheckChanged(int)));
   connect(controls->byteCheck, SIGNAL(stateChanged(int)), this, SLOT(byteCheckChanged(int)));
   connect(controls->refreshButton, SIGNAL(pressed()), this, SLOT(reGraph()));

   controls->clientComboBox->addItem(tr("Any"));
   controls->clientComboBox->addItems(m_console->client_list);

   QStringList volumeList;
   getVolumeList(volumeList);
   controls->volumeComboBox->addItem(tr("Any"));
   controls->volumeComboBox->addItems(volumeList);
   controls->jobComboBox->addItem(tr("Any"));
   controls->jobComboBox->addItems(m_console->job_list);

   levelComboFill(controls->levelComboBox);

   boolComboFill(controls->purgedComboBox);

   controls->fileSetComboBox->addItem(tr("Any"));
   controls->fileSetComboBox->addItems(m_console->fileset_list);
   QStringList statusLongList;
   getStatusList(statusLongList);
   controls->statusComboBox->addItem(tr("Any"));
   controls->statusComboBox->addItems(statusLongList);

   if (m_pass.use) {
      controls->limitCheckBox->setCheckState(m_pass.recordLimitCheck);
      controls->limitSpinBox->setValue(m_pass.recordLimitSpin);
      controls->daysCheckBox->setCheckState(m_pass.daysLimitCheck);
      controls->daysSpinBox->setValue(m_pass.daysLimitSpin);

      comboSel(controls->jobComboBox, m_pass.jobCombo);
      comboSel(controls->clientComboBox, m_pass.clientCombo);
      comboSel(controls->volumeComboBox, m_pass.volumeCombo);
      comboSel(controls->fileSetComboBox, m_pass.fileSetCombo);
      comboSel(controls->purgedComboBox, m_pass.purgedCombo);
      comboSel(controls->levelComboBox, m_pass.levelCombo);
      comboSel(controls->statusComboBox, m_pass.statusCombo);

   } else {
      /* Set Defaults for check and spin for limits */
      controls->limitCheckBox->setCheckState(mainWin->m_recordLimitCheck ? Qt::Checked : Qt::Unchecked);
      controls->limitSpinBox->setValue(mainWin->m_recordLimitVal);
      controls->daysCheckBox->setCheckState(mainWin->m_daysLimitCheck ? Qt::Checked : Qt::Unchecked);
      controls->daysSpinBox->setValue(mainWin->m_daysLimitVal);
   } 
}

/*
 * Setup the control widgets for the graph, this are the objects from JobPlotControls
 */
void JobPlot::runQuery()
{
   /* Set up query */
   QString query("");
   query += "SELECT DISTINCT "
            " Job.Starttime AS JobStart,"
            " Job.Jobfiles AS FileCount,"
            " Job.JobBytes AS Bytes,"
            " Job.JobId AS JobId"
            " FROM Job"
            " JOIN Client ON (Client.ClientId=Job.ClientId)"
            " JOIN Status ON (Job.JobStatus=Status.JobStatus)"
            " LEFT OUTER JOIN FileSet ON (FileSet.FileSetId=Job.FileSetId)";

   QStringList conditions;
   comboCond(conditions, controls->jobComboBox, "Job.Name");
   comboCond(conditions, controls->clientComboBox, "Client.Name");
   int volumeIndex = controls->volumeComboBox->currentIndex();
   if ((volumeIndex != -1) && (controls->volumeComboBox->itemText(volumeIndex) != tr("Any"))) {
      query += " LEFT OUTER JOIN JobMedia ON (JobMedia.JobId=Job.JobId)"
               " LEFT OUTER JOIN Media ON (JobMedia.MediaId=Media.MediaId)";
      conditions.append("Media.VolumeName='" + controls->volumeComboBox->itemText(volumeIndex) + "'");
   }
   comboCond(conditions, controls->fileSetComboBox, "FileSet.FileSet");
   boolComboCond(conditions, controls->purgedComboBox, "Job.PurgedFiles");
   levelComboCond(conditions, controls->levelComboBox, "Job.Level");
   comboCond(conditions, controls->statusComboBox, "Status.JobStatusLong");

   /* If Limit check box For limit by days is checked  */
   if (controls->daysCheckBox->checkState() == Qt::Checked) {
      QDateTime stamp = QDateTime::currentDateTime().addDays(-controls->daysSpinBox->value());
      QString since = stamp.toString(Qt::ISODate);
      conditions.append("Job.Starttime>'" + since + "'");
   }
   bool first = true;
   foreach (QString condition, conditions) {
      if (first) {
         query += " WHERE " + condition;
         first = false;
      } else {
         query += " AND " + condition;
      }
   }
   /* Descending */
   query += " ORDER BY Job.Starttime DESC, Job.JobId DESC";
   /* If Limit check box for limit records returned is checked  */
   if (controls->limitCheckBox->checkState() == Qt::Checked) {
      QString limit;
      limit.setNum(controls->limitSpinBox->value());
      query += " LIMIT " + limit;
   }

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",query.toUtf8().data());
   }
   QString resultline;
   QStringList results;
   if (m_console->sql_cmd(query, results)) {

      QString field;
      QStringList fieldlist;
   
      int row = 0;
      /* Iterate through the record returned from the query */
      foreach (resultline, results) {
         PlotJobData *plotJobData = new PlotJobData();
         fieldlist = resultline.split("\t");
         int column = 0;
         QString statusCode("");
         /* Iterate through fields in the record */
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            if (column == 0) {
               plotJobData->dt = QDateTime::fromString(field, mainWin->m_dtformat);
            } else if (column == 1) {
               plotJobData->files = field.toDouble();
            } else if (column == 2) {
               plotJobData->bytes = field.toDouble();
            }
            column++;
            m_pjd.prepend(plotJobData);
         }
         row++;
      }
   } 
   if ((controls->volumeComboBox->itemText(volumeIndex) != tr("Any")) && (results.count() == 0)){
      /* for context sensitive searches, let the user know if there were no
       *        * results */
      QMessageBox::warning(this, "Bat",
          tr("The Jobs query returned no results.\n"
         "Press OK to continue?"), QMessageBox::Ok );
   }
}

/*
 * The user interface that used to be in the ui header.  I wanted to have a
 * scroll area which is not in designer.
 */
void JobPlot::setupUserInterface()
{
   QSizePolicy sizePolicy(static_cast<QSizePolicy::Policy>(1), static_cast<QSizePolicy::Policy>(5));
   sizePolicy.setHorizontalStretch(0);
   sizePolicy.setVerticalStretch(0);
   sizePolicy.setVerticalStretch(0);
   sizePolicy.setVerticalPolicy(QSizePolicy::Ignored);
   sizePolicy.setHorizontalPolicy(QSizePolicy::Ignored);
   m_gridLayout = new QGridLayout(this);
   m_gridLayout->setSpacing(6);
   m_gridLayout->setMargin(9);
   m_gridLayout->setObjectName(QString::fromUtf8("m_gridLayout"));
   m_splitter = new QSplitter(this);
   m_splitter->setObjectName(QString::fromUtf8("m_splitter"));
   m_splitter->setOrientation(Qt::Horizontal);
   m_jobPlot = new QwtPlot(m_splitter);
   m_jobPlot->setObjectName(QString::fromUtf8("m_jobPlot"));
   m_jobPlot->setSizePolicy(sizePolicy);
   m_jobPlot->setMinimumSize(QSize(0, 0));
   QScrollArea *area = new QScrollArea(m_splitter);
   area->setObjectName(QString::fromUtf8("area"));
   controls = new JobPlotControls();
   area->setWidget(controls);
   
   m_splitter->addWidget(m_jobPlot);
   m_splitter->addWidget(area);

   m_gridLayout->addWidget(m_splitter, 0, 0, 1, 1);
}

/*
 * Add the curves to the plot
 */
void JobPlot::addCurve()
{
   m_jobPlot->setTitle(tr("Files and Bytes backed up"));
   m_jobPlot->insertLegend(new QwtLegend(), QwtPlot::RightLegend);

   // Set axis titles
   m_jobPlot->enableAxis(QwtPlot::yRight);
   m_jobPlot->setAxisTitle(QwtPlot::yRight, tr("<-- Bytes Kb"));
   m_jobPlot->setAxisTitle(m_jobPlot->xBottom, tr("date of backup -->"));
   m_jobPlot->setAxisTitle(m_jobPlot->yLeft, tr("Number of Files -->"));
   m_jobPlot->setAxisScaleDraw(QwtPlot::xBottom, new DateTimeScaleDraw());

   // Insert new curves
   m_fileCurve = new QwtPlotCurve( tr("Files") );
   m_fileCurve->setPen(QPen(Qt::red));
   m_fileCurve->setCurveType(m_fileCurve->Yfx);
   m_fileCurve->setYAxis(QwtPlot::yLeft);

   m_byteCurve = new QwtPlotCurve(tr("Bytes"));
   m_byteCurve->setPen(QPen(Qt::blue));
   m_byteCurve->setCurveType(m_byteCurve->Yfx);
   m_byteCurve->setYAxis(QwtPlot::yRight);
   setPlotType(controls->plotTypeCombo->currentText());
   setFileSymbolType(controls->fileSymbolTypeCombo->currentIndex());
   setByteSymbolType(controls->byteSymbolTypeCombo->currentIndex());

   m_fileCurve->attach(m_jobPlot);
   m_byteCurve->attach(m_jobPlot);

   // attach data
   int size = m_pjd.count();
   int j = 0;
#if defined(__GNU_C)
   double tval[size];
   double fval[size];
   double bval[size];
#else
   double *tval;
   double *fval;
   double *bval;

   tval = (double *)malloc(size * sizeof(double));
   fval = (double *)malloc(size * sizeof(double));
   bval = (double *)malloc(size * sizeof(double));
#endif

   foreach (PlotJobData* plotJobData, m_pjd) {
//      printf("%.0f %.0f %s\n", plotJobData->bytes, plotJobData->files,
//              plotJobData->dt.toString(mainWin->m_dtformat).toUtf8().data());
      fval[j] = plotJobData->files;
      bval[j] = plotJobData->bytes / 1024;
      tval[j] = plotJobData->dt.toTime_t();
//      printf("%i %.0f %.0f %.0f\n", j, tval[j], fval[j], bval[j]);
      j++;
   }
   m_fileCurve->setData(tval,fval,size);
   m_byteCurve->setData(tval,bval,size);

   for (int year=2000; year<2010; year++) {
      for (int month=1; month<=12; month++) {
         QString monthBegin;
         if (month > 9) {
            QTextStream(&monthBegin) << year << "-" << month << "-01 00:00:00";
         } else {
            QTextStream(&monthBegin) << year << "-0" << month << "-01 00:00:00";
         }
         QDateTime mdt = QDateTime::fromString(monthBegin, mainWin->m_dtformat);
         double monbeg = mdt.toTime_t();
   
         //  ...a vertical line at the first of each month
         QwtPlotMarker *mX = new QwtPlotMarker();
         mX->setLabel(mdt.toString("MMM-d"));
         mX->setLabelAlignment(Qt::AlignRight|Qt::AlignTop);
         mX->setLineStyle(QwtPlotMarker::VLine);
         QPen pen(Qt::darkGray);
         pen.setStyle(Qt::DashDotDotLine);
         mX->setLinePen(pen);
         mX->setXValue(monbeg);
         mX->attach(m_jobPlot);
      }
   }

#if !defined(__GNU_C)
   free(tval);
   free(fval);
   free(bval);
#endif
}

/*
 * slot to respond to the plot type combo changing
 */
void JobPlot::setPlotType(QString currentText)
{
   QwtPlotCurve::CurveStyle style = QwtPlotCurve::NoCurve;
   if (currentText == tr("Fitted")) {
      style = QwtPlotCurve::Lines;
      m_fileCurve->setCurveAttribute(QwtPlotCurve::Fitted);
      m_byteCurve->setCurveAttribute(QwtPlotCurve::Fitted);
   } else if (currentText == tr("Sticks")) {
      style = QwtPlotCurve::Sticks;
   } else if (currentText == tr("Lines")) {
      style = QwtPlotCurve::Lines;
      m_fileCurve->setCurveAttribute(QwtPlotCurve::Fitted);
      m_byteCurve->setCurveAttribute(QwtPlotCurve::Fitted);
   } else if (currentText == tr("Steps")) {
      style = QwtPlotCurve::Steps;
   } else if (currentText == tr("None")) {
      style = QwtPlotCurve::NoCurve;
   }
   m_fileCurve->setStyle(style);
   m_byteCurve->setStyle(style);
   m_jobPlot->replot();
}

void JobPlot::fillSymbolCombo(QComboBox *q)
{
  q->addItem( tr("Ellipse"), (int)QwtSymbol::Ellipse);
  q->addItem( tr("Rect"), (int)QwtSymbol::Rect); 
  q->addItem( tr("Diamond"), (int)QwtSymbol::Diamond);
  q->addItem( tr("Triangle"), (int)QwtSymbol::Triangle);
  q->addItem( tr("DTrianle"), (int)QwtSymbol::DTriangle);
  q->addItem( tr("UTriangle"), (int)QwtSymbol::UTriangle);
  q->addItem( tr("LTriangle"), (int)QwtSymbol::LTriangle);
  q->addItem( tr("RTriangle"), (int)QwtSymbol::RTriangle);
  q->addItem( tr("Cross"), (int)QwtSymbol::Cross);
  q->addItem( tr("XCross"), (int)QwtSymbol::XCross);
  q->addItem( tr("HLine"), (int)QwtSymbol::HLine);
  q->addItem( tr("Vline"), (int)QwtSymbol::VLine);
  q->addItem( tr("Star1"), (int)QwtSymbol::Star1);
  q->addItem( tr("Star2"), (int)QwtSymbol::Star2);
  q->addItem( tr("Hexagon"), (int)QwtSymbol::Hexagon); 
  q->addItem( tr("None"), (int)QwtSymbol::NoSymbol);
}


/*
 * slot to respond to the symbol type combo changing
 */
void JobPlot::setFileSymbolType(int index)
{
   setSymbolType(index, 0);
}

void JobPlot::setByteSymbolType(int index)
{
   setSymbolType(index, 1);
}
void JobPlot::setSymbolType(int index, int type)
{
   QwtSymbol sym;
   sym.setPen(QColor(Qt::black));
   sym.setSize(7);

   QVariant style;
   if (0 == type) {
      style = controls->fileSymbolTypeCombo->itemData(index);
      sym.setStyle( (QwtSymbol::Style)style.toInt() );
      sym.setBrush(QColor(Qt::yellow));
      m_fileCurve->setSymbol(sym);
   
   } else {
      style = controls->byteSymbolTypeCombo->itemData(index);
      sym.setStyle( (QwtSymbol::Style)style.toInt() );
      sym.setBrush(QColor(Qt::blue));
      m_byteCurve->setSymbol(sym);

   }
   m_jobPlot->replot();
}

/*
 * slot to respond to the file check box changing state
 */
void JobPlot::fileCheckChanged(int newstate)
{
   if (newstate == Qt::Unchecked) {
      m_fileCurve->detach();
      m_jobPlot->enableAxis(QwtPlot::yLeft, false);
   } else {
      m_fileCurve->attach(m_jobPlot);
      m_jobPlot->enableAxis(QwtPlot::yLeft);
   }
   m_jobPlot->replot();
}

/*
 * slot to respond to the byte check box changing state
 */
void JobPlot::byteCheckChanged(int newstate)
{
   if (newstate == Qt::Unchecked) {
      m_byteCurve->detach();
      m_jobPlot->enableAxis(QwtPlot::yRight, false);
   } else {
      m_byteCurve->attach(m_jobPlot);
      m_jobPlot->enableAxis(QwtPlot::yRight);
   }
   m_jobPlot->replot();
}

/*
 * Save user settings associated with this page
 */
void JobPlot::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("JobPlot");
   settings.setValue("m_splitterSizes", m_splitter->saveState());
   settings.setValue("fileSymbolTypeCombo", controls->fileSymbolTypeCombo->currentText());
   settings.setValue("byteSymbolTypeCombo", controls->byteSymbolTypeCombo->currentText());
   settings.setValue("plotTypeCombo", controls->plotTypeCombo->currentText());
   settings.endGroup();
}

/* 
 * Read settings values for Controls
 */
void JobPlot::readControlSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("JobPlot");
   int fileSymbolTypeIndex = controls->fileSymbolTypeCombo->findText(settings.value("fileSymbolTypeCombo").toString(), Qt::MatchExactly);
   if (fileSymbolTypeIndex == -1) fileSymbolTypeIndex = 2;
      controls->fileSymbolTypeCombo->setCurrentIndex(fileSymbolTypeIndex);
   int byteSymbolTypeIndex = controls->byteSymbolTypeCombo->findText(settings.value("byteSymbolTypeCombo").toString(), Qt::MatchExactly);
   if (byteSymbolTypeIndex == -1) byteSymbolTypeIndex = 3;
      controls->byteSymbolTypeCombo->setCurrentIndex(byteSymbolTypeIndex);
   int plotTypeIndex = controls->plotTypeCombo->findText(settings.value("plotTypeCombo").toString(), Qt::MatchExactly);
   if (plotTypeIndex == -1) plotTypeIndex = 2;
      controls->plotTypeCombo->setCurrentIndex(plotTypeIndex);
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void JobPlot::readSplitterSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("JobPlot");
   if (settings.contains("m_splitterSizes")) { 
      m_splitter->restoreState(settings.value("m_splitterSizes").toByteArray());
   }
   settings.endGroup();
}

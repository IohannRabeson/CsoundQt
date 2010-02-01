/*
    Copyright (C) 2008, 2009 Andres Cabrera
    mantaraya36@gmail.com

    This file is part of QuteCsound.

    QuteCsound is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    QuteCsound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "qutecsound.h"
#include "console.h"
#include "dockhelp.h"
#include "widgetpanel.h"
#include "inspector.h"
#include "opentryparser.h"
#include "options.h"
#include "highlighter.h"
#include "configdialog.h"
#include "configlists.h"
#include "documentpage.h"
#include "utilitiesdialog.h"
#include "graphicwindow.h"
#include "keyboardshortcuts.h"
#include "liveeventframe.h"
//#include "eventsheet.h"

// One day remove these from here for nicer abstraction....
#include "csoundengine.h"
#include "documentview.h"
#include "widgetlayout.h"

// Structs for csound graphs
#include <cwindow.h>
#include "curve.h"

#ifdef Q_OS_WIN32
static const QString SCRIPT_NAME = "qutecsound_run_script-XXXXXX.bat";
#else
static const QString SCRIPT_NAME = "qutecsound_run_script-XXXXXX.sh";
#endif

//csound performance thread function prototype
uintptr_t csThread(void *clientData);

//TODO why does qutecsound not end when it receives a terminate signal?
qutecsound::qutecsound(QStringList fileNames)
{
  setWindowTitle("QuteCsound[*]");
  resize(660,350);
  setWindowIcon(QIcon(":/images/qtcs.png"));
  textEdit = NULL;
  QLocale::setDefault(QLocale::system());  //Does this take care of the decimal separator for different locales?
  curPage = -1;

  m_options = new Options();
  // Create GUI panels
  lineNumberLabel = new QLabel("Line 1"); // Line number display
  statusBar()->addPermanentWidget(lineNumberLabel); // This must be done before a file is loaded
  m_console = new DockConsole(this);
  m_console->setObjectName("m_console");
//   m_console->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
  addDockWidget(Qt::BottomDockWidgetArea, m_console);
  helpPanel = new DockHelp(this);
  helpPanel->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea);
  helpPanel->setObjectName("helpPanel");
  helpPanel->show();
  connect(helpPanel, SIGNAL(openManualExample(QString)), this, SLOT(openManualExample(QString)));
  addDockWidget(Qt::RightDockWidgetArea, helpPanel);

  widgetPanel = new WidgetPanel(this);
  widgetPanel->setFocusPolicy(Qt::NoFocus);
  widgetPanel->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea |Qt::LeftDockWidgetArea);
  widgetPanel->setObjectName("widgetPanel");
  addDockWidget(Qt::RightDockWidgetArea, widgetPanel);
  utilitiesDialog = new UtilitiesDialog(this, m_options/*, _configlists*/);
  connect(utilitiesDialog, SIGNAL(runUtility(QString)), this, SLOT(runUtility(QString)));
//   connect(widgetPanel,SIGNAL(topLevelChanged(bool)), this, SLOT(widgetDockStateChanged(bool)));
//   connect(widgetPanel,SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
//           this, SLOT(widgetDockLocationChanged(Qt::DockWidgetArea)));

  m_inspector = new Inspector(this);
  m_inspector->parseText(QString());
  m_inspector->setObjectName("Inspector");
  addDockWidget(Qt::LeftDockWidgetArea, m_inspector);

  createActions(); // Must be before readSettings as this sets the default shortcuts, and after widgetPanel
  readSettings();

  bool widgetsVisible = !widgetPanel->isHidden(); // Must be after readSettings() to save last state
  if (widgetsVisible)
    widgetPanel->hide();  // Hide until QuteCsound has finished loading

  createMenus();
  createToolBars();
  createStatusBar();

  documentTabs = new QTabWidget (this);
  connect(documentTabs, SIGNAL(currentChanged(int)), this, SLOT(changePage(int)));
  setCentralWidget(documentTabs);
  closeTabButton = new QToolButton(documentTabs);
  closeTabButton->setDefaultAction(closeTabAct);
  documentTabs->setCornerWidget(closeTabButton);
  modIcon.addFile(":/images/modIcon2.png", QSize(), QIcon::Normal);
  modIcon.addFile(":/images/modIcon.png", QSize(), QIcon::Disabled);

  fillFileMenu(); //Must be placed after readSettings to include recent Files
  if (m_options->opcodexmldir == "") {
    opcodeTree = new OpEntryParser(":/opcodes.xml");
  }
  else
    opcodeTree = new OpEntryParser(QString(m_options->opcodexmldir + "/opcodes.xml"));

  // Open files saved from last session
  if (!lastFiles.isEmpty()) {
    foreach (QString lastFile, lastFiles) {
      if (lastFile!="" and !lastFile.startsWith("untitled")) {
        loadFile(lastFile);
      }
    }
    if (lastTabIndex < documentPages.size()) {
      if (documentTabs->currentIndex() != lastTabIndex)
        documentTabs->setCurrentIndex(lastTabIndex);
      else
        this->changePage(lastTabIndex); // To make sure actions like show live score are executed
    }
  }
  // Open files passed in the command line. Only valid for non OS X platforms
  foreach (QString fileName, fileNames) {
    if (fileName!="") {
      loadFile(fileName, true);
    }
  }
  if (widgetsVisible) { // Reshow widget panel if necessary
    widgetPanel->show();
  }
  showWidgetsAct->setChecked(widgetsVisible);  // Button will initialize to current state of panel
  showConsoleAct->setChecked(!m_console->isHidden());  // Button will initialize to current state of panel
  showHelpAct->setChecked(!helpPanel->isHidden());  // Button will initialize to current state of panel
  showInspectorAct->setChecked(!m_inspector->isHidden());  // Button will initialize to current state of panel

  if (documentPages.size() == 0) { // No files yet open. Open default
    newFile();
  }

  changeFont();

  helpPanel->docDir = m_options->csdocdir;
  QString index = m_options->csdocdir + QString("/index.html");
  helpPanel->loadFile(index);

  applySettings();
  createQuickRefPdf();
}

qutecsound::~qutecsound()
{
  qDebug() << "qutecsound::~qutecsound()";
  // This function is not called... see closeEvent()
}

void qutecsound::devicesMessageCallback(CSOUND *csound,
                                         int /*attr*/,
                                         const char *fmt,
                                         va_list args)
{
  QStringList *messages = (QStringList *) csoundGetHostData(csound);
  QString msg = msg.vsprintf(fmt, args);
  messages->append(msg);
}

void qutecsound::utilitiesMessageCallback(CSOUND *csound,
                                          int /*attr*/,
                                          const char *fmt,
                                          va_list args)
{
  DockConsole *console = (DockConsole *) csoundGetHostData(csound);
  QString msg = msg.vsprintf(fmt, args);
  console->text->appendMessage(msg);
  console->text->scrollToEnd();
}


void qutecsound::changeFont()
{
  for (int i = 0; i < documentPages.size(); i++) {
    documentPages[i]->setTextFont(QFont(m_options->font,
                                    (int) m_options->fontPointSize));
  }
  m_console->text->setDefaultFont(QFont(m_options->consoleFont,
                                        (int) m_options->consoleFontPointSize));
  m_console->text->setColors(m_options->consoleFontColor,
                             m_options->consoleBgColor);
//   widgetPanel->setConsoleFont()
}

void qutecsound::changePage(int index)
{
  qDebug() << "qutecsound::changePage " << curPage << "--" << index << "-" << documentPages.size();
  if (curPage >= 0 && curPage < documentPages.size() && documentPages[curPage] != NULL) {
    documentPages[curPage]->showLiveEventFrames(false);
//    documentPages[curPage]->setMacWidgetsText
//        (widgetPanel->widgetsText()); //Updated changes to widgets in file
    disconnect(showLiveEventsAct, SIGNAL(toggled(bool)),
               documentPages[curPage], SLOT(showLiveEventFrames(bool)));
  }
  if (curPage >= 0 && curPage < documentPages.size() && documentPages[curPage] != NULL && index != curPage) {
    documentPages[curPage]->setWidgetLayout(widgetPanel->takeWidgetLayout());  // widget is destroyed by widget panel if it is still there when setting a new one, so we need to take it
  }
  if (index < 0) {
    qDebug() << "qutecsound::changePage index < 0";
    return;
  }
  curPage = index;

  // FIXME connect showLineNumber(int lineNumber) from current document view
  setCurrentFile(documentPages[curPage]->fileName);
  connectActions();
  documentPages[curPage]->setTabStopWidth(m_options->tabWidth);
  documentPages[curPage]->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  documentPages[curPage]->showLiveEventFrames(showLiveEventsAct->isChecked());
  setWidgetPanelGeometry();
  widgetPanel->setWidgetLayout(documentPages[curPage]->getWidgetLayout());
  textEdit = documentPages[curPage];
}

void qutecsound::setWidgetTooltipsVisible(bool visible)
{
  documentPages[curPage]->showWidgetTooltips(visible);
}

//void qutecsound::updateWidgets()
//{
//  documentPages[curPage]->loadWidgets(documentPages[curPage]->getMacWidgetsText());
//  documentPages[curPage]->widgetLayout()->markHistory();
//  setWidgetTooltipsVisible(m_options->showTooltips);
//}

void qutecsound::openExample()
{
  QObject *sender = QObject::sender();
  if (sender == 0)
    return;
  QAction *action = static_cast<QAction *>(sender);
  loadFile(action->data().toString());
//   saveAs();
}

//void qutecsound::registerLiveEvent(QWidget *_e)
//{
//  qDebug() << "qutecsound::registerLiveEvent";
//  EventSheet *e = static_cast<LiveEventFrame *>(_e)->getSheet();
//
//  connect(e,SIGNAL(sendEvent(QString)),widgetPanel,SLOT(queueEvent(QString)));
//}

void qutecsound::closeEvent(QCloseEvent *event)
{
  //FIXME need to destroy all document pages here!
  if (maybeSave()) {
    writeSettings();
    delete closeTabButton;
    closeTabButton = 0;
    close();
    //delete quickRefFile;quickRefFile = 0;
    event->accept();
  } else {
    event->ignore();
  }
  // Delete all temporary files.
  foreach (QString tempFile, tempScriptFiles) {
    QDir().remove(tempFile);
  }
}

void qutecsound::newFile()
{
  if (m_options->defaultCsdActive && m_options->defaultCsd.endsWith(".csd",Qt::CaseInsensitive)) {
    loadFile(m_options->defaultCsd);
  }
  else {
    loadFile(":/default.csd");
  }
  documentPages[curPage]->fileName = "";
  setWindowModified(false);
  documentTabs->setTabIcon(curPage, modIcon);
  documentTabs->setTabText(curPage, "default.csd");
//   documentPages[curPage]->setTabStopWidth(m_options->tabWidth);
  connectActions();
}

void qutecsound::open()
{
  QStringList fileNames;
  bool widgetsVisible = widgetPanel->isVisible();
  if (widgetsVisible && widgetPanel->isFloating())
    widgetPanel->hide(); // Necessary for Mac, as widget Panel covers open dialog
  bool helpVisible = helpPanel->isVisible();
  if (helpVisible && helpPanel->isFloating())
    helpPanel->hide(); // Necessary for Mac, as widget Panel covers open dialog
  bool inspectorVisible = m_inspector->isVisible();
  if (inspectorVisible && m_inspector->isFloating())
    m_inspector->hide(); // Necessary for Mac, as widget Panel covers open dialog
  fileNames = QFileDialog::getOpenFileNames(this, tr("Open File"), lastUsedDir ,
                                            tr("Csound Files (*.csd *.orc *.sco);;All Files (*)"));
  if (widgetsVisible)
    widgetPanel->show();
  if (helpVisible)
    helpPanel->show();
  if (inspectorVisible)
    m_inspector->show();
  foreach (QString fileName, fileNames) {
    int index = isOpen(fileName);
    if (index != -1) {
      documentTabs->setCurrentIndex(index);
      changePage(index);
      statusBar()->showMessage(tr("File already open"), 10000);
    }
    else if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName, true);
    }
  }
}

void qutecsound::reload()
{
  if (documentPages[curPage]->isModified()) {
    QString fileName = documentPages[curPage]->fileName;
    documentPages.remove(curPage);
    documentTabs->removeTab(curPage);
    loadFile(fileName);
  }
}

void qutecsound::openRecent()
{
  QString fileName = static_cast<QAction *>(sender())->text();
  openRecent(fileName);
}

void qutecsound::openRecent(QString fileName)
{
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
}

void qutecsound::createCodeGraph()
{
  QString command = m_options->dot + " -V";
#ifdef Q_OS_WIN32
    // add quotes surrounding dot command if it has spaces in it
  if (m_options->dot.contains(" "))
      command.replace(m_options->dot, "\"" + m_options->dot + "\"");
    // replace linux/mac style directory separators with windows style separators
  command.replace("/", "\\");
#endif

  int ret = system(command.toAscii());
  if (ret != 0) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Dot executable not found.\n"
                            "Please install graphviz from\n"
                            "www.graphviz.org"));
    return;
  }
  QString dotText = documentPages[curPage]->getDotText();
  if (dotText.isEmpty()) {
    qDebug() << "Empty dot text.";
    return;
  }
  qDebug() << dotText;
  QTemporaryFile file(QDir::tempPath() + QDir::separator() + "QuteCsound-GraphXXXXXX.dot");
  QTemporaryFile pngFile(QDir::tempPath() + QDir::separator() + "QuteCsound-GraphXXXXXX.png");
  if (!file.open() || !pngFile.open()) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Cannot create temp dot/png file."));
    return;
  }

  QTextStream out(&file);
  out << dotText;
  file.close();
  file.open();

  command = "\"" + m_options->dot + "\"" + " -Tpng -o \"" + pngFile.fileName() + "\" \"" + file.fileName() + "\"";

#ifdef Q_OS_WIN32
    // remove quotes surrounding dot command if it doesn't have spaces in it
  if (!m_options->dot.contains(" "))
      command.replace("\"" + m_options->dot + "\"", m_options->dot);
    // replace linux/mac style directory separators with windows style separators
  command.replace("/", "\\");
  command.prepend("\"");
  command.append("\"");
#endif

//   qDebug() << command;
  ret = system(command.toAscii());
  if (ret != 0) {
    qDebug() << "qutecsound::createCodeGraph() Error running dot";
  }
  m_graphic = new GraphicWindow(this);
  m_graphic->show();
  m_graphic->openPng(pngFile.fileName());
  connect(m_graphic, SIGNAL(destroyed()), this, SLOT(closeGraph()));
}

void qutecsound::closeGraph()
{
  qDebug("qutecsound::closeGraph()");
}

bool qutecsound::save()
{
  if (documentPages[curPage]->fileName.isEmpty() or documentPages[curPage]->fileName.startsWith(":/examples/")) {
    return saveAs();
  }
  else if (documentPages[curPage]->readOnly){
    if (saveAs()) {
      documentPages[curPage]->readOnly = false;
      return true;
    }
    else {
      return false;
    }
  }
  else {
    return saveFile(documentPages[curPage]->fileName);
  }
}

void qutecsound::copy()
{
  if (helpPanel->hasFocus()) {
    helpPanel->copy();
  }
  else if (m_console->widgetHasFocus()) {
    m_console->copy();
  }
  else {
    documentPages[curPage]->copy();
  }
}

void qutecsound::cut()
{
  documentPages[curPage]->cut();
}

void qutecsound::paste()
{
  documentPages[curPage]->paste();
}

void qutecsound::undo()
{
  qDebug() << "qutecsound::undo()";
  documentPages[curPage]->undo();
}

void qutecsound::redo()
{
  documentPages[curPage]->redo();
}

void qutecsound::setWidgetEditMode(bool active)
{
  documentPages[curPage]->setWidgetEditMode(active);
}

void qutecsound::controlD()
{
  qDebug() << "qutecsound::controlD() not implemented";
  // FIXME put back
//  if (documentPages[curPage]->hasFocus()) {
//    documentPages[curPage]->comment();
//  }
//  else
//    widgetPanel->duplicate();
}

void qutecsound::del()
{
  //TODO finish this...
//   FIXME put back, or can this be done directly in the widget layout class when it has focus?
//  if (documentPages[curPage]->hasFocus()) {
//     documentPages[curPage]->comment();
//  }
//  else
//    widgetPanel->deleteSelected();
}

QString qutecsound::getSaveFileName()
{
  bool widgetsVisible = widgetPanel->isVisible();
  if (widgetsVisible && widgetPanel->isFloating())
    widgetPanel->hide(); // Necessary for Mac, as widget Panel covers open dialog
  bool helpVisible = helpPanel->isVisible();
  if (helpVisible && helpPanel->isFloating())
    helpPanel->hide(); // Necessary for Mac, as widget Panel covers open dialog
  bool inspectorVisible = m_inspector->isVisible();
  if (inspectorVisible && m_inspector->isFloating())
    m_inspector->hide(); // Necessary for Mac, as widget Panel covers open dialog
  QString dir = lastUsedDir;
  dir += documentPages[curPage]->fileName.mid(documentPages[curPage]->fileName.lastIndexOf("/") + 1);
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save File As"),
                                                  dir,
                                                  tr("Csound Files (*.csd *.orc *.sco* *.CSD *.ORC *.SCO)"));
  if (widgetsVisible)
    widgetPanel->show(); // Necessary for Mac, as widget Panel covers open dialog
  if (helpVisible)
    helpPanel->show(); // Necessary for Mac, as widget Panel covers open dialog
  if (inspectorVisible)
    m_inspector->show(); // Necessary for Mac, as widget Panel covers open dialog
  if (fileName.isEmpty())
    return false;
  if (isOpen(fileName) != -1 && isOpen(fileName) != curPage) {
    QMessageBox::critical(this, tr("QuteCsound"),
                          tr("The file is already open in another tab.\nFile not saved!"),
                             QMessageBox::Ok | QMessageBox::Default);
    return false;
  }
//  if (!fileName.endsWith(".csd",Qt::CaseInsensitive) && !fileName.endsWith(".orc",Qt::CaseInsensitive)
//    && !fileName.endsWith(".sco",Qt::CaseInsensitive) && !fileName.endsWith(".txt",Qt::CaseInsensitive)
//    && !fileName.endsWith(".udo",Qt::CaseInsensitive))
//    fileName += ".csd";
  if (!fileName.contains("."))
    fileName += ".csd";
  return fileName;
}

void qutecsound::createQuickRefPdf()
{
  QString tempFileName(QDir::tempPath() + QDir::separator() + "QuteCsound Quick Reference.pdf");
  if (!QFile::exists(tempFileName))
  {
      quickRefFileName = ":/doc/QuteCsound Quick Reference (0.4)-";
      quickRefFileName += _configlists.languageCodes[m_options->language];
      quickRefFileName += ".pdf";
      if (!QFile::exists(quickRefFileName)) {
        quickRefFileName = ":/doc/QuteCsound Quick Reference (0.4).pdf";
      }
      qDebug() << " Opening " << quickRefFileName;
      QFile file(quickRefFileName);
      file.open(QIODevice::ReadOnly);
      QFile quickRefFile(tempFileName);
      quickRefFile.open(QFile::WriteOnly);
      QDataStream quickRefIn(&quickRefFile);
      quickRefIn << file.readAll();
  }
  quickRefFileName = tempFileName;
}

bool qutecsound::saveAs()
{
  QString fileName = getSaveFileName();
  if (fileName != "")
    return saveFile(fileName);
  else
    return false;
}

bool qutecsound::saveNoWidgets()
{
  QString fileName = getSaveFileName();
  if (fileName != "")
    return saveFile(fileName, false);
  else
    return false;
}

bool qutecsound::closeTab()
{
//   qDebug("qutecsound::closeTab() curPage = %i documentPages.size()=%i", curPage, documentPages.size());
  if (documentPages[curPage]->isModified()) {
    int ret = QMessageBox::warning(this, tr("QuteCsound"),
                                   tr("File has been modified.\nDo you want to save it?"),
                                      QMessageBox::Yes | QMessageBox::Default,
                                      QMessageBox::No,
                                      QMessageBox::Cancel);
    if (ret == QMessageBox::Cancel)
      return false;
    else if (ret == QMessageBox::Yes) {
      if (!save())
        return false;
    }
  }
  if (documentPages.size() <= 1) {
    if (QMessageBox::warning(this, tr("QuteCsound"),
        tr("Do you want to exit QuteCsound?"),
           QMessageBox::Yes | QMessageBox::Default,
           QMessageBox::No) == QMessageBox::Yes)
    {
      close();
      return false;
    }
    else {
      newFile();
      curPage = 0;
    }
  }
  documentPages[curPage]->deleteLater();
  documentPages.remove(curPage);
  documentTabs->removeTab(curPage);
  changePage(curPage);
  return true;
}

void qutecsound::print()
{
  QPrinter printer;
  QPrintDialog *dialog = new QPrintDialog(&printer, this);
  dialog->setWindowTitle(tr("Print Document"));
//   if (editor->textCursor().hasSelection())
//     dialog->addEnabledOption(QAbstractPrintDialog::PrintSelection);
  if (dialog->exec() != QDialog::Accepted)
    return;
  documentPages[curPage]->print(&printer);
}
//
//void qutecsound::findReplace()
//{
//  FindReplace *dialog = new FindReplace(this,
//                                        documentPages[curPage],
//                                        &lastSearch,
//                                        &lastReplace,
//                                        &lastCaseSensitive);
//  // lastSearch and lastReplace are passed by reference so they are
//  // updated by FindReplace dialog
//  connect(dialog, SIGNAL(findString(QString)), this, SLOT(findString(QString)));
//  dialog->show();
//}

void qutecsound::join()
{
  QDialog dialog(this);
  dialog.resize(700, 350);
  dialog.setModal(true);
  QPushButton *okButton = new QPushButton(tr("Ok"));
  QPushButton *cancelButton = new QPushButton(tr("Cancel"));

  connect(okButton, SIGNAL(released()), &dialog, SLOT(accept()));
  connect(cancelButton, SIGNAL(released()), &dialog, SLOT(reject()));

  QGridLayout *layout = new QGridLayout(&dialog);
  QListWidget *list1 = new QListWidget(&dialog);
  QListWidget *list2 = new QListWidget(&dialog);
  layout->addWidget(list1, 0, 0);
  layout->addWidget(list2, 0, 1);
  layout->addWidget(okButton, 1,0);
  layout->addWidget(cancelButton, 1,1);
//   layout->resize(400, 200);

  for (int i = 0; i < documentPages.size(); i++) {
    QString name = documentPages[i]->fileName;
    if (documentPages[i]->fileName.endsWith(".orc"))
      list1->addItem(documentPages[i]->fileName);
    else if (documentPages[i]->fileName.endsWith(".sco"))
      list2->addItem(documentPages[i]->fileName);
  }
  QList<QListWidgetItem *> itemList = list1->findItems(documentPages[curPage]->fileName,
      Qt::MatchExactly);
  if (itemList.size() > 0)
    list1->setCurrentItem(itemList[0]);
  QString name = documentPages[curPage]->fileName;
  QList<QListWidgetItem *> itemList2 = list2->findItems(name.replace(".orc", ".sco"),
      Qt::MatchExactly);
  if (itemList2.size() > 0)
    list2->setCurrentItem(itemList2[0]);
  if (itemList.size() == 0 or itemList.size() == 0) {
    QMessageBox::warning(this, tr("Join"),
                        tr("Please open the orc and sco files in QuteCsound first!"));
    return;
  }
  if (dialog.exec() == QDialog::Accepted) {
    QString orcText = "";
    QString scoText = "";
    for (int i = 0; i < documentPages.size(); i++) {
      QString name = documentPages[i]->fileName;
      if (name == list1->currentItem()->text())
        orcText = documentPages[i]->getFullText();
      else if (name == list2->currentItem()->text())
        scoText = documentPages[i]->getFullText();
    }
    QString text = "<CsoundSynthesizer>\n<CsOptions>\n</CsOptions>\n<CsInstruments>\n";
    text += orcText;
    text += "</CsInstruments>\n<CsScore>\n";
    text += scoText;
    text += "</CsScore>\n</CsoundSynthesizer>\n";
    newFile();
    documentPages[curPage]->setTextString(text, m_options->saveWidgets);
  }
//   else {
//     qDebug("qutecsound::join() : No Action");
//   }
}

void qutecsound::putCsladspaText()
{

  documentPages[curPage]->updateCsLadspaText();
}

void qutecsound::exportCabbage()
{
  //TODO finish this
}

void qutecsound::play()
{
  // TODO make csound pause if it is already running
  runAct->setChecked(true);
  if (documentPages[curPage]->fileName.isEmpty()) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("This file has not been saved\nPlease select name and location."));
    if (!saveAs()) {
      runAct->setChecked(false);
      return;
    }
  }
  else if (documentPages[curPage]->isModified()) {
    if (m_options->saveChanges)
      if (!save()) {
        runAct->setChecked(false);
        return;
      }
  }
  m_options->csdPath = "";
  if (documentPages[curPage]->fileName.contains('/')) {
    //FIXME is it necessary to set the csdPath here?
//    m_options->csdPath =
//        documentPages[curPage]->fileName.left(documentPages[curPage]->fileName.lastIndexOf('/'));
    QDir::setCurrent(documentPages[curPage]
                     ->fileName.left(documentPages[curPage]->fileName.lastIndexOf('/')));
  }
  QString fileName, fileName2;
  fileName = documentPages[curPage]->fileName;
  if (!fileName.endsWith(".csd",Qt::CaseInsensitive)) {
    if (documentPages[curPage]->askForFile)
      getCompanionFileName();
    // FIXME run orc file when sco companion is currently active
//    if (fileName.endsWith(".sco",Qt::CaseInsensitive)) {
//      //Must switch filename order when open file is a sco file
//      fileName2 = fileName;
//      fileName = documentPages[curPage]->companionFile;
//    }
//    else
//      fileName2 = documentPages[curPage]->companionFile;
  }
  // FIXME put back this check for FLTK
//  if (m_options->terminalFLTK) { // if "FLpanel" is found in csd run from terminal
//    if (view()->getBasicText().contains("FLpanel"))
//      useAPI = false;
//  }
  //Set directory of current file
  QString runFileName1, runFileName2;
  QTemporaryFile tempFile, csdFile, csdFile2; // TODO add support for orc/sco pairs
  if (fileName.startsWith(":/examples/")) { // TODO is there a proper check to see if example was modified?
    QString tmpFileName = QDir::tempPath();
    if (!tmpFileName.endsWith("/") and !tmpFileName.endsWith("\\")) {
      tmpFileName += QDir::separator();
    }
    tmpFileName += QString("QuteCsoundExample-XXXXXXXX.csd");
    tempFile.setFileTemplate(tmpFileName);
    if (!tempFile.open()) {
      qDebug() << "Error creating temporary file " << tmpFileName;
      runAct->setChecked(false);
      QMessageBox::critical(this,
                            tr("QuteCsound"),
                            tr("Error creating temporary file."),
                            QMessageBox::Ok);
      return;
    }
    QString csdText = documentPages[curPage]->getBasicText();
    runFileName1 = tempFile.fileName();
    tempFile.write(csdText.toAscii());
    tempFile.flush();
  } /*if (fileName.startsWith(":/examples/"))*/
  else if (!m_options->saveChanges) {
    QString tmpFileName = QDir::tempPath();
    if (!tmpFileName.endsWith("/") and !tmpFileName.endsWith("\\")) {
      tmpFileName += QDir::separator();
    }
    if (fileName.endsWith(".csd",Qt::CaseInsensitive)) {
      tmpFileName += QString("csound-tmpXXXXXXXX.csd");
      csdFile.setFileTemplate(tmpFileName);
      if (!csdFile.open()) {
        qDebug() << "Error creating temporary file " << tmpFileName;
        QMessageBox::critical(this,
                              tr("QuteCsound"),
                              tr("Error creating temporary file."),
                              QMessageBox::Ok);
        return;
      }
      QString csdText = documentPages[curPage]->getBasicText();
      runFileName1 = csdFile.fileName();
      csdFile.write(csdText.toAscii());
      csdFile.flush();
    }
  }
  runFileName2 = documentPages[curPage]->companionFile;
  m_options->fileName1 = runFileName1;
  m_options->fileName2 = runFileName2;

  if (m_options->enableWidgets and m_options->showWidgetsOnRun) {
    showWidgetsAct->setChecked(true);
    if (!documentPages[curPage]->usesFltk()) { // Don't bring up widget panel if there's an FLTK panel
      widgetPanel->setVisible(true);
      documentPages[curPage]->focusWidgets();
    }

  }
//  documentPages[curPage]->setCsoundOptions(*m_options);
  int ret = documentPages[curPage]->play(m_options);
  if (ret == -1) {
    runAct->setChecked(false);
    QMessageBox::critical(this,
                          tr("QuteCsound"),
                          tr("Internal error running Csound."),
                          QMessageBox::Ok);
  }
  else if (ret == -2) { // Error creating temporary file
  }
  else if (ret == -3) { // Csound compilation failed
    runAct->setChecked(false);
    //FIXME show error line numbers
  }
}

void qutecsound::runInTerm()
{
  QString fileName = documentPages[curPage]->fileName;
  QTemporaryFile tempFile(QDir::tempPath() + QDir::separator() + "QuteCsoundExample-XXXXXX.csd");
  tempFile.setAutoRemove(false);
  if (fileName.startsWith(":/examples/")) {
    if (!tempFile.open()) {
      qDebug() << "qutecsound::runCsound() : Error creating temp file";
      runAct->setChecked(false);
      return;
    }
    QString csdText = documentPages[curPage]->getBasicText();
    fileName = tempFile.fileName();
    tempFile.write(csdText.toAscii());
    tempFile.flush();
    if (!tempScriptFiles.contains(fileName))
      tempScriptFiles << fileName;
  }
  // FIXME implement usage of realtime / non - realtime here...
//  QString script = generateScript(m_options->realtime, fileName);
  QString script = generateScript(true, fileName);
  QTemporaryFile scriptFile(QDir::tempPath() + QDir::separator() + SCRIPT_NAME);
  scriptFile.setAutoRemove(false);
  if (!scriptFile.open()) {
      runAct->setChecked(false);
    return;
  }
  QTextStream out(&scriptFile);
  out << script;
  //     file.flush();
  scriptFile.close();
  scriptFile.setPermissions (QFile::ExeOwner| QFile::WriteOwner| QFile::ReadOwner);
  QString scriptFileName = scriptFile.fileName();

  QString options;
#ifdef Q_OS_LINUX
  options = "-e " + scriptFileName;
#endif
#ifdef Q_OS_SOLARIS
  options = "-e " + scriptFileName;
#endif
#ifdef Q_WS_MAC
  options = scriptFileName;
#endif
#ifdef Q_OS_WIN32
  options = scriptFileName;
  qDebug() << "m_options.terminal == " << m_options.terminal;
#endif
  execute(m_options->terminal, options);
  runAct->setChecked(false);
  if (!tempScriptFiles.contains(scriptFileName))
    tempScriptFiles << scriptFileName;
}

void qutecsound::pause()
{
  documentPages[curPage]->pause();
//  if (ud->PERF_STATUS == 1) {
//    perfThread->TogglePause();
//  }
}

void qutecsound::stop()
{
  // Must guarantee that csound has stopped when it returns
   qDebug("qutecsound::stop()");
   // FIXME when to stop one document, when to stop all
  documentPages[curPage]->stop();
//  if (ud->PERF_STATUS == 1) {
//    stopCsound();
//  }
//  if (!m_options->thread) {
//    while (ud->PERF_STATUS == -1) {
//      ;
//      // Wait until performance has stopped
//    }
//  }
//  m_console->scrollToEnd();
//  runAct->setChecked(false);
//  recAct->setChecked(false);
//  if (m_options->enableWidgets and m_options->showWidgetsOnRun) {
//    //widgetPanel->setVisible(false);
//  }
}

void qutecsound::record()
{
  documentPages[curPage]->record(m_options->sampleFormat);
}

// void qutecsound::selectMidiOutDevice(QPoint pos)
// {
//   QList<QPair<QString, QString> > devs = ConfigDialog::getMidiInputDevices();
//   QMenu menu;
//
//   for (int i = 0; i < devs.size(); i++) {
//     QAction *action = menu.addAction(devs[i].first/*, this, SLOT()*/);
//     action->setData(devs[i].second);
//   }
//   menu.exec();
// }
//
// void qutecsound::selectMidiInDevice(QPoint pos)
// {
// }
//
// void qutecsound::selectAudioOutDevice(QPoint pos)
// {
// }
//
// void qutecsound::selectAudioInDevice(QPoint pos)
// {
// }

void qutecsound::render()
{
  if (m_options->fileAskFilename) {
    QString defaultFile;
    if (m_options->fileOutputFilenameActive) {
      defaultFile = m_options->fileOutputFilename;
    }
    else {
      defaultFile = lastFileDir;
    }
    QFileDialog dialog(this,tr("Output Filename"),defaultFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setConfirmOverwrite(false);
    QString filter = QString(_configlists.fileTypeLongNames[m_options->fileFileType] + " Files ("
        + _configlists.fileTypeExtensions[m_options->fileFileType] + ")");
    dialog.setFilter(filter);
    if (dialog.exec() == QDialog::Accepted) {
//       QString extension = _configlists.fileTypeExtensions[m_options->fileFileType].left(_configlists.fileTypeExtensions[m_options->fileFileType].indexOf(";"));
//       // Remove the '*' from the extension
//       extension.remove(0,1);
      m_options->fileOutputFilename = dialog.selectedFiles()[0];
//       if (!m_options->fileOutputFilename.endsWith(extension))
//         m_options->fileOutputFilename += extension;
      if (QFile::exists(m_options->fileOutputFilename)) {
        int ret = QMessageBox::warning(this, tr("QuteCsound"),
                tr("The file %1 \nalready exists.\n"
                  "Do you want to overwrite it?").arg(m_options->fileOutputFilename),
                QMessageBox::Save | QMessageBox::Cancel,
                QMessageBox::Save);
        if (ret == QMessageBox::Cancel)
          return;
      }
      lastFileDir = dialog.directory().path();
    }
    else {
      return;
    }
  }
#ifdef Q_OS_WIN32
  m_options->fileOutputFilename.replace('\\', '/');
#endif
  currentAudioFile = m_options->fileOutputFilename;
  m_options->fileName1 = m_options->fileOutputFilename;
  m_options->fileName2 = "";
  documentPages[curPage]->render(m_options);
}

void qutecsound::openExternalEditor()
{
  QString options = currentAudioFile;
  QString optionsText = documentPages[curPage]->getOptionsText();
  if (options == "") {
    if (!optionsText.contains("-o")) {
      options = "test.wav";
    }
    else {
      //TODO this is not very robust...
      optionsText = optionsText.mid(optionsText.indexOf("-o") + 2);
      optionsText = optionsText.left(optionsText.indexOf(" -")).trimmed();
      if (!optionsText.startsWith("dac"))
        options = optionsText;
    }
  }
  options = "\"" + options + "\"";
  QString waveeditor = "\"" + m_options->waveeditor + "\"";
  execute(m_options->waveeditor, options);
}

void qutecsound::openExternalPlayer()
{
  QString options = currentAudioFile;
  QString optionsText = documentPages[curPage]->getOptionsText();
  if (options == "") {
    if (!optionsText.contains("-o")) {
      options = "test.wav";
    }
    else {
      //TODO this is not very robust...
      optionsText = optionsText.mid(optionsText.indexOf("-o") + 2);
      optionsText = optionsText.left(optionsText.indexOf(" -")).trimmed();
      if (!optionsText.startsWith("dac"))
        options = optionsText;
    }
  }
  options = "\"" + options + "\"";
  QString waveplayer = "\"" + m_options->waveplayer + "\"";
  execute(waveplayer, options);
}

void qutecsound::setHelpEntry()
{
  QString text = documentPages[curPage]->wordUnderCursor();
  if (m_options->csdocdir != "") {
    if (text == "0dbfs")
      text = "Zerodbfs";
    else if (text.contains("CsOptions"))
      text = "CommandUnifile";
    helpPanel->docDir = m_options->csdocdir;
    QString fileName = m_options->csdocdir + "/" + text + ".html";
    if (QFile::exists(fileName)) {
        helpPanel->loadFile(fileName);
    }
    else {
        helpPanel->loadFile(m_options->csdocdir + "/index.html");
    }
    helpPanel->show();
  }
  else {
    QMessageBox::critical(this,
                          tr("Error"),
                          tr("HTML Documentation directory not set!\n"
                             "Please go to Edit->Options->Environment and select directory\n"));
  }
}

void qutecsound::openManualExample(QString fileName)
{
  loadFile(fileName);
}

void qutecsound::openExternalBrowser()
{
  QString text = documentPages[curPage]->wordUnderCursor();
  if (m_options->csdocdir != "") {
    QString file =  m_options->csdocdir + "/" + text + ".html";
    execute(m_options->browser, file);
  }
  else {
    QMessageBox::critical(this,
                          tr("Error"),
                          tr("HTML Documentation directory not set!\n"
                             "Please go to Edit->Options->Environment and select directory\n"));
  }
}

void qutecsound::openQuickRef()
{
#ifndef Q_WS_MAC
  if (!QFile::exists(m_options->pdfviewer)) {
    QMessageBox::critical(this,
                          tr("Error"),
                          tr("PDF viewer not found!\n"
                             "Please go to Edit->Options->Environment and select directory\n"));
  }
#endif
  QString arg = "\"" + quickRefFileName + "\"";
  qDebug() << arg;
  execute(m_options->pdfviewer, arg);
}

void qutecsound::openShortcutDialog()
{
  KeyboardShortcuts dialog(this, m_keyActions);
  connect(&dialog, SIGNAL(restoreDefaultShortcuts()), this, SLOT(setDefaultKeyboardShortcuts()));
  dialog.exec();
}

void qutecsound::utilitiesDialogOpen()
{
  qDebug("qutecsound::utilitiesDialog()");
}

void qutecsound::about()
{
  QString text = tr("by: Andres Cabrera\nReleased under the LGPLv2 or GPLv3\nVersion %1\n").arg(QUTECSOUND_VERSION);
  text += tr("French translation:\nFrancois Pinot\n");
  //Why is the ç character not displayed correctly...?
//   text += tr("French translation:\nFrançois Pinot\n");
  text += tr("German translation:\nJoachim Heintz\n");
  text += tr("Portuguese translation:\nVictor Lazzarini\n");
  text += tr("Italian translation:\nFrancesco\n");
  text += QString("qutecsound.sourceforge.net");
  QMessageBox::about(this, tr("About QuteCsound"), text);
}

void qutecsound::documentWasModified()
{
  // FIXME this should be called every time the current document is modified
//  setWindowModified(documentPages[curPage]->isModified());
  if (documentPages[curPage]->isModified())
    documentTabs->setTabIcon(curPage, modIcon);
}

void qutecsound::configure()
{
  ConfigDialog dialog(this, m_options);
  dialog.setCurrentTab(configureTab);
  if (dialog.exec() == QDialog::Accepted) {
    applySettings();
    configureTab = dialog.currentTab();
  }
}

void qutecsound::applySettings()
{
  for (int i = 0; i < documentPages.size(); i++) {
    documentPages[i]->setColorVariables(m_options->colorVariables);
    documentPages[i]->setTabStopWidth(m_options->tabWidth);
    documentPages[i]->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    documentPages[i]->setRunThreaded(m_options->thread);
    documentPages[i]->useInvalue(m_options->useInvalue);
    documentPages[i]->setWidgetEnabled(m_options->enableWidgets);
    documentPages[i]->showWidgetTooltips(m_options->showTooltips);
    documentPages[i]->setKeyRepeatMode(m_options->keyRepeat);
  }
  widgetPanel->setWidgetScrollBarsActive(m_options->scrollbars);
  Qt::ToolButtonStyle toolButtonStyle = (m_options->iconText?
      Qt::ToolButtonTextUnderIcon: Qt::ToolButtonIconOnly);
  fileToolBar->setToolButtonStyle(toolButtonStyle);
  editToolBar->setToolButtonStyle(toolButtonStyle);
  controlToolBar->setToolButtonStyle(toolButtonStyle);
  configureToolBar->setToolButtonStyle(toolButtonStyle);
  m_console->text->setKeyRepeatMode(m_options->keyRepeat);

  QString currentOptions = (m_options->useAPI ? tr("API") : tr("Console")) + " ";
  if (m_options->useAPI) {
    currentOptions +=  (m_options->thread ? tr("Thread") : tr("NoThread")) + " ";
  }

  // Display a summary of options on the status bar
  currentOptions +=  (m_options->saveWidgets ? tr("SaveWidgets") : tr("DontSaveWidgets")) + " ";
  QString playOptions = " (Audio:" + _configlists.rtAudioNames[m_options->rtAudioModule] + " ";
  playOptions += "MIDI:" +  _configlists.rtMidiNames[m_options->rtMidiModule] + ")";
  playOptions += " (" + (m_options->rtUseOptions? tr("UseQuteCsoundOptions"): tr("DiscardQuteCsoundOptions"));
  playOptions += " " + (m_options->rtOverrideOptions? tr("OverrideCsOptions"): tr("")) + ") ";
  playOptions += currentOptions;
  QString renderOptions = " (" + (m_options->fileUseOptions? tr("UseQuteCsoundOptions"): tr("DiscardQuteCsoundOptions")) + " ";
  renderOptions +=  "" + (m_options->fileOverrideOptions? tr("OverrideCsOptions"): tr("")) + ") ";
  renderOptions += currentOptions;
  runAct->setStatusTip(tr("Play") + playOptions);
  renderAct->setStatusTip(tr("Render to file") + renderOptions);
}

void qutecsound::runUtility(QString flags)
{
  //TODO Run utilities from API using soundRunUtility(CSOUND *, const char *name, int argc, char **argv)
  qDebug("qutecsound::runUtility");
  if (m_options->useAPI) {
#ifdef MACOSX_PRE_SNOW
//Remember menu bar to set it after FLTK grabs it
    menuBarHandle = GetMenuBar();
#endif
    m_console->reset();
    static char *argv[33];
    QString name = "";
    QString fileFlags = flags.mid(flags.indexOf("\""));
    flags.remove(fileFlags);
    QStringList indFlags= flags.split(" ",QString::SkipEmptyParts);
    QStringList files = fileFlags.split("\"", QString::SkipEmptyParts);
    if (indFlags.size() < 2) {
      qDebug("qutecsound::runUtility: Error: empty flags");
      return;
    }
    if (indFlags[0] == "-U") {
      indFlags.removeAt(0);
      name = indFlags[0];
      indFlags.removeAt(0);
    }
    else {
      qDebug("qutecsound::runUtility: Error: unexpected flag!");
      return;
    }
    int index = 0;
    foreach (QString flag, indFlags) {
      argv[index] = (char *) calloc(flag.size()+1, sizeof(char));
      strcpy(argv[index],flag.toStdString().c_str());
      index++;
//       qDebug("%s",flag.toStdString().c_str());
    }
    argv[index] = (char *) calloc(files[0].size()+1, sizeof(char));
    strcpy(argv[index++],files[0].toStdString().c_str());
    argv[index] = (char *) calloc(files[2].size()+1, sizeof(char));
    strcpy(argv[index++],files[2].toStdString().c_str());
    int argc = index;
    CSOUND *csoundU;
    csoundU=csoundCreate(0);
    csoundReset(csoundU);
    //FIXME is the userdata needed here? uncomment if needed, think not
//    csoundSetHostData(csoundU, (void *) ud);
    csoundSetMessageCallback(csoundU, &qutecsound::utilitiesMessageCallback);
    csoundPreCompile(csoundU);
    // Utilities always run in the same thread as QuteCsound
    csoundRunUtility(csoundU, name.toStdString().c_str(), argc, argv);
    csoundCleanup(csoundU);
    csoundDestroy(csoundU);
//     free(argv);
#ifdef MACOSX_PRE_SNOW
// Put menu bar back
    SetMenuBar(menuBarHandle);
#endif
  }
  else {
    QString script;
#ifdef Q_OS_WIN32
    script = "";
    if (m_options->opcodedirActive)
      script += "set OPCODEDIR=" + m_options->opcodedir + "\n";
    // Only OPCODEDIR left here as it must be present before csound initializes
    // The problem is that it won't be passed when using the API...
//     if (m_options->sadirActive)
//       script += "set SADIR=" + m_options->sadir + "\n";
//     if (m_options->ssdirActive)
//       script += "set SSDIR=" + m_options->ssdir + "\n";
//     if (m_options->sfdirActive)
//       script += "set SFDIR=" + m_options->sfdir + "\n";
//     if (m_options->ssdirActive)
//       script += "set INCDIR=" + m_options->incdir + "\n";

    script += "cd " + QFileInfo(documentPages[curPage]->fileName).absolutePath() + "\n";
    script += "csound " + flags + "\n";
#else
    script = "#!/bin/sh\n";
    if (m_options->opcodedirActive)
      script += "export OPCODEDIR=" + m_options->opcodedir + "\n";
    // Only OPCODEDIR left here as it must be present before csound initializes
    // The problem is that it won't be passed when using the API...
//     if (m_options->sadirActive)
//       script += "export SADIR=" + m_options->sadir + "\n";
//     if (m_options->ssdirActive)
//       script += "export SSDIR=" + m_options->ssdir + "\n";
//     if (m_options->sfdirActive)
//       script += "export SFDIR=" + m_options->sfdir + "\n";
//     if (m_options->ssdirActive)
//       script += "export INCDIR=" + m_options->incdir + "\n";

    script += "cd " + QFileInfo(documentPages[curPage]->fileName).absolutePath() + "\n";
#ifdef Q_WS_MAC
    script += "/usr/local/bin/csound " + flags + "\n";
#else
    script += "csound " + flags + "\n";
#endif
    script += "echo \"\nPress return to continue\"\n";
    script += "dummy_var=\"\"\n";
    script += "read dummy_var\n";
    script += "rm $0\n";
#endif
    QFile file(SCRIPT_NAME);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      return;

    QTextStream out(&file);
    out << script;
    file.flush();
    file.close();
    file.setPermissions (QFile::ExeOwner| QFile::WriteOwner| QFile::ReadOwner);

    QString options;
#ifdef Q_OS_LINUX
    options = "-e " + SCRIPT_NAME;
#endif
#ifdef Q_OS_SOLARIS
    options = "-e " + SCRIPT_NAME;
#endif
#ifdef Q_WS_MAC
    options = SCRIPT_NAME;
#endif
#ifdef Q_OS_WIN32
    options = SCRIPT_NAME;
#endif
    execute(m_options->terminal, options);
  }
}

// void qutecsound::widgetDockStateChanged(bool topLevel)
// {
// //   qDebug("qutecsound::widgetDockStateChanged()");
//   if (documentPages.size() < 1)
//     return; //necessary check, since widget panel is created early by consructor
//   qApp->processEvents();
//   if (topLevel) {
// //     widgetPanel->setGeometry(documentPages[curPage]->getWidgetPanelGeometry());
//     QRect geometry = documentPages[curPage]->getWidgetPanelGeometry();
//     widgetPanel->move(geometry.x(), geometry.y());
//     widgetPanel->widget()->resize(geometry.width(), geometry.height());
//     qDebug(" %i %i %i %i",geometry.x(), geometry.y(), geometry.width(), geometry.height());
//   }
// }
//
// void qutecsound::widgetDockLocationChanged(Qt::DockWidgetArea area)
// {
//   qDebug("qutecsound::widgetDockLocationChanged() %i", area);
// }

void qutecsound::showLineNumber(int lineNumber)
{
  // FIXME connect this from current document view
  lineNumberLabel->setText(tr("Line %1").arg(lineNumber));
}

void qutecsound::updateInspector()
{
  // This slot is triggered when a tab is closed, so you need to check
  // if curPage is valid.
//  if (curPage < documentPages.size())
//    m_inspector->parseText(documentPages[curPage]->toPlainText());
}

void qutecsound::setDefaultKeyboardShortcuts()
{
//   m_keyActions.append(createCodeGraphAct);
  newAct->setShortcut(tr("Ctrl+N"));
  openAct->setShortcut(tr("Ctrl+O"));
  reloadAct->setShortcut(tr(""));
  saveAct->setShortcut(tr("Ctrl+S"));
  saveAsAct->setShortcut(tr("Shift+Ctrl+S"));
  closeTabAct->setShortcut(tr("Ctrl+W"));

  printAct->setShortcut(tr("Ctrl+P"));
  exitAct->setShortcut(tr("Ctrl+Q"));

  undoAct->setShortcut(tr("Ctrl+Z"));
  redoAct->setShortcut(tr("Shift+Ctrl+Z"));

  cutAct->setShortcut(tr("Ctrl+X"));
  copyAct->setShortcut(tr("Ctrl+C"));
  pasteAct->setShortcut(tr("Ctrl+V"));
  joinAct->setShortcut(tr(""));
  inToGetAct->setShortcut(tr(""));
  getToInAct->setShortcut(tr(""));
  csladspaAct->setShortcut(tr(""));
  findAct->setShortcut(tr("Ctrl+F"));
  findAgainAct->setShortcut(tr("Ctrl+G"));
  autoCompleteAct->setShortcut(tr("Alt+C"));
  configureAct->setShortcut(tr(""));
  editAct->setShortcut(tr("CTRL+E"));
  runAct->setShortcut(tr("CTRL+R"));
  runTermAct->setShortcut(tr(""));
  stopAct->setShortcut(tr("Alt+S"));
  recAct->setShortcut(tr("Ctrl+Space"));
  renderAct->setShortcut(tr("Alt+F"));
  externalPlayerAct->setShortcut(tr(""));
  externalEditorAct->setShortcut(tr(""));
  showWidgetsAct->setShortcut(tr("Alt+1"));
  showHelpAct->setShortcut(tr("Alt+2"));
  showGenAct->setShortcut(tr(""));
  showOverviewAct->setShortcut(tr(""));
  showConsoleAct->setShortcut(tr("Alt+3"));
  showUtilitiesAct->setShortcut(tr("Alt+4"));
  createCodeGraphAct->setShortcut(tr("Alt+5"));
  showInspectorAct->setShortcut(tr("Alt+6"));
  setHelpEntryAct->setShortcut(tr("Shift+F1"));
  browseBackAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Left));
  browseForwardAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Right));
  externalBrowserAct->setShortcut(tr("Shift+Alt+F1"));
  openQuickRefAct->setShortcut(tr(""));
  commentAct->setShortcut(tr("Ctrl+D"));
  uncommentAct->setShortcut(tr("Shift+Ctrl+D"));
  indentAct->setShortcut(tr("Ctrl+I"));
  unindentAct->setShortcut(tr("Shift+Ctrl+I"));
}

void qutecsound::createActions()
{
  // Actions that are not connected here depend on the active document, so they are
  // connected with connectActions() and are changed when the document changes.
  newAct = new QAction(QIcon(":/images/gtk-new.png"), tr("&New"), this);
  newAct->setStatusTip(tr("Create a new file"));
  newAct->setIconText(tr("New"));
  newAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));

  openAct = new QAction(QIcon(":/images/gnome-folder.png"), tr("&Open..."), this);
  openAct->setStatusTip(tr("Open an existing file"));
  openAct->setIconText(tr("Open"));
  openAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

  reloadAct = new QAction(QIcon(":/images/gtk-reload.png"), tr("Reload"), this);
  reloadAct->setStatusTip(tr("Reload file from disk, discarding changes"));
//   reloadAct->setIconText(tr("Reload"));
  connect(reloadAct, SIGNAL(triggered()), this, SLOT(reload()));

  saveAct = new QAction(QIcon(":/images/gnome-dev-floppy.png"), tr("&Save"), this);
  saveAct->setStatusTip(tr("Save the document to disk"));
  saveAct->setIconText(tr("Save"));
  saveAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

  saveAsAct = new QAction(tr("Save &As..."), this);
  saveAsAct->setStatusTip(tr("Save the document under a new name"));
  saveAsAct->setIconText(tr("Save as"));
  saveAsAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

  saveNoWidgetsAct = new QAction(tr("Export without widgets"), this);
  saveNoWidgetsAct->setStatusTip(tr("Save to new file without including widget sections"));
//   saveNoWidgetsAct->setIconText(tr("Save as"));
  connect(saveNoWidgetsAct, SIGNAL(triggered()), this, SLOT(saveNoWidgets()));

  closeTabAct = new QAction(tr("Close current tab"), this);
  closeTabAct->setStatusTip(tr("Close current tab"));
//   closeTabAct->setIconText(tr("Close"));
  closeTabAct->setIcon(QIcon(":/images/cross.png"));
  closeTabAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(closeTabAct, SIGNAL(triggered()), this, SLOT(closeTab()));

  printAct = new QAction(tr("Print"), this);
  printAct->setStatusTip(tr("Print current document"));
//   printAct->setIconText(tr("Print"));
//   closeTabAct->setIcon(QIcon(":/images/cross.png"));
  connect(printAct, SIGNAL(triggered()), this, SLOT(print()));

  for (int i = 0; i < QUTE_MAX_RECENT_FILES; i++) {
    QAction *newAction = new QAction(this);
    openRecentAct.append(newAction);
    connect(newAction, SIGNAL(triggered()), this, SLOT(openRecent()));
  }

  exitAct = new QAction(tr("E&xit"), this);
  exitAct->setStatusTip(tr("Exit the application"));
//   exitAct->setIconText(tr("Exit"));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

  createCodeGraphAct = new QAction(tr("View Code &Graph"), this);
  createCodeGraphAct->setStatusTip(tr("View Code Graph"));
//   createCodeGraphAct->setIconText("Exit");
  connect(createCodeGraphAct, SIGNAL(triggered()), this, SLOT(createCodeGraph()));

  undoAct = new QAction(QIcon(":/images/gtk-undo.png"), tr("Undo"), this);
  undoAct->setStatusTip(tr("Undo last action"));
  exitAct->setIconText(tr("Undo"));
  connect(undoAct, SIGNAL(triggered()), this, SLOT(undo()));

  redoAct = new QAction(QIcon(":/images/gtk-redo.png"), tr("Redo"), this);
  redoAct->setStatusTip(tr("Redo last action"));
  redoAct->setIconText(tr("Redo"));
  connect(redoAct, SIGNAL(triggered()), this, SLOT(redo()));

  cutAct = new QAction(QIcon(":/images/gtk-cut.png"), tr("Cu&t"), this);
  cutAct->setStatusTip(tr("Cut the current selection's contents to the "
      "clipboard"));
  cutAct->setIconText(tr("Cut"));
  connect(cutAct, SIGNAL(triggered()), this, SLOT(cut()));

  copyAct = new QAction(QIcon(":/images/gtk-copy.png"), tr("&Copy"), this);
  copyAct->setStatusTip(tr("Copy the current selection's contents to the "
      "clipboard"));
  copyAct->setIconText(tr("Copy"));
  connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()));

  pasteAct = new QAction(QIcon(":/images/gtk-paste.png"), tr("&Paste"), this);
  pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
      "selection"));
  pasteAct->setIconText(tr("Paste"));
  connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()));

  joinAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("&Join orc/sco"), this);
  joinAct->setStatusTip(tr("Join orc/sco files in a single csd file"));
//   joinAct->setIconText(tr("Join"));
  connect(joinAct, SIGNAL(triggered()), this, SLOT(join()));

  inToGetAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Invalue->Chnget"), this);
  inToGetAct->setStatusTip(tr("Convert invalue/outvalue to chnget/chnset"));
  connect(inToGetAct, SIGNAL(triggered()), this, SLOT(inToGet()));

  getToInAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Chnget->Invalue"), this);
  getToInAct->setStatusTip(tr("Convert chnget/chnset to invalue/outvalue"));
  connect(getToInAct, SIGNAL(triggered()), this, SLOT(getToIn()));

  csladspaAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Insert/Update CsLADSPA text"), this);
  csladspaAct->setStatusTip(tr("Insert/Update CsLADSPA section to csd file"));
  connect(csladspaAct, SIGNAL(triggered()), this, SLOT(putCsladspaText()));

  findAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("&Find and Replace"), this);
  findAct->setStatusTip(tr("Find and replace strings in file"));
//   findAct->setIconText(tr("Find"));

  findAgainAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Find a&gain"), this);
  findAgainAct->setStatusTip(tr("Find next appearance of string"));
//   findAct->setIconText(tr("Find"));
  connect(findAgainAct, SIGNAL(triggered()), this, SLOT(findString()));

  autoCompleteAct = new QAction(tr("AutoComplete"), this);
  autoCompleteAct->setStatusTip(tr("Autocomplete according to Status bar display"));
//   autoCompleteAct->setIconText(tr("AutoComplete"));
  connect(autoCompleteAct, SIGNAL(triggered()), this, SLOT(autoComplete()));

  configureAct = new QAction(QIcon(":/images/control-center2.png"), tr("Configuration"), this);
  configureAct->setStatusTip(tr("Open configuration dialog"));
  configureAct->setIconText(tr("Configure"));
  connect(configureAct, SIGNAL(triggered()), this, SLOT(configure()));

  editAct = new QAction(/*QIcon(":/images/gtk-media-play-ltr.png"),*/ tr("Widget Edit Mode"), this);
  editAct->setStatusTip(tr("Activate Edit Mode for Widget Panel"));
  //   editAct->setIconText("Play");
  editAct->setCheckable(true);
  connect(editAct, SIGNAL(triggered(bool)), this, SLOT(setWidgetEditMode(bool)));

  runAct = new QAction(QIcon(":/images/gtk-media-play-ltr.png"), tr("Run Csound"), this);
  runAct->setStatusTip(tr("Run current file"));
  runAct->setIconText(tr("Run"));
  runAct->setCheckable(true);
  runAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(runAct, SIGNAL(triggered()), this, SLOT(runInTerm()));

  runTermAct = new QAction(QIcon(":/images/gtk-media-play-ltr2.png"), tr("Run in Terminal"), this);
  runTermAct->setStatusTip(tr("Run in external shell"));
  runTermAct->setIconText(tr("Run in Term"));
  //FIXME does it really run in term now?
  connect(runTermAct, SIGNAL(triggered()), this, SLOT(play()));

  stopAct = new QAction(QIcon(":/images/gtk-media-stop.png"), tr("Stop"), this);
  stopAct->setStatusTip(tr("Stop"));
  stopAct->setIconText(tr("Stop"));
  stopAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(stopAct, SIGNAL(triggered()), this, SLOT(stop()));

  recAct = new QAction(QIcon(":/images/gtk-media-record.png"), tr("Record"), this);
  recAct->setStatusTip(tr("Record"));
  recAct->setIconText(tr("Record"));
  recAct->setCheckable(true);
  recAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(recAct, SIGNAL(triggered()), this, SLOT(record()));

  renderAct = new QAction(QIcon(":/images/render.png"), tr("Render to file"), this);
  renderAct->setStatusTip(tr("Render to file"));
  renderAct->setIconText(tr("Render"));
  connect(renderAct, SIGNAL(triggered()), this, SLOT(render()));

  externalPlayerAct = new QAction(QIcon(":/images/playfile.png"), tr("Play Audiofile"), this);
  externalPlayerAct->setStatusTip(tr("Play rendered audiofile in External Editor"));
  externalPlayerAct->setIconText(tr("Ext. Player"));
  connect(externalPlayerAct, SIGNAL(triggered()), this, SLOT(openExternalPlayer()));

  externalEditorAct = new QAction(QIcon(":/images/editfile.png"), tr("Edit Audiofile"), this);
  externalEditorAct->setStatusTip(tr("Edit rendered audiofile in External Editor"));
  externalEditorAct->setIconText(tr("Ext. Editor"));
  connect(externalEditorAct, SIGNAL(triggered()), this, SLOT(openExternalEditor()));

  showWidgetsAct = new QAction(QIcon(":/images/gnome-mime-application-x-diagram.png"), tr("Widgets"), this);
  showWidgetsAct->setCheckable(true);
  //showWidgetsAct->setChecked(true);
  showWidgetsAct->setStatusTip(tr("Show Realtime Widgets"));
  showWidgetsAct->setIconText(tr("Widgets"));
  showWidgetsAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(showWidgetsAct, SIGNAL(triggered(bool)), widgetPanel, SLOT(setVisible(bool)));
  connect(widgetPanel, SIGNAL(Close(bool)), showWidgetsAct, SLOT(setChecked(bool)));

  showInspectorAct = new QAction(QIcon(":/images/edit-find.png"), tr("Inspector"), this);
  showInspectorAct->setCheckable(true);
  showInspectorAct->setStatusTip(tr("Show Inspector"));
  showInspectorAct->setIconText(tr("Inspector"));
  showInspectorAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(showInspectorAct, SIGNAL(triggered(bool)), m_inspector, SLOT(setVisible(bool)));
  connect(m_inspector, SIGNAL(Close(bool)), showInspectorAct, SLOT(setChecked(bool)));

  showHelpAct = new QAction(QIcon(":/images/gtk-info.png"), tr("Help Panel"), this);
  showHelpAct->setCheckable(true);
  showHelpAct->setChecked(true);
  showHelpAct->setStatusTip(tr("Show the Csound Manual Panel"));
  showHelpAct->setIconText(tr("Manual"));
  connect(showHelpAct, SIGNAL(toggled(bool)), helpPanel, SLOT(setVisible(bool)));
  connect(helpPanel, SIGNAL(Close(bool)), showHelpAct, SLOT(setChecked(bool)));

  showLiveEventsAct = new QAction(QIcon(":/images/note.png"), tr("Live Events"), this);
  showLiveEventsAct->setCheckable(true);
//  showLiveEventsAct->setChecked(true);  // Unnecessary because it is set by options
  showLiveEventsAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  showLiveEventsAct->setStatusTip(tr("Show Live Events Panels"));
  showLiveEventsAct->setIconText(tr("Live Events"));

  showManualAct = new QAction(/*QIcon(":/images/gtk-info.png"), */tr("Csound Manual"), this);
  showManualAct->setStatusTip(tr("Show the Csound manual in the help panel"));
  showManualAct->setShortcutContext (Qt::ApplicationShortcut); // Needed because some key events are not propagation properly
  connect(showManualAct, SIGNAL(triggered()), helpPanel, SLOT(showManual()));

  showGenAct = new QAction(/*QIcon(":/images/gtk-info.png"), */tr("GEN Routines"), this);
  showGenAct->setStatusTip(tr("Show the GEN Routines Manual page"));
  connect(showGenAct, SIGNAL(triggered()), helpPanel, SLOT(showGen()));

  showOverviewAct = new QAction(/*QIcon(":/images/gtk-info.png"), */tr("Opcode Overview"), this);
  showOverviewAct->setStatusTip(tr("Show opcode overview"));
  connect(showOverviewAct, SIGNAL(triggered()), helpPanel, SLOT(showOverview()));

  showConsoleAct = new QAction(QIcon(":/images/gksu-root-terminal.png"), tr("Output Console"), this);
  showConsoleAct->setCheckable(true);
  showConsoleAct->setChecked(true);
  showConsoleAct->setStatusTip(tr("Show Csound's message console"));
  showConsoleAct->setIconText(tr("Console"));
  connect(showConsoleAct, SIGNAL(toggled(bool)), m_console, SLOT(setVisible(bool)));
  connect(m_console, SIGNAL(Close(bool)), showConsoleAct, SLOT(setChecked(bool)));

  setHelpEntryAct = new QAction(QIcon(":/images/gtk-info.png"), tr("Show Opcode Entry"), this);
  setHelpEntryAct->setStatusTip(tr("Show Opcode Entry in help panel"));
  setHelpEntryAct->setIconText(tr("Manual for opcode"));
  connect(setHelpEntryAct, SIGNAL(triggered()), this, SLOT(setHelpEntry()));

  browseBackAct = new QAction(tr("Help Back"), this);
  browseBackAct->setStatusTip(tr("Go back in help page"));
  connect(browseBackAct, SIGNAL(triggered()), helpPanel, SLOT(browseBack()));

  browseForwardAct = new QAction(tr("Help Forward"), this);
  browseForwardAct->setStatusTip(tr("Go forward in help page"));
  connect(browseForwardAct, SIGNAL(triggered()), helpPanel, SLOT(browseForward()));

  externalBrowserAct = new QAction(/*QIcon(":/images/gtk-info.png"), */ tr("Show Opcode Entry in External Browser"), this);
  externalBrowserAct->setStatusTip(tr("Show Opcode Entry in external browser"));
  connect(externalBrowserAct, SIGNAL(triggered()), this, SLOT(openExternalBrowser()));

  openQuickRefAct = new QAction(/*QIcon(":/images/gtk-info.png"), */ tr("Open Quick Reference Guide"), this);
  openQuickRefAct->setStatusTip(tr("Open Quick Reference Guide in PDF viewer"));
  connect(openQuickRefAct, SIGNAL(triggered()), this, SLOT(openQuickRef()));

  showUtilitiesAct = new QAction(QIcon(":/images/gnome-devel.png"), tr("Utilities"), this);
  showUtilitiesAct->setCheckable(true);
  showUtilitiesAct->setChecked(false);
  showUtilitiesAct->setStatusTip(tr("Show the Csound Utilities dialog"));
  showUtilitiesAct->setIconText(tr("Utilities"));
  connect(showUtilitiesAct, SIGNAL(triggered(bool)), utilitiesDialog, SLOT(setVisible(bool)));
  connect(utilitiesDialog, SIGNAL(Close(bool)), showUtilitiesAct, SLOT(setChecked(bool)));

  setShortcutsAct = new QAction(tr("Set Keyboard Shortcuts"), this);
  setShortcutsAct->setStatusTip(tr("Set Keyboard Shortcuts"));
  setShortcutsAct->setIconText(tr("Set Shortcuts"));
  connect(setShortcutsAct, SIGNAL(triggered()), this, SLOT(openShortcutDialog()));

  commentAct = new QAction(tr("Comment"), this);
  commentAct->setStatusTip(tr("Comment selection"));
  commentAct->setIconText(tr("Comment"));
  connect(commentAct, SIGNAL(triggered()), this, SLOT(controlD()));

  uncommentAct = new QAction(tr("Uncomment"), this);
  uncommentAct->setStatusTip(tr("Uncomment selection"));
//   uncommentAct->setIconText(tr("Uncomment"));
//   connect(uncommentAct, SIGNAL(triggered()), this, SLOT(uncomment()));

  indentAct = new QAction(tr("Indent"), this);
  indentAct->setStatusTip(tr("Indent selection"));
//   indentAct->setIconText(tr("Indent"));
//   connect(indentAct, SIGNAL(triggered()), this, SLOT(indent()));

  unindentAct = new QAction(tr("Unindent"), this);
  unindentAct->setStatusTip(tr("Unindent selection"));
//   unindentAct->setIconText(tr("Unindent"));
//   connect(unindentAct, SIGNAL(triggered()), this, SLOT(unindent()));

  aboutAct = new QAction(tr("&About QuteCsound"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));
//   aboutAct->setIconText(tr("About"));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct = new QAction(tr("About &Qt"), this);
  aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
//   aboutQtAct->setIconText(tr("About Qt"));
  connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

  //TODO Put this back when documentpage has focus
//   cutAct->setEnabled(false);
//   copyAct->setEnabled(false);
  setKeyboardShortcutsList();
}

void qutecsound::setKeyboardShortcutsList()
{
  // Do not change the order of these actions because the settings
  // read shortcuts for a number. Only add at the end.
  m_keyActions.append(newAct);
  m_keyActions.append(openAct);
  m_keyActions.append(reloadAct);
  m_keyActions.append(saveAct);
  m_keyActions.append(saveAsAct);
  m_keyActions.append(closeTabAct);
  m_keyActions.append(printAct);
  m_keyActions.append(exitAct);
  m_keyActions.append(createCodeGraphAct);
  m_keyActions.append(undoAct);
  m_keyActions.append(redoAct);
  m_keyActions.append(cutAct);
  m_keyActions.append(copyAct);
  m_keyActions.append(pasteAct);
  m_keyActions.append(joinAct);
  m_keyActions.append(inToGetAct);
  m_keyActions.append(getToInAct);
  m_keyActions.append(csladspaAct);
  m_keyActions.append(findAct);
  m_keyActions.append(autoCompleteAct);
  m_keyActions.append(configureAct);
  m_keyActions.append(editAct);
  m_keyActions.append(runAct);
  m_keyActions.append(runTermAct);
  m_keyActions.append(stopAct);
  m_keyActions.append(recAct);
  m_keyActions.append(renderAct);
  m_keyActions.append(commentAct);
  m_keyActions.append(uncommentAct);
  m_keyActions.append(indentAct);
  m_keyActions.append(unindentAct);
  m_keyActions.append(externalPlayerAct);
  m_keyActions.append(externalEditorAct);
  m_keyActions.append(showWidgetsAct);
  m_keyActions.append(showHelpAct);
  m_keyActions.append(showGenAct);
  m_keyActions.append(showOverviewAct);
  m_keyActions.append(showConsoleAct);
  m_keyActions.append(setHelpEntryAct);
  m_keyActions.append(browseBackAct);
  m_keyActions.append(browseForwardAct);
  m_keyActions.append(externalBrowserAct);
  m_keyActions.append(openQuickRefAct);
  m_keyActions.append(showInspectorAct);
  m_keyActions.append(showLiveEventsAct);
}

void qutecsound::connectActions()
{
  DocumentPage * doc = documentPages[curPage];
//  disconnect(undoAct, 0, 0, 0);
//  connect(undoAct, SIGNAL(triggered()), this, SLOT(undo()));
//  disconnect(redoAct, 0, 0, 0);
//  connect(redoAct, SIGNAL(triggered()), this, SLOT(redo()));
//  disconnect(cutAct, 0, 0, 0);
//  connect(cutAct, SIGNAL(triggered()), this, SLOT(cut()));
//  disconnect(copyAct, 0, 0, 0);
//  connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()));
//  disconnect(pasteAct, 0, 0, 0);
//  connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()));

//  disconnect(findAct, 0, 0, 0);
//  connect(findAct, SIGNAL(triggered()), this, SLOT(findReplace()));

//   disconnect(commentAct, 0, 0, 0);
  disconnect(uncommentAct, 0, 0, 0);
  disconnect(indentAct, 0, 0, 0);
  disconnect(unindentAct, 0, 0, 0);
//   connect(commentAct, SIGNAL(triggered()), doc, SLOT(comment()));
  connect(uncommentAct, SIGNAL(triggered()), doc, SLOT(uncomment()));
  connect(indentAct, SIGNAL(triggered()), doc, SLOT(indent()));
  connect(unindentAct, SIGNAL(triggered()), doc, SLOT(unindent()));

  disconnect(doc, SIGNAL(copyAvailable(bool)), 0, 0);
  disconnect(doc, SIGNAL(copyAvailable(bool)), 0, 0);
  //TODO put these back but only when document has focus
//   connect(doc, SIGNAL(copyAvailable(bool)),
//           cutAct, SLOT(setEnabled(bool)));
//   connect(doc, SIGNAL(copyAvailable(bool)),
//           copyAct, SLOT(setEnabled(bool)));

//  disconnect(doc, SIGNAL(textChanged()), 0, 0);
  disconnect(doc, SIGNAL(cursorPositionChanged()), 0, 0);
//  connect(doc, SIGNAL(textChanged()),
//          this, SLOT(documentWasModified()));
  connect(doc, SIGNAL(selectionChanged()),
          this, SLOT(checkSelection()));

  disconnect(widgetPanel, SIGNAL(widgetsChanged(QString)),0,0);
//   connect(widgetPanel, SIGNAL(widgetsChanged(QString)),
//           doc, SLOT(setMacWidgetsText(QString)) );
  disconnect(widgetPanel, SIGNAL(moved(QPoint)),0,0);
  connect(widgetPanel, SIGNAL(moved(QPoint)),
          doc, SLOT(setWidgetPanelPosition(QPoint)) );
//  disconnect(widgetPanel, SIGNAL(resized(QSize)),0,0);
//  connect(widgetPanel, SIGNAL(resized(QSize)),
//          doc, SLOT(setWidgetPanelSize(QSize)) );
  disconnect(doc, SIGNAL(currentLineChanged(int)), 0, 0);
  connect(doc, SIGNAL(currentLineChanged(int)), this, SLOT(showLineNumber(int)));
  connect(doc, SIGNAL(currentTextUpdated()), this, SLOT(updateInspector()));

  // Connect inspector actions to document
  disconnect(m_inspector, 0, 0, 0);
  connect(m_inspector, SIGNAL(jumpToLine(int)),
          doc, SLOT(jumpToLine(int)));
  connect(showLiveEventsAct, SIGNAL(toggled(bool)), doc, SLOT(showLiveEventFrames(bool)));
  connect(doc, SIGNAL(liveEventsVisible(bool)), showLiveEventsAct, SLOT(setChecked(bool)));

}

void qutecsound::createMenus()
{
  fileMenu = menuBar()->addMenu(tr("File"));
  fileMenu->addAction(newAct);
  fileMenu->addAction(openAct);
  fileMenu->addAction(saveAct);
  fileMenu->addAction(saveAsAct);
  fileMenu->addAction(saveNoWidgetsAct);
  fileMenu->addAction(reloadAct);
//   fileMenu->addAction(cabbageAct);
  fileMenu->addAction(closeTabAct);
  fileMenu->addAction(printAct);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);
  fileMenu->addSeparator();

  recentMenu = fileMenu->addMenu(tr("Recent files"));

  editMenu = menuBar()->addMenu(tr("Edit"));
  editMenu->addAction(undoAct);
  editMenu->addAction(redoAct);
  editMenu->addSeparator();
  editMenu->addAction(cutAct);
  editMenu->addAction(copyAct);
  editMenu->addAction(pasteAct);
  editMenu->addSeparator();
  editMenu->addAction(findAct);
  editMenu->addAction(findAgainAct);
  editMenu->addAction(autoCompleteAct);
  editMenu->addSeparator();
  editMenu->addAction(commentAct);
  editMenu->addAction(uncommentAct);
  editMenu->addAction(indentAct);
  editMenu->addAction(unindentAct);
  editMenu->addSeparator();
  editMenu->addAction(joinAct);
  editMenu->addAction(inToGetAct);
  editMenu->addAction(getToInAct);
  editMenu->addAction(csladspaAct);
  editMenu->addSeparator();
  editMenu->addAction(editAct);
  editMenu->addSeparator();
  editMenu->addAction(configureAct);
  editMenu->addAction(setShortcutsAct);

  controlMenu = menuBar()->addMenu(tr("Control"));
  controlMenu->addAction(runAct);
  controlMenu->addAction(runTermAct);
  controlMenu->addAction(stopAct);
  controlMenu->addAction(runAct);
  controlMenu->addAction(renderAct);
  controlMenu->addAction(recAct);
  controlMenu->addAction(externalEditorAct);
  controlMenu->addAction(externalPlayerAct);

  viewMenu = menuBar()->addMenu(tr("View"));
  viewMenu->addAction(showWidgetsAct);
  viewMenu->addAction(showHelpAct);
  viewMenu->addAction(showConsoleAct);
  viewMenu->addAction(showUtilitiesAct);
  viewMenu->addAction(createCodeGraphAct);
  viewMenu->addAction(showInspectorAct);
  viewMenu->addAction(showLiveEventsAct);

  QStringList tutFiles;
  QStringList basicsFiles;
  QStringList realtimeInteractionFiles;
  QStringList widgetFiles;
  QStringList synthFiles;
  QStringList musicFiles;
  QStringList usefulFiles;
  QStringList exampleFiles;
  QList<QStringList> subMenus;
  QStringList subMenuNames;

  widgetFiles.append(":/examples/Widgets/Widget_Panel.csd");
  widgetFiles.append(":/examples/Widgets/Label_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Display_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Slider_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Scrollnumber_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Graph_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Button_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Checkbox_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Menu_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Controller_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Lineedit_Widget.csd");
  widgetFiles.append(":/examples/Widgets/Scope_Widget.csd");
  widgetFiles.append(":/examples/Widgets/String_Channels.csd");
  widgetFiles.append(":/examples/Widgets/Reserved_Channels.csd");

  subMenus << widgetFiles;
  subMenuNames << "Widgets";

  synthFiles.append(":/examples/Synths/Additive_Synth.csd");
  synthFiles.append(":/examples/Synths/Simple_Subtractive.csd");
  synthFiles.append(":/examples/Synths/Simple_FM_Synth.csd");
  synthFiles.append(":/examples/Synths/Phase_Mod_Synth.csd");
  synthFiles.append(":/examples/Synths/Formant_Synth.csd");
  synthFiles.append(":/examples/Synths/Pipe_Synth.csd");
  subMenus << synthFiles;
  subMenuNames << "Synths";

  musicFiles.append(":/examples/Music/Boulanger-Trapped_in_Convert.csd");
  musicFiles.append(":/examples/Music/Chowning-Stria.csd");
  musicFiles.append(":/examples/Music/Kung-Xanadu.csd");
  musicFiles.append(":/examples/Music/Riley-In_C.csd");
  musicFiles.append(":/examples/Music/Stockhausen-Studie_II.csd");

  subMenus << musicFiles;
  subMenuNames << tr("Music");

  usefulFiles.append(":/examples/Useful/IO_Test.csd");
  usefulFiles.append(":/examples/Useful/Audio_Input_Test.csd");
  usefulFiles.append(":/examples/Useful/Audio_Output_Test.csd");
  usefulFiles.append(":/examples/Useful/Audio_Thru_Test.csd");
  usefulFiles.append(":/examples/Useful/MIDI_IO_Test.csd");
  usefulFiles.append(":/examples/Useful/MIDI_Tunings.csd");
  usefulFiles.append(":/examples/Useful/SF_Play_from_buffer.csd");
  usefulFiles.append(":/examples/Useful/SF_Play_from_buffer_2.csd");
  usefulFiles.append(":/examples/Useful/SF_Play_from_HD.csd");
  usefulFiles.append(":/examples/Useful/SF_Play_from_HD_2.csd");
  usefulFiles.append(":/examples/Useful/8_Chn_Player.csd");
  usefulFiles.append(":/examples/Useful/SF_Record.csd");
  usefulFiles.append(":/examples/Useful/File_to_Text.csd");
  usefulFiles.append(":/examples/Useful/Pitch_Tracker.csd");

  subMenus << usefulFiles;
  subMenuNames << tr("Useful");

  exampleFiles.append(":/examples/Miscellaneous/Keyboard_Control.csd");
  exampleFiles.append(":/examples/Miscellaneous/Just_Intonation.csd");
  exampleFiles.append(":/examples/Miscellaneous/Mouse_Control.csd");
  exampleFiles.append(":/examples/Miscellaneous/Event_Panel.csd");
  exampleFiles.append(":/examples/Miscellaneous/Score_Tricks.csd");
  exampleFiles.append(":/examples/Miscellaneous/Simple_Convolution.csd");
  exampleFiles.append(":/examples/Miscellaneous/Universal_Convolution.csd");
  exampleFiles.append(":/examples/Miscellaneous/Cross_Synthesis.csd");
  exampleFiles.append(":/examples/Miscellaneous/Live_Granular.csd");
  exampleFiles.append(":/examples/Miscellaneous/SF_Granular.csd");
  exampleFiles.append(":/examples/Miscellaneous/Oscillator_Aliasing.csd");
  exampleFiles.append(":/examples/Miscellaneous/Circle.csd");
  exampleFiles.append(":/examples/Miscellaneous/Pvstencil.csd");
  exampleFiles.append(":/examples/Miscellaneous/Rms.csd");
  exampleFiles.append(":/examples/Miscellaneous/Reinit_Example.csd");
  exampleFiles.append(":/examples/Miscellaneous/No_Reinit.csd");
  exampleFiles.append(":/examples/Miscellaneous/Binaural_Panning.csd");
  exampleFiles.append(":/examples/Miscellaneous/Noise_Reduction.csd");

  subMenus << exampleFiles;
  subMenuNames << tr("Miscellaneous");

  QMenu *examplesMenu = menuBar()->addMenu(tr("Examples"));
  QAction *newAction;
  QMenu *submenu;

  basicsFiles.append(":/examples/Getting Started/Basics/Hello World.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Document Structure.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Basic Elements Opcodes.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Basic Elements Variables.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Getting Help.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Instrument Control.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Realtime Instrument Control.csd");
  basicsFiles.append(":/examples/Getting Started/Basics/Routing.csd");

  QMenu *tutorialMenu = examplesMenu->addMenu(tr("Getting Started"));
  submenu = tutorialMenu->addMenu(tr("Basics"));
  foreach (QString fileName, basicsFiles) {
    QString name = fileName.mid(fileName.lastIndexOf("/") + 1).replace("_", " ").remove(".csd");
    newAction = submenu->addAction(name);
    newAction->setData(fileName);
    connect(newAction,SIGNAL(triggered()), this, SLOT(openExample()));
  }

  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/Creating_Widgets.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/Widgets_Invalue.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/Widgets_Outvalue.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/Widgets_Buttontypes.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/Widgets_Checkbox.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/Live_Audio_Input.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/MIDI_Receiving_Notes.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/MIDI_Synth.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/MIDI_Control_Data.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/MIDI_Assign_Controllers.csd");
  realtimeInteractionFiles.append(":/examples/Getting Started/Realtime_Interaction/OpenSoundControl.csd");

   submenu = tutorialMenu->addMenu(tr("Realtime Interaction"));
  foreach (QString fileName, realtimeInteractionFiles) {
    QString name = fileName.mid(fileName.lastIndexOf("/") + 1).replace("_", " ").remove(".csd");
    newAction = submenu->addAction(name);
    newAction->setData(fileName);
    connect(newAction,SIGNAL(triggered()), this, SLOT(openExample()));
  }
  tutFiles.append(":/examples/Getting Started/Toots/Toot1.csd");
  tutFiles.append(":/examples/Getting Started/Toots/Toot2.csd");
  tutFiles.append(":/examples/Getting Started/Toots/Toot3.csd");
  tutFiles.append(":/examples/Getting Started/Toots/Toot4.csd");
  tutFiles.append(":/examples/Getting Started/Toots/Toot5.csd");

  submenu = tutorialMenu->addMenu("Toots");
  foreach (QString fileName, tutFiles) {
    QString name = fileName.mid(fileName.lastIndexOf("/") + 1).replace("_", " ").remove(".csd");
    newAction = submenu->addAction(name);
    newAction->setData(fileName);
    connect(newAction,SIGNAL(triggered()), this, SLOT(openExample()));
  }

  tutFiles.clear();
  tutFiles.append(":/examples/Getting Started/Widgets_1.csd");
  tutFiles.append(":/examples/Getting Started/Widgets_2.csd");
  submenu = tutorialMenu->addMenu("Other");
  foreach (QString fileName, tutFiles) {
    QString name = fileName.mid(fileName.lastIndexOf("/") + 1).replace("_", " ").remove(".csd");
    newAction = submenu->addAction(name);
    newAction->setData(fileName);
    connect(newAction,SIGNAL(triggered()), this, SLOT(openExample()));
  }

  for (int i = 0; i < subMenus.size(); i++) {
    submenu = examplesMenu->addMenu(subMenuNames[i]);
    foreach (QString fileName, subMenus[i]) {
      QString name = fileName.mid(fileName.lastIndexOf("/") + 1).replace("_", " ").remove(".csd");
      newAction = submenu->addAction(name);
      newAction->setData(fileName);
      connect(newAction,SIGNAL(triggered()), this, SLOT(openExample()));
    }
  }

  menuBar()->addSeparator();

  helpMenu = menuBar()->addMenu(tr("Help"));
  helpMenu->addAction(setHelpEntryAct);
  helpMenu->addAction(externalBrowserAct);
  helpMenu->addSeparator();
  helpMenu->addAction(browseBackAct);
  helpMenu->addAction(browseForwardAct);
  helpMenu->addSeparator();
  helpMenu->addAction(showManualAct);
  helpMenu->addAction(showOverviewAct);
  helpMenu->addAction(showGenAct);
  helpMenu->addAction(openQuickRefAct);
  helpMenu->addSeparator();
  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);

}

void qutecsound::fillFileMenu()
{
  recentMenu->clear();
  for (int i = 0; i< recentFiles.size(); i++) {
    if (i < recentFiles.size() && recentFiles[i] != "") {
      openRecentAct[i]->setText(recentFiles[i]);
      recentMenu->addAction(openRecentAct[i]);
    }
  }
}

void qutecsound::createToolBars()
{
  fileToolBar = addToolBar(tr("File"));
  fileToolBar->setObjectName("fileToolBar");
  fileToolBar->addAction(newAct);
  fileToolBar->addAction(openAct);
  fileToolBar->addAction(saveAct);

  editToolBar = addToolBar(tr("Edit"));
  editToolBar->setObjectName("editToolBar");
  editToolBar->addAction(undoAct);
  editToolBar->addAction(redoAct);
  editToolBar->addAction(cutAct);
  editToolBar->addAction(copyAct);
  editToolBar->addAction(pasteAct);

  controlToolBar = addToolBar(tr("Control"));
  controlToolBar->setObjectName("controlToolBar");
  controlToolBar->addAction(runAct);
  controlToolBar->addAction(stopAct);
  controlToolBar->addAction(runTermAct);
  controlToolBar->addAction(recAct);
  controlToolBar->addAction(renderAct);
  controlToolBar->addAction(externalEditorAct);
  controlToolBar->addAction(externalPlayerAct);

  configureToolBar = addToolBar(tr("Configure"));
  configureToolBar->setObjectName("configureToolBar");
  configureToolBar->addAction(configureAct);
  configureToolBar->addAction(showWidgetsAct);
  configureToolBar->addAction(showHelpAct);
  configureToolBar->addAction(showConsoleAct);
  configureToolBar->addAction(showUtilitiesAct);
  configureToolBar->addAction(showInspectorAct);
  configureToolBar->addAction(showLiveEventsAct);

  Qt::ToolButtonStyle toolButtonStyle = (m_options->iconText?
      Qt::ToolButtonTextUnderIcon: Qt::ToolButtonIconOnly);
  fileToolBar->setToolButtonStyle(toolButtonStyle);
  editToolBar->setToolButtonStyle(toolButtonStyle);
  controlToolBar->setToolButtonStyle(toolButtonStyle);
  configureToolBar->setToolButtonStyle(toolButtonStyle);
}

void qutecsound::createStatusBar()
{
  statusBar()->showMessage(tr("Ready"));
}

void qutecsound::readSettings()
{
  QSettings settings("csound", "qutecsound");
  int settingsVersion = settings.value("settingsVersion", 0).toInt();
  settings.beginGroup("GUI");
  QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
  QSize size = settings.value("size", QSize(600, 500)).toSize();
  resize(size);
  move(pos);
  restoreState(settings.value("dockstate").toByteArray());
  lastUsedDir = settings.value("lastuseddir", "").toString();
  lastFileDir = settings.value("lastfiledir", "").toString();
  showLiveEventsAct->setChecked(settings.value("liveEventsActive", true).toBool());
  m_options->language = _configlists.languageCodes.indexOf(settings.value("language", QLocale::system().name()).toString());
  if (m_options->language < 0)
    m_options->language = 0;
  recentFiles.clear();
  recentFiles = settings.value("recentFiles").toStringList();
  settings.beginGroup("Shortcuts");
  if (settings.contains("0")) {
    for (int i = 0; i < m_keyActions.size();i++) {
      QString shortcut = settings.value(QString::number(i), "").toString();
      m_keyActions[i]->setShortcut(shortcut);
    }
  }
  else { // No shortcuts are saved, so it is a new installation.
    setDefaultKeyboardShortcuts();
  }
  settings.endGroup();
  settings.endGroup();
  settings.beginGroup("Options");
  settings.beginGroup("Editor");
  m_options->font = settings.value("font", "Courier").toString();
  m_options->fontPointSize = settings.value("fontsize", 12).toDouble();
  m_options->consoleFont = settings.value("consolefont", "Courier").toString();
  m_options->consoleFontPointSize = settings.value("consolefontsize", 10).toDouble();

  m_options->consoleFontColor = settings.value("consoleFontColor", QVariant(QColor(Qt::black))).value<QColor>();
  m_options->consoleBgColor = settings.value("consoleBgColor", QVariant(QColor(Qt::white))).value<QColor>();
  m_options->tabWidth = settings.value("tabWidth", 40).toInt();
  m_options->colorVariables = settings.value("colorvariables", true).toBool();
  m_options->autoPlay = settings.value("autoplay", false).toBool();
  m_options->saveChanges = settings.value("savechanges", true).toBool();
  m_options->rememberFile = settings.value("rememberfile", true).toBool();
  m_options->saveWidgets = settings.value("savewidgets", true).toBool();
  m_options->iconText = settings.value("iconText", true).toBool();
  m_options->wrapLines = settings.value("wrapLines", true).toBool();
  m_options->useInvalue = settings.value("useInvalue", true).toBool();
  m_options->showWidgetsOnRun = settings.value("showWidgetsOnRun", true).toBool();
  m_options->showTooltips = settings.value("showTooltips", true).toBool();
  m_options->enableFLTK = settings.value("enableFLTK", true).toBool();
  m_options->terminalFLTK = settings.value("terminalFLTK", false).toBool();
  m_options->scrollbars = settings.value("scrollbars", true).toBool();
  lastFiles = settings.value("lastfiles", "").toStringList();
  lastTabIndex = settings.value("lasttabindex", "").toInt();
  settings.endGroup();
  settings.beginGroup("Run");
  m_options->useAPI = settings.value("useAPI", true).toBool();
  m_options->thread = settings.value("thread", true).toBool();
  m_options->keyRepeat = settings.value("keyRepeat", false).toBool();
  m_options->consoleBufferSize = settings.value("consoleBufferSize", 1024).toInt();
  m_options->bufferSize = settings.value("bufferSize", 1024).toInt();
  m_options->bufferSizeActive = settings.value("bufferSizeActive", false).toBool();
  m_options->HwBufferSize = settings.value("HwBufferSize", 1024).toInt();
  m_options->HwBufferSizeActive = settings.value("HwBufferSizeActive", false).toBool();
  m_options->dither = settings.value("dither", false).toBool();
  m_options->additionalFlags = settings.value("additionalFlags", "").toString();
  if (settingsVersion < 1)
    m_options->additionalFlags.remove("-d");  // remove old -d preference, as it is fixed now.
  m_options->additionalFlagsActive = settings.value("additionalFlagsActive", false).toBool();
  m_options->fileUseOptions = settings.value("fileUseOptions", true).toBool();
  m_options->fileOverrideOptions = settings.value("fileOverrideOptions", false).toBool();
  m_options->fileAskFilename = settings.value("fileAskFilename", false).toBool();
  m_options->filePlayFinished = settings.value("filePlayFinished", false).toBool();
  m_options->fileFileType = settings.value("fileFileType", 0).toInt();
  m_options->fileSampleFormat = settings.value("fileSampleFormat", 1).toInt();
  m_options->fileInputFilenameActive = settings.value("fileInputFilenameActive", false).toBool();
  m_options->fileInputFilename = settings.value("fileInputFilename", "").toString();
  m_options->fileOutputFilenameActive = settings.value("fileOutputFilenameActive", false).toBool();
  m_options->fileOutputFilename = settings.value("fileOutputFilename", "").toString();
  m_options->rtUseOptions = settings.value("rtUseOptions", true).toBool();
  m_options->rtOverrideOptions = settings.value("rtOverrideOptions", false).toBool();
  m_options->enableWidgets = settings.value("enableWidgets", true).toBool();
  m_options->rtAudioModule = settings.value("rtAudioModule", 0).toInt();
  m_options->rtInputDevice = settings.value("rtInputDevice", "adc").toString();
  m_options->rtOutputDevice = settings.value("rtOutputDevice", "dac").toString();
  m_options->rtJackName = settings.value("rtJackName", "").toString();
  m_options->rtMidiModule = settings.value("rtMidiModule", 0).toInt();
  m_options->rtMidiInputDevice = settings.value("rtMidiInputDevice", "0").toString();
  m_options->rtMidiOutputDevice = settings.value("rtMidiOutputDevice", "").toString();
  m_options->sampleFormat = settings.value("sampleFormat", 0).toInt();
  settings.endGroup();
  settings.beginGroup("Environment");
  m_options->csdocdir = settings.value("csdocdir", DEFAULT_HTML_DIR).toString();
  m_options->opcodedir = settings.value("opcodedir","").toString();
  m_options->opcodedirActive = settings.value("opcodedirActive","").toBool();
  m_options->sadir = settings.value("sadir","").toString();
  m_options->sadirActive = settings.value("sadirActive","").toBool();
  m_options->ssdir = settings.value("ssdir","").toString();
  m_options->ssdirActive = settings.value("ssdirActive","").toBool();
  m_options->sfdir = settings.value("sfdir","").toString();
  m_options->sfdirActive = settings.value("sfdirActive","").toBool();
  m_options->incdir = settings.value("incdir","").toString();
  m_options->incdirActive = settings.value("incdirActive","").toBool();
  m_options->defaultCsd = settings.value("defaultCsd","").toString();
  m_options->defaultCsdActive = settings.value("defaultCsdActive","").toBool();
  m_options->opcodexmldir = settings.value("opcodexmldir", "").toString();
  m_options->opcodexmldirActive = settings.value("opcodexmldirActive","").toBool();
  settings.endGroup();
  settings.beginGroup("External");
  m_options->terminal = settings.value("terminal", DEFAULT_TERM_EXECUTABLE).toString();
  m_options->browser = settings.value("browser", DEFAULT_BROWSER_EXECUTABLE).toString();
  m_options->dot = settings.value("dot", DEFAULT_DOT_EXECUTABLE).toString();
  m_options->waveeditor = settings.value("waveeditor",
                                         DEFAULT_WAVEEDITOR_EXECUTABLE
                                        ).toString();
  m_options->waveplayer = settings.value("waveplayer",
                                         DEFAULT_WAVEPLAYER_EXECUTABLE
                                        ).toString();
  m_options->pdfviewer = settings.value("pdfviewer",
                                         DEFAULT_PDFVIEWER_EXECUTABLE
                                        ).toString();
  settings.endGroup();
  settings.endGroup();
}

void qutecsound::writeSettings()
{
  QSettings settings("csound", "qutecsound");
  settings.setValue("settingsVersion", 1);
  settings.beginGroup("GUI");
  settings.setValue("pos", pos());
  settings.setValue("size", size());
  settings.setValue("dockstate", saveState());
  settings.setValue("lastuseddir", lastUsedDir);
  settings.setValue("lastfiledir", lastFileDir);
  settings.setValue("language", _configlists.languageCodes[m_options->language]);
  settings.setValue("liveEventsActive", showLiveEventsAct->isChecked());
  settings.setValue("recentFiles", recentFiles);
  settings.beginGroup("Shortcuts");
  for (int i = 0; i < m_keyActions.size();i++) {
//     QString key = m_keyActions[i]->text();
    QString key = QString::number(i);
    settings.setValue(key, m_keyActions[i]->shortcut().toString());
  }
  settings.endGroup();
  settings.endGroup();
  settings.beginGroup("Options");
  settings.beginGroup("Editor");
  settings.setValue("font", m_options->font );
  settings.setValue("fontsize", m_options->fontPointSize);
  settings.setValue("consolefont", m_options->consoleFont );
  settings.setValue("consolefontsize", m_options->consoleFontPointSize);
  settings.setValue("consoleFontColor", QVariant(m_options->consoleFontColor));
  settings.setValue("consoleBgColor", QVariant(m_options->consoleBgColor));
  settings.setValue("tabWidth", m_options->tabWidth );
  settings.setValue("colorvariables", m_options->colorVariables);
  settings.setValue("autoplay", m_options->autoPlay);
  settings.setValue("savechanges", m_options->saveChanges);
  settings.setValue("rememberfile", m_options->rememberFile);
  settings.setValue("savewidgets", m_options->saveWidgets);
  settings.setValue("iconText", m_options->iconText);
  settings.setValue("wrapLines", m_options->wrapLines);
  settings.setValue("enableWidgets", m_options->enableWidgets);
  settings.setValue("useInvalue", m_options->useInvalue);
  settings.setValue("showWidgetsOnRun", m_options->showWidgetsOnRun);
  settings.setValue("showTooltips", m_options->showTooltips);
  settings.setValue("enableFLTK", m_options->enableFLTK);
  settings.setValue("terminalFLTK", m_options->terminalFLTK);
  settings.setValue("scrollbars", m_options->scrollbars);
  QStringList files;
  if (m_options->rememberFile) {
    for (int i = 0; i < documentPages.size(); i++ ) {
          files.append(documentPages[i]->fileName);
    }
  }
  settings.setValue("lastfiles", files);
  settings.setValue("lasttabindex", documentTabs->currentIndex());
  settings.endGroup();
  settings.beginGroup("Run");
  settings.setValue("useAPI", m_options->useAPI);
  settings.setValue("thread", m_options->thread);
  settings.setValue("keyRepeat", m_options->keyRepeat);
  settings.setValue("consoleBufferSize", m_options->consoleBufferSize);
  settings.setValue("bufferSize", m_options->bufferSize);
  settings.setValue("bufferSizeActive", m_options->bufferSizeActive);
  settings.setValue("HwBufferSize",m_options->HwBufferSize);
  settings.setValue("HwBufferSizeActive", m_options->HwBufferSizeActive);
  settings.setValue("dither", m_options->dither);
  settings.setValue("additionalFlags", m_options->additionalFlags);
  settings.setValue("additionalFlagsActive", m_options->additionalFlagsActive);
  settings.setValue("fileUseOptions", m_options->fileUseOptions);
  settings.setValue("fileOverrideOptions", m_options->fileOverrideOptions);
  settings.setValue("fileAskFilename", m_options->fileAskFilename);
  settings.setValue("filePlayFinished", m_options->filePlayFinished);
  settings.setValue("fileFileType", m_options->fileFileType);
  settings.setValue("fileSampleFormat", m_options->fileSampleFormat);
  settings.setValue("fileInputFilenameActive", m_options->fileInputFilenameActive);
  settings.setValue("fileInputFilename", m_options->fileInputFilename);
  settings.setValue("fileOutputFilenameActive", m_options->fileOutputFilenameActive);
  settings.setValue("fileOutputFilename", m_options->fileOutputFilename);
  settings.setValue("rtUseOptions", m_options->rtUseOptions);
  settings.setValue("rtOverrideOptions", m_options->rtOverrideOptions);
  settings.setValue("rtAudioModule", m_options->rtAudioModule);
  settings.setValue("rtInputDevice", m_options->rtInputDevice);
  settings.setValue("rtOutputDevice", m_options->rtOutputDevice);
  settings.setValue("rtJackName", m_options->rtJackName);
  settings.setValue("rtMidiModule", m_options->rtMidiModule);
  settings.setValue("rtMidiInputDevice", m_options->rtMidiInputDevice);
  settings.setValue("rtMidiOutputDevice", m_options->rtMidiOutputDevice);
  settings.setValue("sampleFormat", m_options->sampleFormat);
  settings.endGroup();
  settings.beginGroup("Environment");
  settings.setValue("csdocdir", m_options->csdocdir);
  settings.setValue("opcodedir",m_options->opcodedir);
  settings.setValue("opcodedirActive",m_options->opcodedirActive);
  settings.setValue("sadir",m_options->sadir);
  settings.setValue("sadirActive",m_options->sadirActive);
  settings.setValue("ssdir",m_options->ssdir);
  settings.setValue("ssdirActive",m_options->ssdirActive);
  settings.setValue("sfdir",m_options->sfdir);
  settings.setValue("sfdirActive",m_options->sfdirActive);
  settings.setValue("incdir",m_options->incdir);
  settings.setValue("incdirActive",m_options->incdirActive);
  settings.setValue("defaultCsd",m_options->defaultCsd);
  settings.setValue("defaultCsdActive",m_options->defaultCsdActive);
  settings.setValue("opcodexmldir", m_options->opcodexmldir);
  settings.setValue("opcodexmldirActive",m_options->opcodexmldirActive);
  settings.endGroup();
  settings.beginGroup("External");
  settings.setValue("terminal", m_options->terminal);
  settings.setValue("browser", m_options->browser);
  settings.setValue("dot", m_options->dot);
  settings.setValue("waveeditor", m_options->waveeditor);
  settings.setValue("waveplayer", m_options->waveplayer);
  settings.setValue("pdfviewer", m_options->pdfviewer);
  settings.endGroup();
  settings.endGroup();
}

int qutecsound::execute(QString executable, QString options)
{
  qDebug() << "qutecsound::execute";
  QStringList optionlist;

//  // cd to current directory on all platforms
//  QString cdLine = "cd \"" + documentPages[curPage]->getFilePath() + "\"";
//  QProcess::execute(cdLine);

#ifdef Q_WS_MAC
  QString commandLine = "open -a \"" + executable + "\" " + options;
#endif
#ifdef Q_OS_LINUX
  QString commandLine = "\"" + executable + "\" " + options;
#endif
#ifdef Q_OS_SOLARIS
  QString commandLine = "\"" + executable + "\" " + options;
#endif
#ifdef Q_OS_WIN32
  QString commandLine = "\"" + executable + "\" " + (executable.startsWith("cmd")? " /k ": " ") + options;
  if (!QProcess::startDetached(commandLine))
      return 1;
#else
  qDebug() << "qutecsound::execute   " << commandLine << documentPages[curPage]->getFilePath();
  QProcess *p = new QProcess(this);
  p->setWorkingDirectory(documentPages[curPage]->getFilePath());
  p->start(commandLine);
  Q_PID id = p->pid();
  qDebug() << "Launched external program with id:" << id;
  if (!p->waitForStarted())
    return 1;
#endif
  return 0;
}

bool qutecsound::maybeSave()
{
  for (int i = 0; i< documentPages.size(); i++) {
    if (documentPages[i]->isModified()) {
      documentTabs->setCurrentIndex(i);
      changePage(i);
      QString message = tr("The document ")
          + (documentPages[i]->fileName != "" ? documentPages[i]->fileName: "untitled.csd")
          + tr("\nhas been modified.\nDo you want to save the changes before closing?");
      int ret = QMessageBox::warning(this, tr("QuteCsound"),
                                     message,
                                     QMessageBox::Yes | QMessageBox::Default,
                                     QMessageBox::No,
                                     QMessageBox::Cancel | QMessageBox::Escape);
      if (ret == QMessageBox::Yes) {
        if (!save())
          return false;
//         closeTab();
      }
      else if (ret == QMessageBox::Cancel) {
        return false;
      }
    }
  }
  return true;
}

bool qutecsound::loadFile(QString fileName, bool runNow)
{
  QFile file(fileName);
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
    return false;
  }
  int index = isOpen(fileName);
  if (index != -1) {
    documentTabs->setCurrentIndex(index);
    changePage(index);
    statusBar()->showMessage(tr("File already open"), 10000);
    return false;
  }
  if (curPage >= 0 && curPage < documentPages.size() && documentPages[curPage] != NULL) {
    documentPages[curPage]->showLiveEventFrames(false);
//    documentPages[curPage]->setMacWidgetsText(widgetPanel->widgetsText()); //Updated changes to widgets in file
    disconnect(showLiveEventsAct, SIGNAL(toggled(bool)), documentPages[curPage], SLOT(showLiveEventFrames(bool)));
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  DocumentPage *newPage = new DocumentPage(this, opcodeTree);
  documentPages.append(newPage);
  documentTabs->addTab(newPage->getView(),"");
//  widgetPanel->setWidgetLayout(newPage->getWidgetLayout());
  curPage = documentPages.size() - 1;
  documentPages[curPage]->setTabStopWidth(m_options->tabWidth);
  documentPages[curPage]->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  documentPages[curPage]->setColorVariables(m_options->colorVariables);
  documentPages[curPage]->setOpcodeNameList(opcodeTree->opcodeNameList());
//  documentPages[curPage]->setEditAct(editAct);
  documentTabs->setCurrentIndex(curPage);
  connectActions();
  connect(documentPages[curPage], SIGNAL(doCut()), this, SLOT(cut()));
  connect(documentPages[curPage], SIGNAL(doCopy()), this, SLOT(copy()));
  connect(documentPages[curPage], SIGNAL(doPaste()), this, SLOT(paste()));

  if (fileName.startsWith(m_options->csdocdir))
    documentPages[curPage]->readOnly = true;
  QString text;
  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    line.replace("\r\n", "\n");
    line.replace("\r", "\n");  //Change Mac returns to line endings
    QTextDecoder decoder(QTextCodec::codecForLocale());
    text = text + decoder.toUnicode(line);
    if (!line.endsWith("\n"))
      text += "\n";
  }
  documentPages[curPage]->setTextString(text, m_options->saveWidgets);
  documentPages[curPage]->showLiveEventFrames(showLiveEventsAct->isChecked());
  QApplication::restoreOverrideCursor();

//  documentPages[curPage]->showLiveEventFrames(showLiveEventsAct->isChecked());
  documentPages[curPage]->setModified(false);
  if (fileName == ":/default.csd")
    fileName = QString("");
  documentPages[curPage]->fileName = fileName;
  setCurrentFile(fileName);
  setWindowModified(false);
  documentTabs->setTabIcon(curPage, QIcon());
  if (!fileName.startsWith(":/")) {  // Don't store internal examples directory as last used dir
    lastUsedDir = fileName;
    lastUsedDir.resize(fileName.lastIndexOf(QRegExp("[/]")) + 1);
  }
  if (recentFiles.count(fileName) == 0 && fileName!="" && !fileName.startsWith(":/")) {
    recentFiles.prepend(fileName);
    if (recentFiles.size() > QUTE_MAX_RECENT_FILES)
      recentFiles.removeLast();
    fillFileMenu();
  }
  changeFont();
  statusBar()->showMessage(tr("File loaded"), 2000);
  textEdit = documentPages[curPage];
  setWidgetPanelGeometry();

  //FIXME m_inspector->parseText(documentPages[curPage]->toPlainText()); //necessary?
  updateInspector();
  // FIXME put back
//  widgetPanel->clearHistory();
  if (runNow && m_options->autoPlay) {
    play();
  }
  return true;
}

void qutecsound::loadCompanionFile(const QString &fileName)
{
  QString companionFileName = fileName;
  if (fileName.endsWith(".orc")) {
    companionFileName.replace(".orc", ".sco");
  }
  else if (fileName.endsWith(".sco")) {
    companionFileName.replace(".sco", ".orc");
  }
  else
    return;
  if (QFile::exists(companionFileName))
    loadFile(companionFileName);
}

bool qutecsound::saveFile(const QString &fileName, bool saveWidgets)
{
  qDebug("qutecsound::saveFile");
  QString text;
  documentTabs->setTabIcon(curPage, QIcon());
  QApplication::setOverrideCursor(Qt::WaitCursor);
  if (m_options->saveWidgets && saveWidgets)
    text = documentPages[curPage]->getFullText();
  else
    text = documentPages[curPage]->getBasicText();
  QApplication::restoreOverrideCursor();

  if (fileName != documentPages[curPage]->fileName && saveWidgets) {
    documentPages[curPage]->fileName = fileName;
    setCurrentFile(fileName);
  }
  textEdit->setModified(false);
  setWindowModified(false);
  lastUsedDir = fileName;
  lastUsedDir.resize(fileName.lastIndexOf("/") + 1);
  if (recentFiles.count(fileName) == 0) {
    recentFiles.prepend(fileName);
    recentFiles.removeLast();
    fillFileMenu();
  }
  QFile file(fileName);
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("Application"),
                         tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
    return false;
  }

  QTextStream out(&file);
  out << text;
  statusBar()->showMessage(tr("File saved"), 2000);
  return true;
}

void qutecsound::setCurrentFile(const QString &fileName)
{
  QString shownName;
  if (fileName.isEmpty())
    shownName = "untitled.csd";
  else
    shownName = strippedName(fileName);

  setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("QuteCsound")));
  documentTabs->setTabText(curPage, shownName);
//  updateWidgets();
}

QString qutecsound::strippedName(const QString &fullFileName)
{
  return QFileInfo(fullFileName).fileName();
}

QString qutecsound::generateScript(bool realtime, QString tempFileName)
{
#ifndef Q_OS_WIN32
  QString script = "#!/bin/sh\n";
#else
  QString script = "";
#endif

  QString cmdLine = "";
  if (m_options->opcodedirActive)
    script += "export OPCODEDIR=" + m_options->opcodedir + "\n";
    // Only OPCODEDIR left here as it must be present before csound initializes
    // The problem is that it can't be passed when using the API...
//   if (m_options->sadirActive)
//     script += "export SADIR=" + m_options->sadir + "\n";
//   if (m_options->ssdirActive)
//     script += "export SSDIR=" + m_options->ssdir + "\n";
//   if (m_options->sfdirActive)
//     script += "export SFDIR=" + m_options->sfdir + "\n";
//   if (m_options->ssdirActive)
//     script += "export INCDIR=" + m_options->incdir + "\n";

#ifndef Q_OS_WIN32
  script += "cd " + QFileInfo(documentPages[curPage]->fileName).absolutePath() + "\n";
#else
  QString script_cd = "@pushd " + QFileInfo(documentPages[curPage]->fileName).absolutePath() + "\n";
  script_cd.replace("/", "\\");
  script += script_cd;
#endif

#ifdef Q_WS_MAC
  cmdLine = "/usr/local/bin/csound ";
#else
  cmdLine = "csound ";
#endif

  if (tempFileName == ""){
    if (documentPages[curPage]->companionFile != "") {
      if (documentPages[curPage]->fileName.endsWith(".orc"))
        cmdLine += "\""  + documentPages[curPage]->fileName
            + "\" \""+ documentPages[curPage]->companionFile + "\" ";
      else
        cmdLine += "\""  + documentPages[curPage]->companionFile
            + "\" \""+ documentPages[curPage]->fileName + "\" ";
    }
    else if (documentPages[curPage]->fileName.endsWith(".csd",Qt::CaseInsensitive)) {
      cmdLine += "\""  + documentPages[curPage]->fileName + "\" ";
    }
  }
  else {
    cmdLine += "\""  + tempFileName + "\" ";
  }
  m_options->rt = (realtime and m_options->rtUseOptions)
                  or (!realtime and m_options->fileUseOptions);
  cmdLine += m_options->generateCmdLineFlags();
  script += "echo \"" + cmdLine + "\"\n";
  script += cmdLine + "\n";

#ifndef Q_OS_WIN32
  script += "echo \"\nPress return to continue\"\n";
  script += "dummy_var=\"\"\n";
  script += "read dummy_var\n";
  script += "rm $0\n";
#else
  script += "@echo.\n";
  script += "@pause\n";
  script += "@exit\n";
#endif
  return script;
}

void qutecsound::getCompanionFileName()
{
  QString fileName = "";
  QDialog dialog(this);
  dialog.resize(400, 200);
  dialog.setModal(true);
  QPushButton *button = new QPushButton(tr("Ok"));

  connect(button, SIGNAL(released()), &dialog, SLOT(accept()));

  QSplitter *splitter = new QSplitter(&dialog);
  QListWidget *list = new QListWidget(&dialog);
  QCheckBox *checkbox = new QCheckBox(tr("Do not ask again"), &dialog);
  splitter->addWidget(list);
  splitter->addWidget(checkbox);
  splitter->addWidget(button);
  splitter->resize(400, 200);
  splitter->setOrientation(Qt::Vertical);
  QString extensionComplement = "";
  if (documentPages[curPage]->fileName.endsWith(".orc"))
    extensionComplement = ".sco";
  else if (documentPages[curPage]->fileName.endsWith(".sco"))
    extensionComplement = ".orc";

  for (int i = 0; i < documentPages.size(); i++) {
    QString name = documentPages[i]->fileName;
    if (documentPages[i]->fileName.endsWith(extensionComplement))
      list->addItem(documentPages[i]->fileName);
  }
  QList<QListWidgetItem *> itemList = list->findItems(documentPages[curPage]->companionFile,
      Qt::MatchExactly);
  if (itemList.size() > 0)
    list->setCurrentItem(itemList[0]);
  dialog.exec();
  QListWidgetItem *item = list->currentItem();
  QString itemText = item->text();
  if (checkbox->isChecked())
    documentPages[curPage]->askForFile = false;
  documentPages[curPage]->companionFile = itemText;
  for (int i = 0; i < documentPages.size(); i++) {
    if (documentPages[i]->fileName == documentPages[curPage]->companionFile) {
      documentPages[i]->companionFile = documentPages[curPage]->fileName;
      documentPages[i]->askForFile = documentPages[curPage]->askForFile;
      break;
    }
  }
}
void qutecsound::setWidgetPanelGeometry()
{
  QRect geometry = documentPages[curPage]->getWidgetPanelGeometry();
  if (geometry.width() == 0)
    return;
  if (geometry.x() < 0) {
    geometry.setX(10);
    qDebug() << "qutecsound::setWidgetPanelGeometry() Warining: X is negative.";
  }
  if (geometry.y() < 0) {
    geometry.setY(10);
    qDebug() << "qutecsound::setWidgetPanelGeometry() Warining: Y is negative.";
  }
  widgetPanel->setGeometry(geometry);
}

int qutecsound::isOpen(QString fileName)
{
  int open = -1;
  int i = 0;
  for (i = 0; i < documentPages.size(); i++) {
      if (documentPages[i]->fileName == fileName) {
        open = i;
        break;
      }
  }
  return open;
}

//void qutecsound::markErrorLine()
//{
////   qDebug("qutecsound::markErrorLine()");
//  documentPages[curPage]->markErrorLines(m_console->errorLines);
//}


QStringList qutecsound::runCsoundInternally(QStringList flags)
{
  qDebug("qutecsound::runCsoundInternally()");
  static char *argv[33];
  int index = 0;
  foreach (QString flag, flags) {
    argv[index] = (char *) calloc(flag.size()+1, sizeof(char));
    strcpy(argv[index],flag.toStdString().c_str());
    index++;
  }
  int argc = flags.size();
#ifdef MACOSX_PRE_SNOW
//Remember menu bar to set it after FLTK grabs it
  menuBarHandle = GetMenuBar();
#endif
  m_deviceMessages.clear();
  CSOUND *csoundD;
  csoundD=csoundCreate(0);
  csoundReset(csoundD);
  csoundSetHostData(csoundD, (void *) &m_deviceMessages);
  csoundSetMessageCallback(csoundD, &qutecsound::devicesMessageCallback);
  int result = csoundCompile(csoundD,argc,argv);
  if(!result){
    while(csoundPerformKsmps(csoundD)==0) {};
  }

  csoundCleanup(csoundD);
  csoundDestroy(csoundD);

#ifdef MACOSX_PRE_SNOW
// Put menu bar back
  SetMenuBar(menuBarHandle);
#endif
  return m_deviceMessages;
}

//void qutecsound::newCurve(Curve * curve)
//{
//  newCurveBuffer.append(curve);
//}
//

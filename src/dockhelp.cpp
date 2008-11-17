/***************************************************************************
 *   Copyright (C) 2008 by Andres Cabrera   *
 *   mantaraya36@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include "dockhelp.h"

#include <QTextBrowser>
#include <QTextDocument>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDir>

DockHelp::DockHelp(QWidget *parent)
  : QDockWidget(parent)
{
  setWindowTitle("Opcode Help");
  setMinimumSize(400,200);
  text = new QTextBrowser(this);
  text->setAcceptRichText(true);
  setWidget (text);
}

DockHelp::~DockHelp()
{
}

void DockHelp::loadFile(QString fileName)
{
  QFile file(fileName);
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
//     QMessageBox::warning(this, tr("QuteCsound"),
//                          tr("Cannot read file %1:\n%2.")
//                              .arg(fileName)
//                              .arg(file.errorString()));
    return;
  }
  //FIXME: Fix this hack so it works fine in windows as well...
  QStringList searchPaths;
  searchPaths << docDir;
  text->setSearchPaths(searchPaths);
#ifdef WIN32
  QTextStream in(&file);
  in.setAutoDetectUnicode(true);
  text->setHtml(in.readAll());
#else
  text->setSource(QUrl(fileName));
#endif

}

void DockHelp::closeEvent(QCloseEvent * event)
{
  emit Close(false);
}

void DockHelp::showGen()
{
  this->setVisible(true);
  this->loadFile(docDir + "/ScoreGenRef.html");
}

void DockHelp::showOverview()
{
  this->setVisible(true);
  this->loadFile(docDir + "/PartOpcodesOverview.html");
}

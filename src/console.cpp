/***************************************************************************
 *   Copyright (C) 2008 by Andres Cabrera   *
 *   mantaraya36@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "console.h"

Console::Console(QWidget * parent)
  : QDockWidget(parent)
{
  setWindowTitle("Csound Output Console");
  text = new QTextEdit();
  text->setReadOnly(true);
  setWidget (text);
  text->document()->setDefaultFont(QFont("Courier", 10));
}

Console::~Console()
{
}

void Console::appendMessage(QString msg)
{
  if (msg.contains("B ") or msg.contains("rtevent", Qt::CaseInsensitive)) {
    text->setTextColor(QColor("blue"));
  }
  if (msg.contains("error", Qt::CaseInsensitive)
      or msg.contains("overall samples out of range")) {
    text->setTextColor(QColor("red"));
  }
  if (msg.contains("warning", Qt::CaseInsensitive)) {
    text->setTextColor(QColor("orange"));
  }
  text->insertPlainText(msg);
  text->moveCursor(QTextCursor::End);
  text->setTextColor(QColor("black"));
}

void Console::clear()
{
  text->clear();
}

void Console::closeEvent(QCloseEvent * event)
{
  emit Close(false);
}
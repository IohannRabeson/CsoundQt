/***************************************************************************
 *   Copyright (C) 2008 by Andres Cabrera                                  *
 *   mantaraya36@gmail.com                                                 *
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
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/
#include "documentpage.h"

DocumentPage::DocumentPage(QWidget *parent):
    QTextEdit(parent)
{
  fileName = "";
  companionFile = "";
  askForFile = true;
}


DocumentPage::~DocumentPage()
{
}

int DocumentPage::setTextString(QString text)
{
  if (text.contains("<MacOptions>") and text.contains("</MacOptions>")) {
    macOptions = text.right(text.size()-text.indexOf("<MacOptions>"));
    qDebug("<MacOptions> present. \n%s", macOptions.toStdString().c_str());
    macOptions.resize(macOptions.indexOf("</MacOptions>") + 12);
    qDebug("<MacOptions> present. \n%s", macOptions.toStdString().c_str());
    if (text.indexOf("</MacOptions>") + 12 < text.size() and text[text.indexOf("</MacOptions>") + 13] == '\n')
      text.remove(text.indexOf("</MacOptions>") + 13, 1); //remove final line break
    if (text.indexOf("<MacOptions>") > 0 and text[text.indexOf("<MacOptions>") - 1] == '\n')
      text.remove(text.indexOf("<MacOptions>") - 1, 1); //remove initial line break
    text.remove(text.indexOf("<MacOptions>"), macOptions.size());
    qDebug("<MacOptions> present. \n%s", macOptions.toStdString().c_str());
  }
  else {
    macOptions = "";
  }
  if (text.contains("<MacPresets>") and text.contains("</MacPresets>")) {
    macPresets = text.right(text.size()-text.indexOf("<MacPresets>"));
    macPresets.resize(macPresets.indexOf("</MacPresets>") + 13);
    if (text.indexOf("</MacPresets>") + 12 < text.size() and text[text.indexOf("</MacPresets>") + 13] == '\n')
      text.remove(text.indexOf("</MacPresets>") + 13, 1); //remove final line break
    if (text.indexOf("<MacPresets>") > 0 and text[text.indexOf("<MacPresets>") - 1] == '\n')
      text.remove(text.indexOf("<MacPresets>") - 1, 1); //remove initial line break
    text.remove(text.indexOf("<MacPresets>") - 1, macPresets.size() + 2);
    qDebug("<MacPresets> present.");
  }
  else {
    macPresets = "";
  }
  if (text.contains("<MacGUI>") and text.contains("</MacGUI>")) {
    macGUI = text.right(text.size()-text.indexOf("<MacGUI>"));
    macGUI.resize(macGUI.indexOf("</MacGUI>") + 8);
    if (text.indexOf("</MacGUI>") + 8 < text.size() and text[text.indexOf("</MacGUI>") + 9] == '\n')
      text.remove(text.indexOf("</MacGUI>") + 9, 1); //remove final line break
    if (text.indexOf("<MacGUI>") > 0 and text[text.indexOf("<MacGUI>") - 1] == '\n')
      text.remove(text.indexOf("<MacGUI>") - 1, 1); //remove initial line break
    text.remove(text.indexOf("<MacGUI>") - 1, macGUI.size() + 2);
    qDebug("<MacGUI> present.");
  }
  else {
    macGUI = "\n<MacGUI>\nioView nobackground {59352, 11885, 65535}\nioSlider {5, 5} {20, 100} 0.000000 1.000000 0.000000 slider1\nioSlider {45, 5} {20, 100} 0.000000 1.000000 0.000000 slider2\nioSlider {85, 5} {20, 100} 0.000000 1.000000 0.000000 slider3\nioSlider {125, 5} {20, 100} 0.000000 1.000000 0.000000 slider4\nioSlider {165, 5} {20, 100} 0.000000 1.000000 0.000000 slider5\n</MacGUI>\n";
  }
  setPlainText(text);
  return 0;
}

QString DocumentPage::getFullText()
{
  QString fullText;
  fullText = document()->toPlainText();
  if (!fullText.endsWith("\n"))
    fullText += "\n";
  fullText += macOptions + "\n" + macGUI + "\n" + macPresets + "\n";
  return fullText;
}

QString DocumentPage::getMacWidgetsText()
{
  return macGUI;
}

void DocumentPage::setMacWidgetsText(QString text)
{
  qDebug("DocumentPage::setMacWidgetsText");
  macGUI = text;
  document()->setModified(true);
}

void DocumentPage::comment()
{
  QTextCursor cursor = textCursor();
  QString text = cursor.selectedText();
  text.prepend(";");
  text.replace(QChar(QChar::ParagraphSeparator), QString("\n;"));
  cursor.insertText(text);
  setTextCursor(cursor);
}

void DocumentPage::uncomment()
{
  QTextCursor cursor = textCursor();
  QString text = cursor.selectedText();
  if (text.startsWith(";"))
    text.remove(0,1);
  text.replace(QChar(QChar::ParagraphSeparator), QString("\n"));
  text.replace(QString("\n;"), QString("\n")); //TODO make more robust
  cursor.insertText(text);
  setTextCursor(cursor);
}

void DocumentPage::indent()
{
  qDebug("DocumentPage::indent");
  QTextCursor cursor = textCursor();
  QString text = cursor.selectedText();
//   if (text[0] == '\n')
  text.prepend("\t"); //TODO check if previous character is \n
  text.replace(QChar(QChar::ParagraphSeparator), QString("\n\t"));
  cursor.insertText(text);
  setTextCursor(cursor);
}

void DocumentPage::unindent()
{
  QTextCursor cursor = textCursor();
  QString text = cursor.selectedText();
  if (text.startsWith("\t"))
    text.remove(0,1);
  text.replace(QChar(QChar::ParagraphSeparator), QString("\n"));
  text.replace(QString("\n\t"), QString("\n")); //TODO make more robust
  cursor.insertText(text);
  setTextCursor(cursor);
}

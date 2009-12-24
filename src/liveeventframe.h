/*
    Copyright (C) 2009 Andres Cabrera
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

#ifndef LIVEEVENTFRAME_H
#define LIVEEVENTFRAME_H

#include "ui_liveeventframe.h"

class EventSheet;

class LiveEventFrame : public QFrame, private Ui::LiveEventFrame
{
    Q_OBJECT
  public:
    LiveEventFrame(QString csdName, QWidget *parent = 0);
    EventSheet * getSheet();

  protected:
    void changeEvent(QEvent *e);

  private:
    QWidget *m_editor; //TODO add text editor
    QString m_csdName;
    EventSheet *m_sheet;  // Can this be made private some way?
};

#endif // LIVEEVENTFRAME_H
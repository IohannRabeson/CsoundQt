/*
	Copyright (C) 2008, 2009 Andres Cabrera
	mantaraya36@gmail.com

	This file is part of CsoundQt.

	CsoundQt is free software; you can redistribute it
	and/or modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	CsoundQt is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with Csound; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
	02111-1307 USA
*/

#ifndef QUTEGRAPH_H
#define QUTEGRAPH_H

#include "qutewidget.h"
#include "csoundengine.h"  //necessary for the CsoundUserData struct

class Curve;

class QuteGraph : public QuteWidget
{
	Q_OBJECT

//	Q_PROPERTY(bool QCS_grid MEMBER m_grid)
//	Q_PROPERTY(bool QCS_logx MEMBER m_logx)
//	Q_PROPERTY(bool QCS_logy MEMBER m_logy)

public:
	QuteGraph(QWidget *parent);

	~QuteGraph();

	virtual QString getWidgetLine();
	virtual QString getWidgetXmlText();
	virtual QString getWidgetType();
	virtual void setWidgetGeometry(int x,int y,int width,int height);
	void setValue(double value);
	void clearCurves();
	void addCurve(Curve *curve);
	int getCurveIndex(Curve * curve);
	void setCurveData(Curve * curve);
	void setUd(CsoundUserData *ud);
	virtual void applyInternalProperties();

protected:
	CsoundUserData *m_ud;
	QLabel * m_label;
	QComboBox *m_pageComboBox;
	QLineEdit* name2LineEdit;
	QDoubleSpinBox *zoomxBox;
	QDoubleSpinBox *zoomyBox;
	QVector<Curve *> curves;
	QVector<QVector <QGraphicsLineItem *> > lines;
	QVector<QGraphicsPolygonItem *> polygons;

	QVector<QVector <QGraphicsLineItem *> > m_gridlines;
	QVector<QVector <QGraphicsTextItem *> > m_gridtext;

	virtual void refreshWidget();
	virtual void createPropertiesDialog();
	virtual void applyProperties();

public slots:
	void changeCurve(int index);
	void indexChanged(int index);

private:
	void drawFtable(Curve * curve, int index);
    void drawSpectrum(Curve * curve, int index);
    void drawSignal(Curve * curve, int index);
	void scaleGraph(int index);
	int getTableNumForIndex(int index);
	int getIndexForTableNum(int ftable);
	void setInternalValue(double value);

	//    QMutex curveLock;
	bool m_grid;
	bool m_logx;
	bool m_logy;
};

class StackedLayoutWidget : public QStackedWidget
{
	Q_OBJECT
public:
	StackedLayoutWidget(QWidget* parent) : QStackedWidget(parent)
	{
		setFrameShape(QFrame::StyledPanel);
	}
	~StackedLayoutWidget() {}

	void setWidgetGeometry(int x,int y,int width,int height)
	{
		setGeometry(x,y,width, height);
		setMaximumSize(width, height);
	}

	void clearCurves()
	{
		QWidget *widget;
		widget = currentWidget();
		while (widget != 0) {
			removeWidget(widget);
			widget = currentWidget();
		}
	}
	/*
  protected:
	virtual void contextMenuEvent(QContextMenuEvent *event)
	{emit(popUpMenu(event->globalPos()));}

  signals:
	void popUpMenu(QPoint pos);*/
};

#endif

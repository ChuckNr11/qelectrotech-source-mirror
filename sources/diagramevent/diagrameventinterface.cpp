/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "diagrameventinterface.h"

#include "../diagram.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

DiagramEventInterface::DiagramEventInterface(Diagram *diagram) :
	QObject{diagram},
	m_diagram{diagram},
	m_help_horiz (nullptr), // Achim DiagramEditor helpCross
	m_help_verti (nullptr)	// Achim DiagramEditor helpCross
{
	m_diagram -> clearSelection();
}

DiagramEventInterface::~DiagramEventInterface()
{
	delete m_help_horiz;	// Achim DiagramEditor helpCross
	delete m_help_verti;	// Achim DiagramEditor helpCross
}

void DiagramEventInterface::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
	Q_UNUSED (event);
}

void DiagramEventInterface::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	Q_UNUSED (event);
}

void DiagramEventInterface::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	Q_UNUSED (event);
}

void DiagramEventInterface::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	Q_UNUSED (event);
}

void DiagramEventInterface::wheelEvent(QGraphicsSceneWheelEvent *event) {
	Q_UNUSED (event);
}

/**
	@brief DiagramEventInterface::keyPressEvent
	By default, press escape key abort the current action
	@param event
*/
void DiagramEventInterface::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape)
	{
		m_running = false;
		m_abort = true;
		emit finish();
		event->setAccepted(true);
	}
}

void DiagramEventInterface::keyReleaseEvent(QKeyEvent *event){
	Q_UNUSED (event);
}

bool DiagramEventInterface::isRunning() const
{
	return m_running;
}

void DiagramEventInterface::init()
{}

// Achim DiagramEditor helpCross
/*
	Deklaration und Definition von updateHelpCross
	in das DiagramEventInterface verlegt, damit alle
	abgeleiteten Klassen das HelpCross nutzen können
*/
/**
	@brief DiagramEventInterface::updateHelpCross
	Create and update the position of the cross to help user for draw new shape
	@param p : the center of the cross
*/
void DiagramEventInterface::updateHelpCross(const QPointF &p)
{
  //If line isn't created yet, we create it.
	if (!m_help_horiz || !m_help_verti)
	{
		QPen pen;
		pen.setWidthF(0.4);
		pen.setCosmetic(true);
		pen.setColor(Diagram::background_color == Qt::darkGray ? Qt::lightGray : Qt::darkGray);

		QRectF rect = m_diagram->border_and_titleblock.insideBorderRect();

		if (!m_help_horiz)
		{
			m_help_horiz = new QGraphicsLineItem(rect.topLeft().x(), 0, rect.topRight().x(), 0);
			m_help_horiz->setPen(pen);
			m_diagram->addItem(m_help_horiz);
		}

		if (!m_help_verti)
		{
			m_help_verti = new QGraphicsLineItem(0, rect.topLeft().y(), 0, rect.bottomLeft().y());
			m_help_verti->setPen(pen);
			m_diagram->addItem(m_help_verti);
		}
	}

		   //Update the position of the cross
	QPointF point = Diagram::snapToGrid(p);

	m_help_horiz->setY(point.y());
	m_help_verti->setX(point.x());
}

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
#include "diagrameventaddshape.h"

#include "../diagram.h"
#include "../undocommand/addgraphicsobjectcommand.h"

/**
	@brief DiagramEventAddShape::DiagramEventAddShape
	Default constructor
	@param diagram : the diagram where this event must operate
	@param shape_type : the type of shape to draw
*/
DiagramEventAddShape::DiagramEventAddShape(Diagram *diagram, QetShapeItem::ShapeType shape_type) :
	DiagramEventInterface(diagram),
	m_shape_type (shape_type),
	m_shape_item (nullptr)
{
	m_running = true;
	init();
}

/**
	@brief DiagramEventAddShape::~DiagramEventAddShape
*/
DiagramEventAddShape::~DiagramEventAddShape()
{
	if ((m_running || m_abort) && m_shape_item)
	{
		m_diagram->removeItem(m_shape_item);
		delete m_shape_item;
	}

	foreach (QGraphicsView *v, m_diagram->views())
		v->setContextMenuPolicy(Qt::DefaultContextMenu);
}

/**
	@brief DiagramEventAddShape::mousePressEvent
	Action when mouse is pressed
	@param event : event of mouse press
*/
void DiagramEventAddShape::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (Q_UNLIKELY(m_diagram->isReadOnly())) {
		return;
	}

	QPointF pos = event->scenePos();
	if (event->modifiers() != Qt::ControlModifier) {
		pos = Diagram::snapToGrid(pos);
	}

		//Action for left mouse click
	if (event->button() == Qt::LeftButton)
	{
			//Create shape item
		if (!m_shape_item)
		{
			m_shape_item = new QetShapeItem(pos, pos, m_shape_type);
			m_diagram->addItem (m_shape_item);
			event->setAccepted(true);
			return;
		}

			//If current item isn't a polyline, add it with an undo command
		if (m_shape_type != QetShapeItem::Polygon)
		{
			m_shape_item->setP2 (pos);
			if (m_shape_item->shapeType() == QetShapeItem::Rectangle || m_shape_item->shapeType() == QetShapeItem::Ellipse) {
				m_shape_item->setRect(m_shape_item->rect().normalized());
			}
			m_diagram->undoStack().push (new AddGraphicsObjectCommand(m_shape_item, m_diagram));
			m_shape_item = nullptr; //< set to nullptr for create new shape at next left clic
		}
			//Else add a new point to polyline
		else
		{
			m_shape_item->setNextPoint (pos);
		}

		event->setAccepted(true);
		return;
	}

	if (event->button() == Qt::RightButton) {
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddShape::mouseMoveEvent
	Action when mouse move
	@param event : event of mouse move
*/
void DiagramEventAddShape::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	updateHelpCross(event->scenePos());

	if (m_shape_item && event->buttons() == Qt::NoButton)
	{
		QPointF pos = event->scenePos();
		if (event->modifiers() != Qt::ControlModifier) {
			pos = Diagram::snapToGrid(pos);
		}

		m_shape_item->setP2 (pos);
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddShape::mouseReleaseEvent
	Action when mouse button is released
	@param event : event of mouse release
*/
void DiagramEventAddShape::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::RightButton)
	{
			//If shape is created, we manage right click
		if (m_shape_item)
		{
				//Shape is a polyline and have three points or more we just remove the last point
			if (m_shape_type == QetShapeItem::Polygon && (m_shape_item->pointsCount() >= 3) )
			{
				m_shape_item->removePoints();

				QPointF pos = event->scenePos();
				if (event->modifiers() != Qt::ControlModifier)
					pos = Diagram::snapToGrid(pos);

				m_shape_item->setP2(pos); //Set the new last point under the cursor
				event->setAccepted(true);
				return;
			}

				//For other case, we remove item from scene
			m_diagram->removeItem(m_shape_item);
			delete m_shape_item;
			m_shape_item = nullptr;
			event->setAccepted(true);
			return;
		}

			//Else (no shape), we set to false the running status
			//for indicate to the owner of this event that everything is done
		m_running = false;
		emit finish();
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddShape::mouseDoubleClickEvent
	Action when mouse button is double clicked
	@param event : event of mouse double click
*/
void DiagramEventAddShape::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
		//If current item is a polyline, add it with an undo command
	if (m_shape_item && m_shape_type == QetShapeItem::Polygon && event->button() == Qt::LeftButton)
	{
			//<double clic is used to finish polyline, but they also add two points at the same pos
			//<(double clic is a double press event), so we remove the last point of polyline
		m_shape_item->removePoints();

			//If the last is at the same pos of the first point
			//that mean user want a closed polygon, so we remove the last point and close polygon
		QPolygonF polygon = m_shape_item->polygon();
		if (polygon.first() == polygon.last())
		{
			m_shape_item->removePoints();
			m_shape_item->setClosed(true);
		}
		m_diagram->undoStack().push (new AddGraphicsObjectCommand(m_shape_item, m_diagram));
		m_shape_item = nullptr; //< set to nullptr for create new shape at next left clic
		event->setAccepted(true);
	}
}

void DiagramEventAddShape::init()
{
	foreach (QGraphicsView *v, m_diagram->views())
		v->setContextMenuPolicy(Qt::NoContextMenu);
}


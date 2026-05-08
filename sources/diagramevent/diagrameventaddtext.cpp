/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/

#include "diagrameventaddtext.h"

#include "../diagram.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../qetgraphicsitem/independenttextitem.h"

/**
	@brief DiagramEventAddText::DiagramEventAddText
	Default constructor
	@param diagram : the diagram where this event must operate
*/
DiagramEventAddText::DiagramEventAddText(Diagram *diagram) :
	DiagramEventInterface(diagram),
	m_text_item (nullptr),	// Achim DiagramEditor helpCross
	m_text_is_added (false)	// Achim DiagramEditor helpCross
{}

/**
	@brief DiagramEventAddText::~DiagramEventAddText
*/
DiagramEventAddText::~DiagramEventAddText()
{
	// Achim DiagramEditor helpCross
		// bei Abbruch den am HelpCross geführten Text wieder löschen
	if(m_running || m_abort){
		if(m_text_is_added){
			m_diagram->removeItem(m_text_item);
			delete m_text_item;
		}
	}

		// Achim DiagramEditor helpCross
		// ContextMenü auf default setzen
	foreach (QGraphicsView *view, m_diagram->views())
		view->setContextMenuPolicy((Qt::DefaultContextMenu));

		// Hilfs-CrossCursor löschen
	m_diagram->deleteHelpCross();	// Achim DiagramEditor helpCross
}

// Achim DiagramEditor helpCross
/**
	@brief DiagramEventAddText::mousePressEvent
	@param event : event of mouse press event.
*/
void DiagramEventAddText::mousePressEvent(QGraphicsSceneMouseEvent *event)// Achim DiagramEditor helpCross
{
	// Original Code
	/*if (event->button() == Qt::LeftButton)
	{
		IndependentTextItem *text = new IndependentTextItem();
		m_diagram->undoStack().push(new AddGraphicsObjectCommand(
			text,
			m_diagram,
			event->scenePos()));
		text->setTextInteractionFlags(Qt::TextEditorInteraction);
		text->setFocus(Qt::MouseFocusReason);
		emit finish();
		event->setAccepted(true);
	}*/

	if (m_text_item && event->button() == Qt::LeftButton)
	{
		//IndependentTextItem *text = new IndependentTextItem();	// Achim DiagramEditor helpCross
		m_diagram->undoStack().push(new AddGraphicsObjectCommand(
									m_text_item,
									m_diagram,
									// snapToGrid damit der Text auf dem Punkt abgesetzt
									// wo sich das Helpcross befindet
									// schöner wäre wohl wenn die Position zum setzen des
									// des Diagramtextes vom HelpCross kommen würde und
									// die scenePos().
									//event->scenePos()));
									//m_diagram->snapToGrid(event->scenePos()))); // Achim DiagramEditor helpCross
									m_pos));
		m_text_item->setTextInteractionFlags(Qt::TextEditorInteraction);
		m_text_item->setFocus(Qt::MouseFocusReason);

			// passendes Contextmenü setzen
		for (QGraphicsView *view : m_diagram->views()) {
			view->setContextMenuPolicy((Qt::DefaultContextMenu));
		}

		m_running = false;
		emit finish();
		event->setAccepted(true);
	}
	else if(m_text_item && event -> button() == Qt::RightButton){
		event->setAccepted(true);
	}
}

// Achim DiagramEditor helpCross
/**
	@brief DiagramEventAddText::mouseMoveEvent
	@param event
*/
void DiagramEventAddText::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
  //
	m_pos = m_diagram->updateHelpCross(event->scenePos());	// Achim DiagramEditor helpCross


	if(!m_text_item){
		//
		for (QGraphicsView *view : m_diagram->views()) {
			view->setContextMenuPolicy((Qt::NoContextMenu));
		}
		  // diagramTextitem erstellen
		m_text_item = new IndependentTextItem;
		m_text_item->setPos(m_pos);
		m_text_item->setSelected(true);
		m_diagram->addItem(m_text_item);
		m_text_is_added = true;
		m_running = true;

	}
	//m_diagram->updateHelpCross(event->scenePos()); // Achim DiagramEditor helpCross

		   // diagramTextItem am Cursor führen
	m_text_item->setPos(m_pos);

	event->setAccepted(true);

}


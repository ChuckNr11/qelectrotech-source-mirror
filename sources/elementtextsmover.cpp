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
#include "elementtextsmover.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "diagram.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/elementtextitemgroup.h"

#include <QObject>

/**
	@brief ElementTextsMover::ElementTextsMover
*/
ElementTextsMover::ElementTextsMover()
{}

/**
	@brief ElementTextsMover::isReady
	@return true if this ElementTextsMover is ready to process a new movement.
	False if this ElementTextsMover is actually process a movement
*/
bool ElementTextsMover::isReady() const
{
	return(!m_movement_running);
}

/**
	@brief ElementTextsMover::beginMovement
	Begin a movement
	@param diagram : diagram where the movement is apply
	@param driver_item : item moved by the mouse
	@return : the number of moved text (driver_item include), or -1 if this ElementTextsMover can't begin a movement
*/
int ElementTextsMover::beginMovement(Diagram *diagram, QGraphicsItem *driver_item)
{
	if (m_movement_running || !diagram)
		return(-1);

	m_diagram = diagram;
	m_movement_driver = driver_item;
	m_items_hash.clear();
	m_text_count = m_group_count =0;
	
	for(QGraphicsItem *item : diagram->selectedItems())
	{
		 if(item->type() == DynamicElementTextItem::Type)
		 {
			 m_items_hash.insert(item, item->pos());
			 m_text_count++;
		 }
		 else if(item->type() == QGraphicsItemGroup::Type)
		 {
			 if(dynamic_cast<ElementTextItemGroup *>(item))
			 {
				 m_items_hash.insert(item, item->pos());
				 m_group_count++;
			 }
		 }
	}
	
	
	if(m_items_hash.isEmpty())
		return -1;
	
	m_movement_running = true;
	m_first = true;	// Achim deti movement
	
	return m_items_hash.size();
}

void ElementTextsMover::continueMovement(QGraphicsSceneMouseEvent *event)
{
		// Achim deti movement
	/*######################################
		Variante 0 Qet0.9
		Variante 1 elevatormind  ( aktuelle Variante )
		Variante 2 Achim: bewegen in Rasterschritten mit shift-Taste
		Variante 3 Achim: bewegen in Rasterschritten
			- Rasterschritt
			- 1 Pixel Schritt
			- undo/redo bei jedem Wechsel des schrittmaßes
	//######################################*/
	/*
	if(!m_movement_running)
		return;

	for(QGraphicsItem *qgi : m_items_hash.keys())
	{
		if(qgi == m_movement_driver)
			continue;
		
		QPointF current_parent_pos;
		QPointF button_down_parent_pos;

		current_parent_pos = qgi->mapToParent(qgi->mapFromScene(event->scenePos()));
		button_down_parent_pos = qgi->mapToParent(qgi->mapFromScene(event->buttonDownScenePos(Qt::LeftButton)));
		
		QPointF new_pos = m_items_hash.value(qgi) + current_parent_pos - button_down_parent_pos;
		event->modifiers() == Qt::ControlModifier ? qgi->setPos(new_pos) : qgi->setPos(Diagram::snapToGrid(new_pos));
	}
	*/
	if(!m_movement_running)
		return;

	for(QGraphicsItem *qgi : m_items_hash.keys())
	{
		if(qgi == m_movement_driver)
			continue;

		if(m_first){
			m_button_down_parent_pos = qgi->mapToParent(qgi->mapFromScene(event->buttonDownScenePos(Qt::LeftButton)));
			m_first	= false;
		}

		QPointF new_pos;
		QPointF current_pos = qgi->mapToParent(qgi->mapFromScene(event->scenePos()));
																					  // Nur einmal nachdem die Ctrl-Taste losgelassen wurde
		if(!(event->modifiers() == Qt::ControlModifier) && m_ctrl){
			m_button_down_parent_pos = current_pos;
			m_ctrl = false;
		}

			   // Nur einmal nachdem die Ctrl-Taste gedrückt wurde
		if((event->modifiers() == Qt::ControlModifier) && !m_ctrl)
			m_button_down_parent_pos = current_pos;

			   // Position bei kleinen Schritten
		if(event->modifiers() == Qt::ControlModifier){
			new_pos = m_items_hash.value(qgi) + current_pos - m_button_down_parent_pos;
			qgi->setPos(qRound(new_pos.x()),qRound(new_pos.y()));
			m_ctrl = true;

				   // new-pos nur für qDebug unten
			//new_pos.setX(qRound(new_pos.x()));
			//new_pos.setY(qRound(new_pos.y()));
		}

			   // Position bei großen Rasterschritten
		else{
			QSettings settings;
			int grid_x = settings.value(QStringLiteral("diagrameditor/Xgrid"), Diagram::xGrid).toInt();
			int grid_y = settings.value(QStringLiteral("diagrameditor/Ygrid"), Diagram::yGrid).toInt();

				   // Differnz der neuen zur alten Position berechnen
			int p_x = qRound((current_pos.x()-m_button_down_parent_pos.x())/grid_x);
			int p_y = qRound((current_pos.y()-m_button_down_parent_pos.y())/grid_y);
																						 // Neue Position in großen Schritten
			qgi->setPos(m_items_hash.value(qgi).x() + p_x*grid_x, m_items_hash.value(qgi).y() + p_y*grid_y);
																												 // new-pos nur für qDebug unten
			//new_pos.setX(m_items_hash.value(qgi).x() + p_x*grid_x);
			//new_pos.setY(m_items_hash.value(qgi).y() + p_y*grid_y);
		}
		  // Nur für qDebug
		//QPointF item_pos = m_items_hash.value(qgi);
		//qDebug() << "mover"
		//		 << "item_pos" <<item_pos
		//		 << "current_pos" <<current_pos
		//		 << "button_down" <<m_button_down_parent_pos
		//		 << "new_pos" << new_pos;
	}
}

/**
	@brief ElementTextsMover::endMovement
	Finish the movement by pushing an undo command to the parent diagram of text item
*/
void ElementTextsMover::endMovement()
{	
		//No movement or no items to move
	if(!m_movement_running || m_items_hash.isEmpty())
		return;
			
		//Movement is null
	QGraphicsItem *qgi = m_items_hash.keys().first();
	if(qgi->pos() == m_items_hash.value(qgi))
		return;
					 
	QUndoCommand *undo = new QUndoCommand(undoText());
	
	for (QGraphicsItem *qgi : m_items_hash.keys())
	{
		if(QObject *object = dynamic_cast<QObject *>(qgi))
		{
			QPropertyUndoCommand *child_undo = new QPropertyUndoCommand(object, "pos", m_items_hash.value(qgi), qgi->pos(), undo);
			child_undo->enableAnimation();
		}
	}

	m_diagram->undoStack().push(undo);
	
	m_movement_running = false;
}

QString ElementTextsMover::undoText() const
{
	QString undo_text;
	
	if(m_text_count == 1)
		undo_text.append(QObject::tr("Déplacer un texte d'élément"));
	else if(m_text_count > 1)
		undo_text.append(QObject::tr("Déplacer %1 textes d'élément").arg(m_items_hash.size()));
	
	if(m_group_count >= 1)
	{
		if(undo_text.isEmpty())
			undo_text.append(QObject::tr("Déplacer"));
		else
			undo_text.append(QObject::tr(" et"));
		
		if(m_group_count == 1)
			undo_text.append(QObject::tr(" un groupe de texte"));
		else
			undo_text.append(QObject::tr((" %1 groupes de textes")).arg(m_group_count));
	}
	
	return undo_text;
}

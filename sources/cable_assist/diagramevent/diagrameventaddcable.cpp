/*
	Copyright 2006-2025 The QElectroTech Team
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
/*
 */
#include "diagrameventaddcable.h"
#include "../diagram.h"
#include "sources/conductorautonumerotation.h"
#include "sources/qetdiagrameditor.h"
#include "sources/qetgraphicsitem/conductor.h"
#include "sources/qetgraphicsitem/conductortextitem.h"
#include "../ui/cablepropertiesdialog.h"
#include "../factory/elementfactory.h"
#include "../qetgraphicsitem/element.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "sources/qetgraphicsitem/terminal.h"

DiagramEventAddCable::DiagramEventAddCable(Diagram *diagram):
	DiagramEventInterface(diagram),
	m_help_horiz (nullptr),
	m_help_verti (nullptr),
	m_selection_line (nullptr),
	m_collide_rect (nullptr),
	m_cable_line (nullptr),
	m_cabel_label (nullptr)
{
	m_running = true;
	init();
}

/**
	@brief DiagramEventAddCable::~DiagramEventAddCable
*/
DiagramEventAddCable::~DiagramEventAddCable()
{
		// delete graphics objects
	delete m_help_horiz;
	delete m_help_verti;

	delete m_selection_line;
	delete m_collide_rect;

		   // set default ContextMenü
	foreach (QGraphicsView *v, m_diagram->views())
		v->setContextMenuPolicy(Qt::DefaultContextMenu);
}

/**
	@brief DiagramEventAddCable::mousePressEvent
	Action when mouse is pressed
	@param event : event of mouse press
*/
void DiagramEventAddCable::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (Q_UNLIKELY(m_diagram->isReadOnly())) {
		return;
	}

	if (event->button() == Qt::LeftButton){
			// prepare selection
		m_selection_start_pos = Diagram::snapToGrid(event->scenePos()); 
		m_start_selection = true;
		m_cond_list.clear();
		m_cond_pos.clear();
		event->setAccepted(true);
		return;
	}

	if (event->button() == Qt::RightButton) {
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddCable::mouseMoveEvent
	Action when mouse move
	@param event : event of mouse move
*/
void DiagramEventAddCable::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	QPointF pos = Diagram::snapToGrid(event->scenePos());
	updateHelpCross(pos);


	if (m_start_selection && event->buttons() == Qt::LeftButton){

		drawSelectionLine(pos);
	}

	event->setAccepted(true);
}

/**
	@brief DiagramEventAddCable::mouseReleaseEvent
	Action when mouse button is released
	@param event : event of mouse release
*/
void DiagramEventAddCable::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::LeftButton ){

		m_start_selection=false;

		// if there is no selection
		if(m_cond_list.isEmpty()){

			  // clean graphics objects
			m_diagram->removeItem(m_selection_line);
			m_selection_line=nullptr;
			m_diagram->removeItem(m_collide_rect);
			m_collide_rect=nullptr;
			m_cable_line=nullptr;

			event->setAccepted(true);
			return;
		}

			   // call Dialog
		m_cable_data_.clear();
		m_cable_data_=CablePropertiesDialog::cableDialog(m_cable_data,diagramEditor(),m_cond_list.count());
		if (!m_cable_data_.isEmpty())
			makeCable();

			   // // clean graphics objects
		m_diagram->removeItem(m_selection_line);
		m_selection_line=nullptr;
		m_diagram->removeItem(m_collide_rect);
		m_collide_rect=nullptr;
		m_cable_line=nullptr;

			   // deselect condductors
		for(auto item :m_diagram->selectedItems())
			item->setSelected(false);

		event->setAccepted(true);
		//return;
	}

		   // stop event
	if (event->button() == Qt::RightButton){
		m_running = false;
		emit finish();
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddCable::mouseDoubleClickEvent
	Action when mouse button is double clicked
	@param event : event of mouse double click
*/
void DiagramEventAddCable::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event)
}

/**
	@brief DiagramEventAddCable::hoverEnterEvent
	Action when hovering an item starts
	@param event :
*/
void DiagramEventAddCable::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	Q_UNUSED(event)
}

/**
	@brief DiagramEventAddCable::hoverLeaveEvent
	Action when hovering an item ends
	@param event :
*/
void DiagramEventAddCable::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	Q_UNUSED(event)
}

void DiagramEventAddCable::keyPressEvent(QKeyEvent *event)
{
	Q_UNUSED(event)
}

void DiagramEventAddCable::keyReleaseEvent(QKeyEvent *event)
{
	Q_UNUSED(event)
}

/**
	@brief DiagramEventAddCable::init
	Init für den Constructor
*/
void DiagramEventAddCable::init()
{
		// init ContextMenü
	foreach (QGraphicsView *v, m_diagram->views())
		v->setContextMenuPolicy(Qt::NoContextMenu);

	connect(m_diagram, &QGraphicsScene::selectionChanged,this, &DiagramEventAddCable::getSelectedConductors);
}

/**
	@brief DiagramEventAddCable::updateHelpCross
	Create and update the position of the cross to help user for draw new shape
	@param p : the center of the cross
*/
void DiagramEventAddCable::updateHelpCross(const QPointF &p)
{
	QPen pen;
	pen.setWidthF(0.4);
	pen.setCosmetic(true);
	pen.setColor(Diagram::background_color == Qt::darkGray ? Qt::lightGray : Qt::darkGray);

		   //If line isn't created yet, we create it.
	if (!m_help_horiz || !m_help_verti){
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

		   //Update the position of the helpCross
	m_help_horiz->setPen(pen);
	m_help_horiz->setY(p.y());
	m_help_verti->setPen(pen);
	m_help_verti->setX(p.x());
}

/**
	@brief DiagramEventAddCable::drawSelectionLine
	zeichnet die Linie die zur Auswahl der Conductoren benutzt wird
	@param p  aktuelle Position des MouseMoveEvents
*/
void DiagramEventAddCable::drawSelectionLine(const QPointF &p)
{
  // design the Line
	QPen pen;
	pen.setWidthF(4);
	pen.setCosmetic(true);
	pen.setColor(Qt::blue);

	QPointF p1 = p;
		// Line only could moved horizontally or vertically
	abs(m_selection_start_pos.x()-p.x()) > abs(m_selection_start_pos.y()-p.y()) ? p1.setY(m_selection_start_pos.y()):p1.setX(m_selection_start_pos.x());

		//If the line does not yet exist (upon the first call), or if the Space key is pressed
		//to interrupt the selection line, a new selection line is started.
	if (!m_selection_line){
		m_selection_line = new QGraphicsLineItem(m_selection_start_pos.x(),m_selection_start_pos.y(),p.x(),p.y());
		m_selection_line->setPen(pen);
		m_diagram->addItem(m_selection_line);
	}

	if (!m_collide_rect){
		m_collide_rect = new QGraphicsRectItem(p.x()-1.0,p.y()-1.0,2.0,2.0);
		m_collide_rect->setPen(pen);
		m_diagram->addItem(m_collide_rect);
	}
	  // define line
	m_selection_line->setPen(pen);
		//                   |				   starpoint						 |	endpoint    |
	m_selection_line->setLine(m_selection_start_pos.x(),m_selection_start_pos.y(),p1.x(),p1.y());

		// Define a small selection rectangle, it is carried along at the movable point of the selection line.
		// The selection rectangle captures the conductor and the position of the line on the conductor
		// in `getSelectedConductors()`.
	m_collide_rect->setPen(pen);
		// Set the selection rectangle to the end position of the line.
	m_collide_rect->setRect(p1.x()-1.0,p1.y()-1.0,2.0,2.0);

		// The bounding rectangle of the line is to serve as the selection area;
		// all elements touched by this area will be selected.
	QRectF lineArea= m_selection_line->boundingRect();
		// create path (PainterPath)
	QPainterPath selection_area;
		// Pass the LineArea (the bounding rectangle of the line) to the path.
	selection_area.addPolygon(lineArea);
		// set the selection area in the scene
		// ReplaceSelektion: default, The currently selected items are replaced by
		//					 items in the selection area.
	m_diagram->setSelectionArea(selection_area,Qt::ReplaceSelection);

		// ### Alternativ ###
		// AddToSelection: default, Elemente werden entsprechend hinzugefügt
		// m_diagram->setSelectionArea(selection_area,Qt::AddToSelection);
}

/**
	@brief DiagramEventAddCable::getSelectedConductors
	Solange die Linie aktiv ist werdent die Conductoren und die Position der Linie auf dem Conductor
	erfasst.
*/
void DiagramEventAddCable::getSelectedConductors()
{
	// Warning: QGraphicsScene::collidingItems: cannot find collisions for null item
	if (!m_collide_rect)
		return;

			// List of all elements currently encompassed by the selection rectangle.
	QList<QGraphicsItem *> collidedItemens = m_diagram->collidingItems(m_collide_rect);
			// for every element
	for(auto qgi : collidedItemens){
			// save conductor and conductor position
		if(qgi->type() == Conductor::Type){
			auto cond = static_cast<Conductor *>(qgi);
			m_cond_list << cond;

			QPointF pos = m_diagram->snapToGrid(m_collide_rect->rect().topLeft());
			m_cond_pos << pos;
				// If multiple conductors overlap, cancel selection.
			/*
				If multiple conductors overlap, we must select the specific conductor
				onto which the cable symbol is to be placed.
				Selecting it programmatically strikes me as somewhat complex.
				Therefore, I show aMessageBox and terminate the function.
				An alternative would be to display the conductors in a dialog box,
				allowing for a manual selection.
			*/
			if(pos == m_last_cond_pos){
				QMessageBox msgBox;
				msgBox.setText("Es liegen mehrere Verbindungen üereinander, \n"
					"beheben Sie das Problem und starten Sie die\n"
					"Funktion erneut");
				msgBox.exec();
				m_running = false;
				emit finish();
				// deselect conductors
				for(auto item :m_diagram->selectedItems())
					item->setSelected(false);
				return;
			}
			m_last_cond_pos = pos;
		}
	}
		// If the direction of movement of the selection line is reversed,
		// the selection of the conductors is undone, and the
		// deselected conductor must be removed from the list again.
	int selectedItems=m_diagram->selectedItems().count();
	if(selectedItems < m_cond_list.size())
			// After reversing the movement, the last conductor is
			// added a second time; therefore, the last two conductors are removed.
		for(int i=0;i<=1;++i)
			m_cond_list.removeAt(m_cond_list.size()-1);

}

QETDiagramEditor* DiagramEventAddCable::diagramEditor() const
{
	if (!m_diagram)                     return nullptr;
	if (m_diagram -> views().isEmpty()) return nullptr;

	QWidget *w = const_cast<QGraphicsView *>(m_diagram -> views().at(0));
	while (w -> parentWidget() && !w -> isWindow()) {
		w = w -> parentWidget();
	}
	return(qobject_cast<QETDiagramEditor *>(w));
}

void DiagramEventAddCable::makeCable()

{
	// erste Variante, damit überhaupt was gezeichnet wird.
	//m_group = new QGraphicsItemGroup;
	QPointF p1,p2;
	p1 = m_cond_pos.first();
	p1.setX(p1.x()-5);
	p2 = m_cond_pos.last();
	p2.setX(p2.x()+5);
	if(!m_cable_line){
		m_cable_line = new QetShapeItem(p1,p2, m_shapeType);
		//m_group->addToGroup(m_cable_line);
	}

	/*for(int i=0; i<m_cond_pos.count(); i++){
		QPointF p1 = m_cond_pos[i];

		  m_cabel_label = new IndependentTextItem();
		  m_cabel_label->setPlainText(QString::number(i+1));

			p1.setX(p1.x()+5.0);
			p1.setY(p1.y()-5.0-m_cabel_label->boundingRect().height());
			m_cabel_label->setPos(p1);
			m_group->addToGroup(m_cabel_label);
		}

	  m_cabel_label = new IndependentTextItem();
	  m_cabel_label->setPos(p1 += QPointF(0, -(m_cabel_label->boundingRect().height())+5));
	  m_cabel_label->setAlignment(Qt::AlignTop | Qt::AlignRight);
	  m_group->addToGroup(m_cabel_label);;
	  m_cabel_label->setPlainText(m_cable_data_[7]);

		m_diagram->addItem(m_group);*/
	m_diagram->addItem(m_cable_line);

	if(m_cable_data_.count()>10){
		for(int i=10;i<m_cable_data_.count();i++){

				// element path
			ElementsLocation loc ("custom://symbole/w/w_kabel-1ader.elmt");
			ElementsLocation import_loc = m_diagram->project()->importElement(loc);
			if (import_loc.exist()) {
				m_integrate_path = import_loc.projectCollectionPath();
			}
			else {
				qDebug() << "DiagramView::addDroppedElement : Impossible d'ajouter l'element.";
				return;
			}

				// create element
			int state;
			Element *element;

			element = ElementFactory::Instance() -> createElement(m_integrate_path, nullptr, &state);
				// Bei Fehler beim erstellen -> abbrechen
			if (state){
				delete element;
				return;
			}

				//We must add item to scene (even if addItemCommand do this)
				//for create the autoconnection below
			element -> setPos(m_cond_pos[i-10]);
			element -> setRotation(element -> rotation());
					// Element beschriften
			DiagramContext dc = element->elementInformations();
			dc.addValue("label", m_cable_data_.value(6));
			dc.addValue("plant", m_cable_data_.value(4));
			dc.addValue("location", m_cable_data_.value(5));
			dc.addValue("function", m_cable_data_.value(0));	// function = Kabeltyp
			dc.addValue("description", m_cable_data_.value(1)+"x"+m_cable_data_.value(2)+m_cable_data_.value(3));	// Textual description =
			dc.addValue("designation", m_cable_data_.value(i));	// Order Number = Aderbezeichnung
			dc.addValue("machine_manufacturer_reference", QString::number(i-9));	// interne NUmmer
			dc.addValue("unity", m_cable_data_.value(3));	// Einheit
			element->setElementInformations(dc);

			m_diagram -> addItem(element);

			QUndoCommand *undo_object = new QUndoCommand(tr("Ajouter %1").arg(element->name()));
			new AddGraphicsObjectCommand(element, m_diagram, element -> pos(), undo_object);

				//When we search for free aligned terminal we temporally remove m_element to
				//avoid any interaction with the function Element::AlignedFreeTerminals
				//This is useful when an element has two (or more) terminals on opposite sides,
				//because m_element is exactly at the same pos of the new element
				//added to the scene so new conductor are created between terminal of the new element
				//and the opposite terminal of m_element.
			m_diagram->removeItem(element);
				// create new Conductors
				// Array for new conductor terminals
			QPair <Terminal *, Terminal *> pair;
				// original conductor terminals
			QList<Terminal *> cond_term;
			cond_term << m_cond_list[i-10]->terminal1 << m_cond_list[i-10]->terminal2;
			QList<Terminal *> elem_term = element->terminals();

			while(!cond_term.isEmpty()){
				Terminal *terminal= cond_term.takeFirst();
				pair.first=terminal;
				switch(terminal->orientation()){
				case Qet::North:
					for(int i=0;i<elem_term.count();i++){
						if(elem_term[i]->orientation()==Qet::South)
							pair.second=elem_term[i];
					}
					break;

				case Qet::East:
					for(int i=0;i<elem_term.count();i++){
						if(elem_term[i]->orientation()==Qet::West)
							pair.second=elem_term[i];
					}
					break;
				case Qet::South:
					for(int i=0;i<elem_term.count();i++){
						if(elem_term[i]->orientation()==Qet::North)
							pair.second=elem_term[i];
					}
					break;
				case Qet::West:
					for(int i=0;i<elem_term.count();i++){
						if(elem_term[i]->orientation()==Qet::East)
							pair.second=elem_term[i];
					}
					break;
				} // switch

				Conductor *new_conductor = new Conductor(pair.first, pair.second);
				new AddGraphicsObjectCommand(new_conductor, m_diagram, QPointF(), undo_object);

					   //Autonum the new conductor, the undo command associated for this, have for parent undo_object
				ConductorAutoNumerotation can  (new_conductor, m_diagram, undo_object);
				can.numerate();
				if (m_diagram->freezeNewConductors() || m_diagram->project()->isFreezeNewConductors()) {
					new_conductor->setFreezeLabel(true);
				}
			}	//while

			//m_diagram->addItem(element);

			if(i>10){
				for(DynamicElementTextItem *deti: element->dynamicTextItems()){
					if(!(deti->infoName()=="designation")){
						deti->parentElement()->removeDynamicTextItem(deti);
					}
				}
			}

			m_diagram -> undoStack().push(undo_object);
			element->freezeNewAddedElement();
		}	//for
	}	// if

		   // remove old conductors
	while(!m_cond_list.isEmpty())
		m_diagram->removeItem(m_cond_list.takeFirst());
}

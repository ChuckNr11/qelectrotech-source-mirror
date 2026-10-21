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
#include "diagrameventaddterminal.h"

#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../autobreakconductor.h"
#include "../../conductorautonumerotation.h"
#include "../../diagram.h"
#include "../../factory/elementfactory.h"
#include "../../qetapp.h"
#include "../../qetdiagrameditor.h"
#include "../../qetgraphicsitem/conductor.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetgraphicsitem/terminal.h"
#include "../../qetgraphicsitem/terminalelement.h"
#include "../../qetproject.h"
#include "../../undocommand/addgraphicsobjectcommand.h"
#include "../UndoCommand/bindterminalcommand.h"

#include <QMimeData>
#include <QSet>
#include <QStringList>

#include <utility>

/**
	@brief DiagramEventAddTerminal::DiagramEventAddTerminal
	@param pendings : the terminals of the strips which are not drawn
	in any folio yet, in the order they are drawn in
	@param diagram : diagram owner of this event
	@param pos : first pos of the symbol (optional, by default QPointF(0,0))
*/
DiagramEventAddTerminal::DiagramEventAddTerminal(
		const QVector<QSharedPointer<RealTerminal>> &pendings,
		Diagram *diagram,
		QPointF pos) :
	DiagramEventInterface(diagram),
	m_queue(pendings),
	m_element(nullptr)
{
		//Nothing is built when one of the terminals can not be drawn
		//here: it stopped waiting for its symbol, its strip is gone or
		//belongs to another project (the strip would not follow, the
		//folio would not save with it). m_running stays false, which is
		//what the diagram goes by to delete a tool which never started
		//(@sa Diagram::setEventInterface). The drop handler says why
		//before this event is even built, these checks are the ones
		//which make a refused construction simply not run.
		//A run is started as a whole or not at all: one terminal like
		//this one refuses every one of them, rather than drawing part
		//of the selection and leaving the rest of it unexplained.
	if (m_queue.isEmpty()) {
		return;
	}
	for (const auto &pending : std::as_const(m_queue)) {
		if (!checkPending(pending)) {
			return;
		}
	}

	m_pending = m_queue.constFirst();
		//The location of the symbol, bound to the project of the strip
		//the terminal lives in: the saved string carries no usable
		//session id anymore, and a location built from it alone would be
		//empty in every session but the one which wrote it
		//(@sa RealTerminal::symbolWithoutProjectId)
	m_location = m_pending->symbolLocation();
	if (!(m_location.isElement() && m_location.exist())) {
		qDebug() << "DiagramEventAddTerminal, location can't be used :" << m_location;
		return;
	}

	if (buildElement())
	{
		init();
		m_element -> setPos(pos);
		m_element -> displayHelpLine(true);
		m_element -> setFlag(QGraphicsItem::ItemIsSelectable, false);
		m_diagram -> addItem(m_element);
		m_running = true;

		if (!diagram->views().isEmpty()) {
			const auto qde = QETApp::diagramEditorAncestorOf(diagram->views().at(0));
			if (qde) {
				m_status_bar = qde->statusBar();
			}
		} else {
			m_status_bar.clear();
		}
	}
}

/**
	@brief DiagramEventAddTerminal::~DiagramEventAddTerminal
	Destructor
	Enable context menu for each view of diagram
*/
DiagramEventAddTerminal::~DiagramEventAddTerminal()
{
	if (m_element)
	{
		if (m_element->scene() == m_diagram.data()) {
			m_diagram->removeItem(m_element);
		}
		m_element->deleteLater();
	}

	if (m_status_bar) {
		m_status_bar->clearMessage();
	}

	if (m_menus_taken) {
		for (auto view : m_diagram->views())
			view -> setContextMenuPolicy(Qt::DefaultContextMenu);
	}
}

/**
	@brief DiagramEventAddTerminal::mouseMoveEvent
	Move the symbol to new pos of mouse
	the event is always accepted
	@param event
*/
void DiagramEventAddTerminal::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_element)
	{
		const auto pos_{Diagram::snapToGrid(event->scenePos())};
		m_element->setPos(pos_);

		if (m_status_bar) {
			m_status_bar->showMessage(QString("x %1 : y %2").arg(QString::number(pos_.x()), QString::number(pos_.y())));
		}
	}
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddTerminal::mousePressEvent
	Do nothing, but accept the event for not transit it to other thing in diagram.
	@param event
*/
void DiagramEventAddTerminal::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	event->setAccepted(true);
}

/**
	@brief DiagramEventAddTerminal::mouseReleaseEvent
	Right button gives the run up (isRunning = false) and emits finish:
	the terminals already drawn stay on the folio, the others keep
	waiting for their symbol in the navigator.
	Left button draws the terminal the cursor holds: the symbol is
	placed and bound to its pending terminal, and the next one of the
	run -- if there is still one -- takes the cursor; the event is done
	with all of them once the last one is placed.
	the event is always accepted
	@param event
*/
void DiagramEventAddTerminal::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	event->setAccepted(true);

	if (!m_element) {
		return;
	}

	if (event->button() == Qt::RightButton)
	{
		endPlacement();
		return;				//'this' is deleted by endPlacement()
	}
	else if (event->button() == Qt::LeftButton)
	{
		addElement();		//draws it, and the next of the run takes the cursor
	}
}

/**
	@brief DiagramEventAddTerminal::mouseDoubleClickEvent
	Left double click gives the placement up the right click does.
	the event is always accepted
	@param event
*/
void DiagramEventAddTerminal::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (event -> button() == Qt::LeftButton)
	{
		endPlacement();
		event->setAccepted(true);
		return;				//'this' is deleted by endPlacement()
	}

	event->setAccepted(true);
}

/**
	@brief DiagramEventAddTerminal::keyPressEvent
	Press space key rotate the symbol to 90°.
	Every other key goes to the base class, where Escape gives the run
	up the way a right click does: the terminals already drawn stay on
	the folio, the others keep waiting in the navigator (@sa
	DiagramEventInterface::keyPressEvent).
	@param event
*/
void DiagramEventAddTerminal::keyPressEvent(QKeyEvent *event)
{
	if (m_element && event->key() == Qt::Key_Space)
	{
		m_element->setRotation(m_element->rotation() + 90);
		event->setAccepted(true);
	}
	else {
		DiagramEventInterface::keyPressEvent(event);
	}
}

/**
	@brief DiagramEventAddTerminal::init
	Init this event.
*/
void DiagramEventAddTerminal::init()
{
	foreach(QGraphicsView *view, m_diagram->views())
		view->setContextMenuPolicy(Qt::NoContextMenu);
	m_menus_taken = true;
}

/**
	@brief DiagramEventAddTerminal::pendingTerminalMime
	@return the mime format of a drag which carries a terminal of a strip
	which is not drawn in any folio yet
*/
QString DiagramEventAddTerminal::pendingTerminalMime() {
	return QStringLiteral("application/x-qet-pending-terminal");
}

/**
	@brief DiagramEventAddTerminal::pendingTerminalPayload
	Build what @a strip_uuid and @a terminal_uuid are carried in: two
	uuids the drop handler resolves through the project, which is where
	both are known -- the terminal itself can not be carried by pointer,
	the tree holding it is rebuilt by every reload.
	@param strip_uuid
	@param terminal_uuid
	@return the payload for pendingTerminalMime()
*/
QByteArray DiagramEventAddTerminal::pendingTerminalPayload(const QUuid &strip_uuid,
														   const QUuid &terminal_uuid)
{
	return (strip_uuid.toString() + QLatin1Char(';') + terminal_uuid.toString()).toUtf8();
}

/**
	@brief DiagramEventAddTerminal::pendingTerminalsPayload
	Build what a whole run of terminals is carried in: one line of
	pendingTerminalPayload() per terminal, the lines in the order the
	terminals have to be drawn in. A drag of a single terminal is
	exactly one line, which is all a single-terminal payload ever was.
	@param terminals : the strips and the terminals, both by uuid,
	in the order of the run
	@return the payload for pendingTerminalMime()
*/
QByteArray DiagramEventAddTerminal::pendingTerminalsPayload(
		const QVector<QPair<QUuid, QUuid>> &terminals)
{
	QStringList lines;
	lines.reserve(terminals.size());
	for (const auto &terminal : terminals) {
		lines.append(terminal.first.toString()
					 + QLatin1Char(';')
					 + terminal.second.toString());
	}
	return lines.join(QLatin1Char('\n')).toUtf8();
}

/**
	@brief DiagramEventAddTerminal::readPendingTerminalsMime
	Read what pendingTerminalsPayload() wrote: every line of the
	payload, in the order they come in -- which is the order of the
	selection in the navigator, and the order the run draws them in.
	@param mime
	@param terminals : receives the strips and terminals as they were
	written (may be null when only the validity is of interest)
	@return true when @a mime carries at least one terminal to be drawn
*/
bool DiagramEventAddTerminal::readPendingTerminalsMime(
		const QMimeData *mime,
		QVector<QPair<QUuid, QUuid>> *terminals)
{
	if (!mime || !mime->hasFormat(pendingTerminalMime())) {
		return false;
	}

	const auto lines = QString::fromUtf8(mime->data(pendingTerminalMime()))
			.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
	if (lines.isEmpty()) {
		return false;
	}

		//Every line has to be one terminal of one strip: a payload with
		//a line worth nothing in it is no terminal drag at all, whoever
		//wrote it -- half of a run can not be read back, and placing it
		//would draw an order nobody asked for.
	QVector<QPair<QUuid, QUuid>> read_;
	read_.reserve(lines.size());
	for (const auto &line : std::as_const(lines)) {
		const auto parts = line.split(QLatin1Char(';'));
		if (parts.size() != 2) {
			return false;
		}
		const QUuid strip_{parts.at(0)};
		const QUuid terminal_{parts.at(1)};
		if (strip_.isNull() || terminal_.isNull()) {
			return false;
		}
		read_.append(qMakePair(strip_, terminal_));
	}

	if (terminals) {
		*terminals = read_;
	}
	return true;
}

/**
	@brief DiagramEventAddTerminal::readPendingTerminalMime
	Read the first terminal of what pendingTerminalsPayload() wrote.
	@param mime
	@param strip_uuid : receives the uuid of the strip
	@param terminal_uuid : receives the uuid of the terminal
	@return true when @a mime carries a terminal to be drawn
*/
bool DiagramEventAddTerminal::readPendingTerminalMime(const QMimeData *mime,
													  QUuid *strip_uuid,
													  QUuid *terminal_uuid)
{
		//A single terminal is the first line of a run, and the payload
		//of a lone terminal has always been exactly that one line --
		//so this reads back what it wrote and a run as well.
	QVector<QPair<QUuid, QUuid>> read_;
	if (!readPendingTerminalsMime(mime, &read_)) {
		return false;
	}

	if (strip_uuid) {
		*strip_uuid = read_.constFirst().first;
	}
	if (terminal_uuid) {
		*terminal_uuid = read_.constFirst().second;
	}
	return true;
}

/**
	@brief DiagramEventAddTerminal::checkPending
	@param pending : the terminal to check
	@return true when @a pending is still one which waits for its
	symbol, in a strip of the project this folio belongs to
*/
bool DiagramEventAddTerminal::checkPending(
		const QSharedPointer<RealTerminal> &pending) const
{
	if (pending.isNull() || !pending->isPending()) {
		return false;
	}

	auto *strip_ = pending->parentStrip();
	if (!strip_) {
		return false;
	}

		//A terminal of another project has no business on this folio:
		//the strip would not follow it, the folio would not save with it
	return strip_->project() == m_diagram->project();
}

/**
	@brief DiagramEventAddTerminal::buildElement
	Build the symbol of the pending terminal, imported into the project's
	own collection the way a drop from the elements panel does it, so
	that the element saved with the project is the one the strip names.
	@return true if the symbol is built successfully
*/
bool DiagramEventAddTerminal::buildElement()
{
	ElementsLocation import_loc = m_diagram->project()->importElement(m_location);
	if (import_loc.exist()) {
		m_integrate_path = import_loc.projectCollectionPath();
	}
	else {
		qDebug() << "DiagramEventAddTerminal : Impossible to add the symbol "
					"of the terminal to the project :" << m_location;
		return false;
	}

	int state;
	ElementsLocation loc(m_integrate_path);
	m_element = ElementFactory::Instance() -> createElement(loc, nullptr, &state);
		//The creation of element failed, we delete it. The pointer is
		//cleared as well: unlike the sibling event, whose destructor
		//would keep the failed construction's pointer, this one is only
		//deleted later, when the diagram gets rid of a tool which never
		//started, and a dangling m_element would be removed from the
		//scene there.
	if (state) {
		delete m_element;
		m_element = nullptr;
		return false;
	}
		//The symbol of a terminal has to be a terminal: anything else
		//would sit on the folio and be found in no strip afterwards
	if (m_element->elementData().m_type != ElementData::Terminal) {
		delete m_element;
		m_element = nullptr;
		return false;
	}

		//Everything is good
	return true;
}

/**
	@brief DiagramEventAddTerminal::addElement
	Draw the symbol at the current pos and current rotation, and bind it
	to its pending terminal: one single undo macro takes the symbol off
	the folio and gives that pending terminal its place back.
	If the project autoconductor option is enabled, and the symbol can be
	wired, we do it.

	One click, one terminal, one macro: when the run holds several
	terminals, advance() loads the next one onto the cursor afterwards,
	or ends the event when this one was the last.
*/
void DiagramEventAddTerminal::addElement()
{
		//The terminal stopped waiting while this event ran: the strip
		//was undone, its terminal drawn by another way... Nothing can
		//be bound anymore, so this terminal is passed over -- it stays
		//where it is, and the run carries on with the next one (or is
		//done when there is none, which is the whole run of one).
	if (!checkPending(m_pending)) {
		showMessage(m_pending.isNull()
					? tr("This terminal does not wait for its symbol anymore")
					: tr("The terminal %1 does not wait for its symbol anymore")
					  .arg(m_pending->label()));
		advance();
		return;				//'this' may be deleted by advance()
	}

	int state;
	Element *element;
	if (m_integrate_path.isEmpty())
		element = ElementFactory::Instance() -> createElement(m_location, nullptr, &state);
	else
		element = ElementFactory::Instance() -> createElement(ElementsLocation(m_integrate_path), nullptr, &state);

		//Build failed: keep the placement, the user can click again
	if (state)
	{
		delete element;
		return;
	}
		//The symbol built once is a terminal, this one has to be too
		//(same location, same result -- kept as the safety net a second
		//creation path needs)
	if (element->elementData().m_type != ElementData::Terminal)
	{
		delete element;
		showMessage(tr("The symbol of the terminal %1 is not a terminal symbol")
					.arg(m_pending->label()));
		advance();
		return;				//'this' may be deleted by advance()
	}

		//We must add item to scene (even if addItemCommand do this)
		//for create the autoconnection below
	element -> setPos(m_element->pos());
	element -> setRotation(m_element -> rotation());
	m_diagram -> addItem(element);

	QUndoCommand *undo_object = new QUndoCommand(
				tr("Draw the terminal %1").arg(m_pending->label()));
	new AddGraphicsObjectCommand(element, m_diagram, m_element -> pos(), undo_object);

	//When we search for free aligned terminal we temporally remove m_element to
	//avoid any interaction with the function Element::AlignedFreeTerminals
	//This is useful when an element has two (or more) terminals on opposite sides,
	//because m_element is exactly at the same pos of the new element
	//added to the scene so new conductor are created between terminal of the new element
	//and the opposite terminal of m_element.
	m_diagram->removeItem(m_element);

		//Auto break conductor: if a terminal of the new element lies on an existing
		//conductor, break the conductor and reconnect through the new element's terminal.
		//Track the endpoints of broken conductors so auto-connect doesn't create duplicates.
	QList<Conductor *> conductors_handled;
	QSet<Terminal *> used_terminals;
	QSet<Terminal *> broken_endpoints = autoBreakConductors(m_diagram, element, undo_object,
							    conductors_handled, used_terminals);

		//Auto-connect: collect all aligned pairs first, then filter and process.
	QList<QPair<Terminal *, Terminal *>> aligned_pairs;
	if (m_diagram->project()->autoConductor())
		aligned_pairs = element->AlignedFreeTerminals();

	for (const QPair<Terminal *, Terminal *> &pair : aligned_pairs)
	{
			//Skip if the other terminal was an endpoint of a broken conductor
		if (broken_endpoints.contains(pair.second))
			continue;

		Conductor *conductor = new Conductor(pair.first, pair.second);
		new AddGraphicsObjectCommand(conductor, m_diagram, QPointF(), undo_object);

			//Autonum the new conductor, the undo command associated for this, have for parent undo_object
		ConductorAutoNumerotation can  (conductor, m_diagram, undo_object);
		can.numerate();
		if (m_diagram->freezeNewConductors() || m_diagram->project()->isFreezeNewConductors()) {
			conductor->setFreezeLabel(true);
		}
	}

		//The bind comes after the placement in the macro: one undo first
		//takes the terminal out of the strip, then the symbol off the
		//folio, which is the order the two belong to.
		//No element->setUpFormula() here, unlike the sibling event: the
		//label of this element is the one of the terminal strip, typed
		//or numbered there, and the symbol's own numbering would keep
		//computing another one (@sa Element::actualLabel()).
	new BindTerminalCommand(m_pending, static_cast<TerminalElement *>(element), undo_object);

	m_diagram -> undoStack().push(undo_object);
	element->freezeNewAddedElement();

		//This terminal is drawn: the symbol hangs on the folio and
		//stays there, its macro is in the undo stack -- advance() drops
		//the symbol which followed the cursor and either loads the next
		//terminal of the run onto it or ends the event.
	advance();
}

/**
	@brief DiagramEventAddTerminal::advance
	Give up the symbol which followed the cursor and load the next
	terminal of the run: its own symbol takes the place of the previous
	one, at the very position and rotation the user left it at.

	A terminal whose symbol can not be built anymore (it was taken out
	of the collections while the run was on...) is left waiting where it
	is -- undrawn, exactly like the terminals of a run given up -- and
	the run carries on with the next one.

	When nothing is left to place, endPlacement() drops whatever remains
	of the cursor symbol and emits finish(): the diagram deletes this
	object during that emission, so nothing may be touched after it.
*/
void DiagramEventAddTerminal::advance()
{
		//Where the user was, and how the symbol they were placing was
		//turned: the next terminal of the run picks both up, the way a
		//placement keeps them while it loads the next symbol.
	QPointF pos_;
	qreal rotation_ = 0.0;
	if (m_element)
	{
		pos_ = Diagram::snapToGrid(m_element->pos());
		rotation_ = m_element->rotation();
		if (m_element->scene() == m_diagram.data()) {
			m_diagram->removeItem(m_element);
		}
		m_element->deleteLater();
		m_element = nullptr;
	}

	while (true)
	{
		++m_current_index;
		if (m_current_index >= m_queue.size()) {
			break;			//Every terminal of the run is dealt with
		}

		m_pending = m_queue.at(m_current_index);
			//The symbol of the next terminal of the run, bound to its
			//own project the same way the first one was
			//(@sa RealTerminal::symbolLocation)
		m_location = m_pending->symbolLocation();
		m_integrate_path.clear();
		if (buildElement()) {
			m_element->setPos(pos_);
			m_element->setRotation(rotation_);
			m_element->displayHelpLine(true);
			m_element->setFlag(QGraphicsItem::ItemIsSelectable, false);
			m_diagram->addItem(m_element);
			return;			//The next terminal follows the cursor
		}

			//This one has nothing to be drawn with right now: it keeps
			//waiting for its symbol in the navigator, and the run goes
			//on without it -- the same thing a terminal passed over in
			//addElement() gets, only found out here instead of on the
			//click of the user.
		showMessage(tr("The symbol of the terminal %1 can not be added to this project")
					.arg(m_pending->label()));
	}

	endPlacement();			//Nothing left to place: 'this' is deleted here
}

/**
	@brief DiagramEventAddTerminal::showMessage
	Say @a message in the status bar of the editor this diagram is shown
	in, if any -- a dialog would be far much for a drop which simply did
	not start, and there is no view at all when this event is built by a
	test.
	@param message
*/
void DiagramEventAddTerminal::showMessage(const QString &message)
{
	if (m_status_bar) {
		m_status_bar->showMessage(message, 5000);
	}
}

/**
	@brief DiagramEventAddTerminal::endPlacement
	Give the placement up: the symbol which followed the cursor is
	dropped, the event stops and the diagram deletes it (@sa
	DiagramEventInterface::finish).
	Nothing is touched after emitting finish(): the diagram's slot
	deletes this very object during the emission.
*/
void DiagramEventAddTerminal::endPlacement()
{
	if (m_element)
	{
		if (m_element->scene() == m_diagram.data()) {
			m_diagram->removeItem(m_element);
		}
		m_element->deleteLater();
		m_element = nullptr;
	}

	m_running = false;
	emit finish();
}

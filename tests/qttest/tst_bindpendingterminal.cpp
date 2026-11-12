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
#include "qetapptest.h"

#include "ElementsCollection/xmlelementcollection.h"
#include "TerminalNavigator/diagramevent/diagrameventaddterminal.h"
#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "diagram.h"
#include "diagramview.h"
#include "qetapp.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"

#include <QDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QUndoStack>

/**
	@brief The tst_bindpendingterminal class
	Drawing a terminal of a strip on a folio: the drag out of the
	terminal navigator carries the terminal which is not drawn yet, the
	drop starts the placement of its own symbol, and the click which
	drops the symbol binds it to the terminal -- one single undo macro
	which takes the symbol off the folio and gives the pending terminal
	its place back.
*/
class tst_bindpendingterminal : public QObject
{
	Q_OBJECT

private slots:
	void drawsThePendingTerminalOnAFolio();
	void startsThePlacementFromADrop();
	void placesATerminalWhoseSymbolCarriesAnotherProjectId();
	void refusesADropWhichHasNothingToDraw();
	void readsBackItsOwnPayload();
	void drawsASelectionOfTerminalsInOrder();
	void abortsWithEscape();
	void abortsARunWithARightClick();
};

	//A view which hands the drops over the way a drag out of the
	//navigator reaches them: both handlers are protected in the view,
	//and a drop is nothing the test can send through the window system
	//on an offscreen platform. keyPressEvent() is exposed for Escape,
	//which reaches the running tool through the very same view.
class DropView : public DiagramView
{
	public:
		using DiagramView::DiagramView;
		using DiagramView::dragEnterEvent;
		using DiagramView::dropEvent;
		using DiagramView::keyPressEvent;
};

	//A project registered with the application, the way an open project
	//is: only then the location strings of its embedded symbols read
	//back as locations of this very project (@sa ElementsLocation), and
	//a terminal can be drawn with them. The project does not unregister
	//itself when it is destroyed, so the helper does it before it goes.
class RegisteredProject
{
	public:
		explicit RegisteredProject(QETProject &project) : m_project(project)
		{
			QVERIFY2(QETApp::registerProject(&m_project),
					 "the project is registered with the application");
		}
		~RegisteredProject()
		{
			QETApp::unregisterProject(&m_project);
		}

	private:
		QETProject &m_project;
};

	//The location string of a terminal symbol of the project itself:
	//read the way the strip dialog reads the symbol it offers, the
	//first terminal of the embedded collection of the project. The
	//symbol of a terminal element of a folio is worth nothing here --
	//such an element carries no location of its own once placed.
static QString someTerminalSymbol(QETProject &project)
{
	auto *collection = project.embeddedElementCollection();
	if (!collection) {
		return QString();
	}

	const auto definitions = collection->root().elementsByTagName(
				QStringLiteral("definition"));
	for (int i = 0 ; i < definitions.count() ; ++i) {
		const auto definition = definitions.at(i).toElement();
		if (definition.attribute(QStringLiteral("link_type"))
				!= QLatin1String("terminal")) {
			continue;
		}
		const auto location = collection->domToLocation(
					definition.parentNode().toElement());
		if (!location.isNull()) {
			return location.toString();
		}
	}

	return QString();
}

	//Every terminal element of @p folio, drawn in a strip or not
static int terminalElementsOf(Diagram *folio)
{
	int count = 0;
	for (const auto &element : folio->elements()) {
		if (element->elementData().m_type == ElementData::Terminal) {
			++count;
		}
	}
	return count;
}

/**
	The click which drops the symbol of a pending terminal draws it and
	binds it to that terminal, in one single undo macro: the terminal of
	the strip is the one of the element just placed, it carries the label
	the strip numbered it with, and the element took over what the strip
	editor had edited for the terminal -- its type here, since the label
	is checked through the terminal itself.

	One undo takes the symbol off the folio and gives the pending
	terminal its place back, at the very same position of the strip,
	data included, and a redo binds it again.
*/
void tst_bindpendingterminal::drawsThePendingTerminalOnAFolio()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);
	if (symbol_.isEmpty()) {
		QSKIP("the test project holds no free terminal to take a symbol from");
	}

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	auto pending = RealTerminal::createPending(QStringLiteral("X999:1"), symbol_);
	QVERIFY(pending);
		//The row of the terminal in the strip editor, edited before the
		//terminal is drawn: what the element has to take over
	auto data = pending->spareData();
	data.setTerminalType(ElementData::TTFuse);
	pending->setSpareData(data);
	QVERIFY(strip->addTerminal(pending));
	QCOMPARE(strip->realTerminals().count(), 1);

	const int terminals_before = terminalElementsOf(folio);
	const int undo_index_before = project.undoStack()->index();

	const QVector<QSharedPointer<RealTerminal>> run{pending};
	auto *event = new DiagramEventAddTerminal(run, folio, QPointF(10, 20));
	QVERIFY2(event->isRunning(), "the symbol is built and the placement started");
	folio->setEventInterface(event);
	QVERIFY(folio->eventInterfaceIsRunning());

		//A left click drops the symbol: the very call the click of the
		//user reaches through the folio. The event emits finish() with
		//it, and the folio deletes it during the emission -- nothing of
		//it may be touched anymore after this line.
	QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
	release.setButton(Qt::LeftButton);
	release.setScenePos(QPointF(10, 20));
	event->mouseReleaseEvent(&release);
	QVERIFY2(!folio->eventInterfaceIsRunning(),
			 "one click draws the terminal, the event is done with it");

		//The symbol is on the folio, and it is the terminal of the strip
	QCOMPARE(terminalElementsOf(folio), terminals_before + 1);
	QCOMPARE(strip->realTerminals().count(), 1);
	const auto bound = strip->realTerminals().constFirst();
	QVERIFY2(bound->isElement(), "the terminal of the strip is drawn now");
	QVERIFY(!bound->isPending());
	QVERIFY(bound->element());
	QCOMPARE(bound->label(), QStringLiteral("X999:1"));

		//The element took over what the strip editor edited for the
		//terminal -- its type here, the label is checked above through
		//the terminal, which reads it from the element
	QCOMPARE(bound->element()->elementData().terminalType(), ElementData::TTFuse);
	QCOMPARE(bound->parentStrip(), strip);

		//One undo takes the symbol off the folio and gives the pending
		//terminal its place back, data included
	project.undoStack()->undo();
	QCOMPARE(project.undoStack()->index(), undo_index_before);
	QCOMPARE(terminalElementsOf(folio), terminals_before);
	QCOMPARE(strip->realTerminals().count(), 1);
	QCOMPARE(strip->realTerminals().constFirst(), pending);
	QVERIFY2(pending->isPending(), "the terminal waits for its symbol again");
	QVERIFY(!pending->isElement());
	QCOMPARE(pending->label(), QStringLiteral("X999:1"));
		//The terminal keeps the plain path: the runtime project id the
		//helper read the symbol with is worth nothing outside the session
		//which wrote it, and is dropped both in memory and in the files
		//(@sa RealTerminal::symbolWithoutProjectId)
	QCOMPARE(pending->symbol(),
			 RealTerminal::symbolWithoutProjectId(symbol_));
	QCOMPARE(pending->terminalData().terminalType(), ElementData::TTFuse);

		//And a redo binds it again, to the very same result
	project.undoStack()->redo();
	QCOMPARE(terminalElementsOf(folio), terminals_before + 1);
	const auto rebound = strip->realTerminals().constFirst();
	QVERIFY(rebound->isElement());
	QCOMPARE(rebound->label(), QStringLiteral("X999:1"));
	QCOMPARE(rebound->element()->elementData().terminalType(), ElementData::TTFuse);
}

/**
	The drop on the folio is what starts the placement: the view takes a
	drag which carries a terminal waiting for its symbol, reads the strip
	and the terminal back from the project through their uuid, and hands
	the placement over to the event of the terminal. The placement then
	works the way every placement of a symbol does -- the symbol follows
	the cursor, a click drops it -- and the test finishes it by clearing
	the interface, since a click can not be sent through the window
	system on an offscreen platform.
*/
void tst_bindpendingterminal::startsThePlacementFromADrop()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);
	if (symbol_.isEmpty()) {
		QSKIP("the test project holds no free terminal to take a symbol from");
	}

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	auto pending = RealTerminal::createPending(QStringLiteral("X999:1"), symbol_);
	QVERIFY(pending);
	QVERIFY(strip->addTerminal(pending));

	DropView view(folio);
	const int terminals_before = terminalElementsOf(folio);

	QMimeData mime;
	mime.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				 DiagramEventAddTerminal::pendingTerminalPayload(
					 strip->uuid(), pending->uuid()));

	QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime,
						  Qt::LeftButton, Qt::NoModifier);
	enter.setAccepted(false);
	view.dragEnterEvent(&enter);
	QVERIFY2(enter.isAccepted(),
			 "the view takes a drag which carries a terminal to be drawn");

	QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime,
					Qt::LeftButton, Qt::NoModifier);
	view.dropEvent(&drop);
	QVERIFY2(folio->eventInterfaceIsRunning(),
			 "the drop starts the placement of the symbol of the terminal");

		//The placement is the interactive one: the pending terminal is
		//still waiting, and the one terminal element more on the folio
		//is the symbol which follows the cursor -- the same temporary
		//every placement of a symbol has, dropped again when the
		//placement ends.
	QVERIFY(pending->isPending());
	QCOMPARE(terminalElementsOf(folio), terminals_before + 1);

	folio->clearEventInterface();
	QVERIFY(!folio->eventInterfaceIsRunning());
	QVERIFY2(pending->isPending(),
			 "giving the placement up drew nothing and bound nothing");
	QCOMPARE(terminalElementsOf(folio), terminals_before);
}

/**
	A symbol saved with the project id of the session which saved it is
	drawn just as well as one saved without it. The id is handed out
	afresh in every session and never reused (@sa
	QETApp::registerProject), so a 'symbol="project7+embed://..."' read
	in a later session resolves to another project, or to no project at
	all: the drop then found no location for the terminal and refused to
	start, saying the symbol could not be found -- which is how a saved
	project lost the ability to draw its own pending terminals, with the
	verdict shown in the status bar of the editor.

	The id is dropped when the terminal is built, and the path is bound
	back to the project of the strip the terminal lives in instead
	(@sa RealTerminal::symbolLocation), so the very same saved string
	starts a placement again, in whatever session it is read.
*/
void tst_bindpendingterminal::placesATerminalWhoseSymbolCarriesAnotherProjectId()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);
	if (symbol_.isEmpty()) {
		QSKIP("the test project holds no free terminal to take a symbol from");
	}

		//The id of this very project replaced by another one: a string
		//which reads as a project location and names no open project,
		//which is what a later session makes out of every saved id. The
		//other id is a number this test process never registered -- only
		//this project is open, at its own id -- so the difference does
		//not depend on which number that is.
	const int own_id_ = QETApp::projectId(&project);
	const int foreign_id_ = own_id_ == 0 ? 1 : 0;
	const auto stale_symbol_ = QStringLiteral("project%1+").arg(foreign_id_)
			+ RealTerminal::symbolWithoutProjectId(symbol_);
	QVERIFY(stale_symbol_ != symbol_);

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	auto pending = RealTerminal::createPending(QStringLiteral("X999:1"), stale_symbol_);
	QVERIFY(pending);
		//The terminal keeps the plain path, the foreign id is not its to
		//keep -- createPending is the one place symbols are set
	QCOMPARE(pending->symbol(),
			 RealTerminal::symbolWithoutProjectId(symbol_));
	QVERIFY(strip->addTerminal(pending));

	DropView view(folio);
	const int terminals_before = terminalElementsOf(folio);

	QMimeData mime;
	mime.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				 DiagramEventAddTerminal::pendingTerminalPayload(
					 strip->uuid(), pending->uuid()));
	QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime,
					Qt::LeftButton, Qt::NoModifier);
	view.dropEvent(&drop);
	QVERIFY2(folio->eventInterfaceIsRunning(),
			 "a symbol saved with a project id of another session still "
			 "starts the placement");

		//And the run is the usual one: it can be given up without
		//having drawn or bound anything
	folio->clearEventInterface();
	QVERIFY(pending->isPending());
	QCOMPARE(terminalElementsOf(folio), terminals_before);
}

/**
	A drag which leads nowhere is refused on the view, and nothing is
	started: a terminal without any symbol has nothing to be drawn with,
	and a terminal which is not part of this project anymore (a uuid no
	strip of the project answers to) can not be bound to a folio of it.
	Every one of those drops says why in the status bar of the editor --
	which is not there when the view stands on its own, as here.
*/
void tst_bindpendingterminal::refusesADropWhichHasNothingToDraw()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
		//The mixed case at the end of this test needs a symbol which
		//resolves: the project is registered the way an open one is
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	auto without_symbol = RealTerminal::createPending(QStringLiteral("X999:1"), QString());
	QVERIFY(without_symbol);
	QVERIFY(strip->addTerminal(without_symbol));

	const int terminals_before = terminalElementsOf(folio);
	DropView view(folio);

		//A terminal which has no symbol to be drawn with: the drop is
		//taken (it is a terminal of the navigator) but nothing starts
	QMimeData mime;
	mime.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				 DiagramEventAddTerminal::pendingTerminalPayload(
					 strip->uuid(), without_symbol->uuid()));
	QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime,
					Qt::LeftButton, Qt::NoModifier);
	view.dropEvent(&drop);
	QVERIFY2(!folio->eventInterfaceIsRunning(),
			 "a terminal without a symbol is drawn nowhere");
	QCOMPARE(terminalElementsOf(folio), terminals_before);
	QVERIFY(without_symbol->isPending());

		//A terminal no strip of the project answers to: the payload is
		//readable, the uuid behind it is not
	QMimeData unknown;
	unknown.setData(DiagramEventAddTerminal::pendingTerminalMime(),
					DiagramEventAddTerminal::pendingTerminalPayload(
						strip->uuid(), QUuid::createUuid()));
	QDropEvent unknown_drop(QPointF(10, 10), Qt::CopyAction, &unknown,
							Qt::LeftButton, Qt::NoModifier);
	view.dropEvent(&unknown_drop);
	QVERIFY2(!folio->eventInterfaceIsRunning(),
			 "an unknown terminal is drawn nowhere");
	QCOMPARE(terminalElementsOf(folio), terminals_before);

		//And a payload which carries nothing readable is not a terminal
		//drag at all
	QMimeData broken;
	broken.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				   QByteArrayLiteral("not-a-payload"));
	QDropEvent broken_drop(QPointF(10, 10), Qt::CopyAction, &broken,
						   Qt::LeftButton, Qt::NoModifier);
	view.dropEvent(&broken_drop);
	QVERIFY(!folio->eventInterfaceIsRunning());
	QCOMPARE(terminalElementsOf(folio), terminals_before);

		//A selection is started as a whole or not at all: one terminal
		//without a symbol refuses every one of them, even next to a
		//terminal which could be drawn right away -- a run which drew
		//part of the marks would leave the rest of them without a word.
	if (!symbol_.isEmpty()) {
		auto with_symbol = RealTerminal::createPending(
					QStringLiteral("X999:2"), symbol_);
		QVERIFY(with_symbol);
		QVERIFY(strip->addTerminal(with_symbol));

		QMimeData mixed;
		mixed.setData(DiagramEventAddTerminal::pendingTerminalMime(),
					  DiagramEventAddTerminal::pendingTerminalsPayload({
						  qMakePair(strip->uuid(), with_symbol->uuid()),
						  qMakePair(strip->uuid(), without_symbol->uuid())}));
		QDropEvent mixed_drop(QPointF(10, 10), Qt::CopyAction, &mixed,
							  Qt::LeftButton, Qt::NoModifier);
		view.dropEvent(&mixed_drop);
		QVERIFY2(!folio->eventInterfaceIsRunning(),
				 "one terminal without a symbol refuses the whole selection");
		QCOMPARE(terminalElementsOf(folio), terminals_before);
		QVERIFY(with_symbol->isPending());
		QVERIFY(without_symbol->isPending());

		QVERIFY(strip->removeTerminal(with_symbol));
	}
}

/**
	A selection of the navigator is drawn as a whole: the terminals are
	taken one per click, in the order of the payload -- which is the
	order of the selection in the tree, top to bottom -- and the run
	keeps going between the clicks, the symbol of the next terminal
	following the cursor as soon as the previous one hangs on the folio.

	Every click has pushed its own undo macro, so the run can be taken
	back terminal by terminal, the last drawn first.
*/
void tst_bindpendingterminal::drawsASelectionOfTerminalsInOrder()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);
	if (symbol_.isEmpty()) {
		QSKIP("the test project holds no free terminal to take a symbol from");
	}

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	const QStringList labels{QStringLiteral("X999:1"),
							 QStringLiteral("X999:2"),
							 QStringLiteral("X999:3")};
	QVector<QSharedPointer<RealTerminal>> pendings;
	for (const auto &label_ : labels) {
		auto pending = RealTerminal::createPending(label_, symbol_);
		QVERIFY(pending);
		QVERIFY(strip->addTerminal(pending));
		pendings.append(pending);
	}
	QCOMPARE(strip->realTerminals().count(), int(labels.size()));

	const int terminals_before = terminalElementsOf(folio);
	const int undo_index_before = project.undoStack()->index();

		//One terminal of the strip, by its position in it: the run
		//draws them in the order of the payload, which is the order
		//the strip holds them in.
	const auto terminal_at = [strip](int index) {
		return strip->realTerminals().at(index);
	};

	auto *event = new DiagramEventAddTerminal(pendings, folio, QPointF(10, 20));
	QVERIFY2(event->isRunning(),
			 "the three symbols are built and the placement started");
	folio->setEventInterface(event);

		//Move the cursor symbol and drop it, the way the clicks of the
		//user reach the tool through the folio. The last click of the
		//run ends the event, which deletes it during the call -- so
		//nothing of @c event may be touched after that one.
	const auto move_and_click = [](DiagramEventAddTerminal *tool,
								   const QPointF &pos) {
		QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
		move.setScenePos(pos);
		tool->mouseMoveEvent(&move);
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setButton(Qt::LeftButton);
		release.setScenePos(pos);
		tool->mouseReleaseEvent(&release);
	};

		//Before the first click nothing is bound: every terminal still
		//waits, and the one terminal element more on the folio is the
		//symbol which follows the cursor
	QVERIFY(terminal_at(0)->isPending());
	QCOMPARE(terminalElementsOf(folio), terminals_before + 1);

		//First click: the first terminal of the run is drawn, the run
		//goes on and the symbol of the next terminal takes the cursor
	move_and_click(event, QPointF(60, 80));
	QVERIFY2(folio->eventInterfaceIsRunning(),
			 "the run goes on after the first terminal");
	QCOMPARE(terminalElementsOf(folio), terminals_before + 2);
	QVERIFY2(terminal_at(0)->isElement(),
			 "the first terminal of the run is drawn first");
	QVERIFY(!terminal_at(0)->isPending());
	QVERIFY(terminal_at(1)->isPending());
	QVERIFY(terminal_at(2)->isPending());
	QCOMPARE(project.undoStack()->index(), undo_index_before + 1);

		//Second click: same, one terminal further
	move_and_click(event, QPointF(110, 140));
	QVERIFY(folio->eventInterfaceIsRunning());
	QCOMPARE(terminalElementsOf(folio), terminals_before + 3);
	QVERIFY(terminal_at(0)->isElement());
	QVERIFY(terminal_at(1)->isElement());
	QVERIFY(terminal_at(2)->isPending());
	QCOMPARE(project.undoStack()->index(), undo_index_before + 2);

		//Third click: the last terminal of the run, which ends the event
	move_and_click(event, QPointF(160, 200));
		//'event' is deleted by this very click
	QVERIFY2(!folio->eventInterfaceIsRunning(),
			 "the run is over once its last terminal is placed");
	QCOMPARE(terminalElementsOf(folio), terminals_before + 3);
	for (int i = 0 ; i < int(labels.size()) ; ++i) {
		const auto bound = terminal_at(i);
		QVERIFY2(bound->isElement(), "every terminal of the run is drawn");
		QCOMPARE(bound->label(), labels.at(i));
	}
	QCOMPARE(project.undoStack()->index(), undo_index_before + 3);

		//One undo per terminal, the last drawn first
	project.undoStack()->undo();
	QCOMPARE(terminalElementsOf(folio), terminals_before + 2);
	QVERIFY(terminal_at(2)->isPending());
	QVERIFY(terminal_at(1)->isElement());
	project.undoStack()->undo();
	project.undoStack()->undo();
	QCOMPARE(project.undoStack()->index(), undo_index_before);
	QCOMPARE(terminalElementsOf(folio), terminals_before);
	for (const auto &terminal_ : strip->realTerminals()) {
		QVERIFY2(terminal_->isPending(),
				 "every terminal of the run waits for its symbol again");
		QVERIFY(!terminal_->isElement());
	}
}
/**
	The payload of a drag is the format of the event it starts, and it
	reads back the two uuids it was written with -- the strip and the
	terminal -- and nothing else: a mime of the same format which holds
	something else is no terminal drag at all. A whole run reads back
	the same way, line for line and in order.
*/
void tst_bindpendingterminal::readsBackItsOwnPayload()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	auto pending = RealTerminal::createPending(QStringLiteral("X999:1"), QString());
	QVERIFY(pending);
	QVERIFY(strip->addTerminal(pending));

	QMimeData mime;
	mime.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				 DiagramEventAddTerminal::pendingTerminalPayload(
					 strip->uuid(), pending->uuid()));

	QUuid strip_uuid, terminal_uuid;
	QVERIFY(DiagramEventAddTerminal::readPendingTerminalMime(
				&mime, &strip_uuid, &terminal_uuid));
	QCOMPARE(strip_uuid, strip->uuid());
	QCOMPARE(terminal_uuid, pending->uuid());

		//No format, no terminal: a mime of a text drag answers nothing
	QMimeData text;
	text.setText(QStringLiteral("some text"));
	QVERIFY(!DiagramEventAddTerminal::readPendingTerminalMime(
				&text, &strip_uuid, &terminal_uuid));

		//The format, but nothing worth reading behind it
	QMimeData broken;
	broken.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				   QByteArrayLiteral("half;a;payload;"));
	QVERIFY(!DiagramEventAddTerminal::readPendingTerminalMime(
				&broken, &strip_uuid, &terminal_uuid));

		//A whole run reads back as well, line for line and in the order
		//it was written in -- which is the order the run draws them
	const QVector<QPair<QUuid, QUuid>> run{
		qMakePair(strip->uuid(), pending->uuid()),
		qMakePair(strip->uuid(), QUuid::createUuid())};
	QMimeData many;
	many.setData(DiagramEventAddTerminal::pendingTerminalMime(),
				 DiagramEventAddTerminal::pendingTerminalsPayload(run));
	QVector<QPair<QUuid, QUuid>> read_back;
	QVERIFY(DiagramEventAddTerminal::readPendingTerminalsMime(&many, &read_back));
	QCOMPARE(read_back.count(), run.count());
	QCOMPARE(read_back.at(0).first, run.at(0).first);
	QCOMPARE(read_back.at(0).second, run.at(0).second);
	QCOMPARE(read_back.at(1).first, run.at(1).first);
	QCOMPARE(read_back.at(1).second, run.at(1).second);
		//A single terminal reads through the run function too: it is
		//simply one line
	QVERIFY(DiagramEventAddTerminal::readPendingTerminalsMime(&mime, &read_back));
	QCOMPARE(read_back.count(), 1);

		//One line worth nothing makes no run at all: half of an order
		//can not be read back, whoever wrote it
	QMimeData broken_line;
	broken_line.setData(DiagramEventAddTerminal::pendingTerminalMime(),
						DiagramEventAddTerminal::pendingTerminalsPayload(run)
						+ QByteArrayLiteral("\nhalf;a;payload;"));
	QVERIFY(!DiagramEventAddTerminal::readPendingTerminalsMime(
				&broken_line, &read_back));
}

/**
	Escape gives the run up the same way a right click does -- the key
	takes the path it really takes, through the view, which passes it on
	to the running tool only while one runs. The terminals already drawn
	stay on the folio, their macros in the undo stack, and the others are
	still waiting for their symbol, cursor symbol included: what follows
	the cursor is dropped, what hangs on the folio stays.
*/
void tst_bindpendingterminal::abortsWithEscape()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);
	if (symbol_.isEmpty()) {
		QSKIP("the test project holds no free terminal to take a symbol from");
	}

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	QVector<QSharedPointer<RealTerminal>> pendings;
	for (const auto &label_ : {QStringLiteral("X999:1"),
							   QStringLiteral("X999:2")}) {
		auto pending = RealTerminal::createPending(label_, symbol_);
		QVERIFY(pending);
		QVERIFY(strip->addTerminal(pending));
		pendings.append(pending);
	}
	QCOMPARE(strip->realTerminals().count(), 2);

	DropView view(folio);
	const int terminals_before = terminalElementsOf(folio);
	const int undo_index_before = project.undoStack()->index();

	auto *event = new DiagramEventAddTerminal(pendings, folio, QPointF(10, 20));
	QVERIFY(event->isRunning());
	folio->setEventInterface(event);

		//One click draws the first terminal of the run
	QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
	release.setButton(Qt::LeftButton);
	release.setScenePos(QPointF(10, 20));
	event->mouseReleaseEvent(&release);
	QVERIFY(folio->eventInterfaceIsRunning());
	QCOMPARE(strip->realTerminals().at(0)->isElement(), true);
	QCOMPARE(terminalElementsOf(folio), terminals_before + 2);

		//Escape, the way the keyboard of the user reaches the tool:
		//through the view, which hands it to the running tool
	QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
	view.keyPressEvent(&escape);
		//'event' is deleted by the tool answering with finish()
	QVERIFY2(escape.isAccepted(),
			 "the running tool takes the Escape key itself");
	QVERIFY2(!folio->eventInterfaceIsRunning(),
			 "Escape gives the run up");
	QVERIFY2(terminalElementsOf(folio) == terminals_before + 1,
			 "the terminal already drawn stays on the folio, "
			 "the symbol which followed the cursor is dropped again");
	QVERIFY2(strip->realTerminals().at(0)->isElement(),
			 "the drawn terminal stays drawn");
	QVERIFY2(strip->realTerminals().at(1)->isPending(),
			 "the terminal not drawn yet still waits for its symbol");
	QCOMPARE(strip->realTerminals().at(1)->label(), QStringLiteral("X999:2"));
	QCOMPARE(project.undoStack()->index(), undo_index_before + 1);

		//And the one macro of the drawn terminal is undone as usual
	project.undoStack()->undo();
	QCOMPARE(project.undoStack()->index(), undo_index_before);
	QCOMPARE(terminalElementsOf(folio), terminals_before);
	QVERIFY(strip->realTerminals().at(0)->isPending());
}

/**
	A right click gives the run up in the middle: the terminal already
	drawn stays on the folio -- its macro is in the undo stack -- and
	the terminals not drawn yet keep waiting for their symbol, exactly
	like the terminals of a run which was never started.
*/
void tst_bindpendingterminal::abortsARunWithARightClick()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	RegisteredProject open(project);
	const auto symbol_ = someTerminalSymbol(project);
	if (symbol_.isEmpty()) {
		QSKIP("the test project holds no free terminal to take a symbol from");
	}

	auto *folio = project.folios().constFirst();
	QVERIFY(folio);
	project.buildFolioToShow(folio);

	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	QVector<QSharedPointer<RealTerminal>> pendings;
	for (const auto &label_ : {QStringLiteral("X999:1"),
							   QStringLiteral("X999:2"),
							   QStringLiteral("X999:3")}) {
		auto pending = RealTerminal::createPending(label_, symbol_);
		QVERIFY(pending);
		QVERIFY(strip->addTerminal(pending));
		pendings.append(pending);
	}
	QCOMPARE(strip->realTerminals().count(), 3);

	const int terminals_before = terminalElementsOf(folio);
	const int undo_index_before = project.undoStack()->index();

	auto *event = new DiagramEventAddTerminal(pendings, folio, QPointF(10, 20));
	QVERIFY(event->isRunning());
	folio->setEventInterface(event);

		//One click draws the first terminal of the run, which keeps
		//going with the second one
	QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
	release.setButton(Qt::LeftButton);
	release.setScenePos(QPointF(10, 20));
	event->mouseReleaseEvent(&release);
	QVERIFY(folio->eventInterfaceIsRunning());
	QVERIFY(strip->realTerminals().at(0)->isElement());
	QCOMPARE(terminalElementsOf(folio), terminals_before + 2);

		//Right click gives the run up
	QGraphicsSceneMouseEvent right_release(QEvent::GraphicsSceneMouseRelease);
	right_release.setButton(Qt::RightButton);
	right_release.setScenePos(QPointF(10, 20));
	event->mouseReleaseEvent(&right_release);
		//'event' is deleted by the right click itself
	QVERIFY2(!folio->eventInterfaceIsRunning(),
			 "the right click gives the run up");
	QVERIFY2(terminalElementsOf(folio) == terminals_before + 1,
			 "the terminal already drawn stays on the folio");
	QVERIFY(strip->realTerminals().at(0)->isElement());
	QCOMPARE(strip->realTerminals().at(0)->label(), QStringLiteral("X999:1"));
	QVERIFY2(strip->realTerminals().at(1)->isPending(),
			 "the terminal not drawn yet still waits for its symbol");
	QVERIFY2(strip->realTerminals().at(2)->isPending(),
			 "the terminal not drawn yet still waits for its symbol");
	QCOMPARE(strip->realTerminals().at(1)->label(), QStringLiteral("X999:2"));
	QCOMPARE(strip->realTerminals().at(2)->label(), QStringLiteral("X999:3"));
	QCOMPARE(project.undoStack()->index(), undo_index_before + 1);

		//The one macro of the drawn terminal is undone as usual, and
		//the strip is left with every terminal waiting again
	project.undoStack()->undo();
	QCOMPARE(project.undoStack()->index(), undo_index_before);
	QCOMPARE(terminalElementsOf(folio), terminals_before);
	for (const auto &terminal_ : strip->realTerminals()) {
		QVERIFY2(terminal_->isPending(),
				 "every terminal waits for its symbol again");
	}
}

QTEST_MAIN(tst_bindpendingterminal)
#include "tst_bindpendingterminal.moc"

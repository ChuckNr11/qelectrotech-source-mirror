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

#include "TerminalNavigator/ui/terminalnavigatorwidget.h"
#include "TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "TerminalStrip/terminalstripbridge.h"
#include "TerminalStrip/ui/terminalstriptreedockwidget.h"
#include "TerminalStrip/UndoCommand/terminalstripdeletion.h"
#include "diagram.h"
#include "diagramcontent.h"
#include "elementprovider.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>

/**
	Taking terminals and strips off a project, the way the terminal
	navigator does it (context menu and Delete key) and the way the
	drawing editor does it when its delete dialog answered "from the
	project": the symbols leave the folios, the rows leave their strips,
	the strips leave the project -- the navigator tree, the terminal
	block manager's tree and the drawings all follow --, and one undo
	which brings the whole of it back, plans and bridges included.

	What must not go silently is checked too: the free terminals are
	never touched, and a bridged or multi-level row is refused the way
	the drawing editor has always refused to delete such terminals.
*/
class tst_deleteterminalstrips : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;
		//A navigator whose "delete strips and branches?" question is
		//answered by the test instead of a human, and counted.
	class TestNavigator : public TerminalNavigatorWidget
	{
		public:
			using TerminalNavigatorWidget::TerminalNavigatorWidget;
			bool answer{true};
			int asked_strips{-1};
			int asked_terminals{-1};
		protected:
			bool confirmGroupDeletion(int strip_count, int terminal_count) override {
				asked_strips = strip_count;
				asked_terminals = terminal_count;
				return answer;
			}
	};
	TestNavigator *m_widget = nullptr;
	QTreeWidget *m_tree = nullptr;

	/// The item of the tree whose text is @p text, anywhere below
	/// @p item, else nullptr. The names of the strips are unique here.
	static QTreeWidgetItem *findByText(QTreeWidgetItem *item, const QString &text)
	{
		if (!item) {
			return nullptr;
		}
		if (item->text(0) == text) {
			return item;
		}
		for (int i = 0 ; i < item->childCount() ; ++i) {
			if (auto *found = findByText(item->child(i), text)) {
				return found;
			}
		}
		return nullptr;
	}

	QTreeWidgetItem *treeItem(const QString &text) const
	{
		return findByText(m_tree->topLevelItem(0), text);
	}

	/// The strip of the project named @p name, else nullptr.
	TerminalStrip *stripNamed(const QString &name) const
	{
		const auto strips = m_project->terminalStrip();
		for (auto *strip : strips) {
			if (strip->name() == name)
				return strip;
		}
		return nullptr;
	}

	/// Every symbol bound to the terminals of @p strip, wherever it
	/// is drawn.
	static QList<Element *> elementsOf(TerminalStrip *strip)
	{
		QList<Element *> elements;
		for (const auto &real_t : strip->realTerminals()) {
			if (real_t->element()) {
				elements << real_t->element();
			}
		}
		return elements;
	}

	static bool onItsFolio(Element *element)
	{
		return element && element->diagram()
				&& element->diagram()->items().contains(element);
	}

	/// Select @p item alone in the tree, and say so.
	void selectOnly(QTreeWidgetItem *item)
	{
		QVERIFY(item);
		m_tree->clearSelection();
		item->setSelected(true);
	}

	/// The signals of the project reach the trees through a single
	/// shot timer in the navigator (a burst of element signals becomes
	/// one rebuild), so the event loop has to turn before the tree
	/// says what it followed.
	static void flushTheTrees()
	{
		QCoreApplication::processEvents();
	}

	/// Take the last pushed macro back, and let the trees say what
	/// they followed: the next test reads them right away, without an
	/// event loop turn of its own in between.
	void undoLastPush()
	{
		m_project->undoStack()->undo();
		flushTheTrees();
	}

private slots:
	void initTestCase();
	void deletesATerminalRowWithoutAsking();
	void asksBeforeDeletingAWholeStrip();
	void deletesEveryStripOfTheProjectButNoFreeTerminal();
	void doesNotDeleteFreeTerminals();
	void deletesAPlanWithItsStrip();
	void refusesBridgedRowsAndKeepsThem();
	void deletesOnlyTheDrawingWhenAsked();
	void deletesTheRowFromTheProjectWhenAsked();
	void saysTheStripsInAndOutThroughTheProjectSignals();
	void followsTheStripsInTheManagerTree();
	void bringsTheBridgesBackWithAnUndoneStripDeletion();
	void cleanupTestCase();
};

void tst_deleteterminalstrips::initTestCase()
{
		//The signals carry the strip itself, and QSignalSpy needs the
		//pointer type registered to record it.
	qRegisterMetaType<TerminalStrip *>();

	m_project = new QETProject(QStringLiteral(QET_TEST_PROJECT_PATH), this);

	auto *x10 = m_project->newTerminalStrip(
				QStringLiteral("KA"), QStringLiteral("Cabinet A"), QStringLiteral("X10"));
	auto *x2 = m_project->newTerminalStrip(
				QStringLiteral("KA"), QStringLiteral("Cabinet A"), QStringLiteral("X2"));
	auto *x201 = m_project->newTerminalStrip(
				QStringLiteral("KB"), QStringLiteral("Roof"), QStringLiteral("X201"));

		//No example carries terminal strips, so the terminals below
		//the strips are taken from the ones the project has free --
		//drawn on a folio already, which is what makes the symbols
		//these tests take off the drawings and put back.
	auto free_terminals = ElementProvider(m_project).freeTerminal();
	QVERIFY(free_terminals.count() >= 7);
	for (int i = 0 ; i < 4 ; ++i)
		QVERIFY(x10->addTerminal(free_terminals.at(i)));
	for (int i = 4 ; i < 6 ; ++i)
		QVERIFY(x2->addTerminal(free_terminals.at(i)));
	QVERIFY(x201->addTerminal(free_terminals.at(6)));
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(x2->physicalTerminalCount(), 2);
	QCOMPARE(x201->physicalTerminalCount(), 1);

	m_widget = new TestNavigator;
	m_widget->setProject(m_project);
	m_tree = m_widget->findChild<QTreeWidget *>();
	QVERIFY(m_tree);
	QCOMPARE(m_tree->topLevelItemCount(), 2);
}

void tst_deleteterminalstrips::deletesATerminalRowWithoutAsking()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto reals = x10->realTerminals();
	QCOMPARE(reals.count(), 4);
	auto *element = reals.first()->element();
	QVERIFY(element);
	auto *diagram = element->diagram();
	QVERIFY(diagram);
	QVERIFY(onItsFolio(element));

	selectOnly(findByText(treeItem(QStringLiteral("X10")), reals.first()->label()));
	m_widget->asked_strips = -1;
	m_widget->deleteSelection();

		//No question for a row: the strip keeps its other rows, the
		//row leaves the drawing, and the tree knows through the
		//signals of the project.
	QCOMPARE(m_widget->asked_strips, -1);
	QCOMPARE(x10->physicalTerminalCount(), 3);
	QVERIFY(!x10->physicalTerminal(reals.first()->physicalTerminal()->uuid()));
	QVERIFY(!onItsFolio(element));
	flushTheTrees();

	undoLastPush();

	QCOMPARE(x10->physicalTerminalCount(), 4);
	QVERIFY(onItsFolio(element));
	QCOMPARE(reals.first()->element(), element);
}

void tst_deleteterminalstrips::asksBeforeDeletingAWholeStrip()
{
	auto *x2 = stripNamed(QStringLiteral("X2"));
	QVERIFY(x2);
	const auto elements = elementsOf(x2);
	QCOMPARE(elements.count(), 2);

	selectOnly(treeItem(QStringLiteral("X2")));
	m_widget->answer = false;
	m_widget->asked_strips = -1;
	m_widget->asked_terminals = -1;
	m_widget->deleteSelection();

		//The question is asked for a strip -- one strip with no rows
		//selected besides --, and a "no" leaves everything where it is.
	QCOMPARE(m_widget->asked_strips, 1);
	QCOMPARE(m_widget->asked_terminals, 0);
	QVERIFY(m_project->terminalStrip().contains(x2));
	QCOMPARE(x2->physicalTerminalCount(), 2);
	for (auto *element : elements) {
		QVERIFY(onItsFolio(element));
	}

	m_widget->answer = true;
	m_widget->deleteSelection();

	QVERIFY(!m_project->terminalStrip().contains(x2));
	for (auto *element : elements) {
		QVERIFY(!onItsFolio(element));
	}
	flushTheTrees();

	undoLastPush();

	QVERIFY(m_project->terminalStrip().contains(x2));
	QCOMPARE(x2->physicalTerminalCount(), 2);
	const auto reals = x2->realTerminals();
	QCOMPARE(reals.count(), 2);
	for (int i = 0 ; i < 2 ; ++i) {
		QCOMPARE(reals.at(i)->element(), elements.at(i));
		QVERIFY(onItsFolio(elements.at(i)));
	}
}

void tst_deleteterminalstrips::deletesEveryStripOfTheProjectButNoFreeTerminal()
{
	const int free_before = ElementProvider(m_project).freeTerminal().count();
	QCOMPARE(m_project->terminalStrip().count(), 3);

	selectOnly(m_tree->topLevelItem(0));		//The project itself
	m_widget->answer = true;
	m_widget->asked_strips = -1;
	m_widget->deleteSelection();

	QCOMPARE(m_widget->asked_strips, 3);
	QVERIFY(m_project->terminalStrip().isEmpty());
	flushTheTrees();

		//The free terminals belong to no strip: the branch of the
		//project is the strips it holds, all of them, and nothing else.
	QCOMPARE(ElementProvider(m_project).freeTerminal().count(), free_before);

	undoLastPush();

	QCOMPARE(m_project->terminalStrip().count(), 3);
	QCOMPARE(ElementProvider(m_project).freeTerminal().count(), free_before);
	for (const auto &strip : m_project->terminalStrip()) {
		QVERIFY(!elementsOf(strip).isEmpty());
	}
}

void tst_deleteterminalstrips::doesNotDeleteFreeTerminals()
{
	auto *free_root = m_tree->topLevelItem(1);
	QVERIFY(free_root);
	QVERIFY(free_root->childCount() > 0);
	const int commands = m_project->undoStack()->count();
	const int free_before = ElementProvider(m_project).freeTerminal().count();

	selectOnly(free_root->child(0));
	m_widget->deleteSelection();

		//A free terminal belongs to no strip and leaves nothing of a
		//strip behind: not even an undo step is made for it.
	QCOMPARE(m_project->undoStack()->count(), commands);
	QCOMPARE(ElementProvider(m_project).freeTerminal().count(), free_before);
}

void tst_deleteterminalstrips::deletesAPlanWithItsStrip()
{
	auto *x201 = stripNamed(QStringLiteral("X201"));
	QVERIFY(x201);
	auto *diagram = m_project->diagrams().constFirst();
	QVERIFY(diagram);
	auto *plan = new TerminalStripItem(x201);
	diagram->addItem(plan);
	QVERIFY(diagram->items().contains(plan));

	selectOnly(treeItem(QStringLiteral("X201")));
	m_widget->answer = true;
	m_widget->deleteSelection();

		//A strip leaves with the plans drawn for it, wherever they
		//stand: a plan of nothing would be left on the folio else.
	QVERIFY(!m_project->terminalStrip().contains(x201));
	QVERIFY(!diagram->items().contains(plan));
	flushTheTrees();

	undoLastPush();

	QVERIFY(m_project->terminalStrip().contains(x201));
	QVERIFY(diagram->items().contains(plan));
}

void tst_deleteterminalstrips::refusesBridgedRowsAndKeepsThem()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto reals = x10->realTerminals();
	QCOMPARE(reals.count(), 4);
	QVERIFY(x10->setBridge({reals.at(0), reals.at(1)}));

	auto *strip_item = treeItem(QStringLiteral("X10"));
	QVERIFY(strip_item);
	m_tree->clearSelection();
	auto *row0 = findByText(strip_item, reals.at(0)->label());
	auto *row1 = findByText(strip_item, reals.at(1)->label());
	QVERIFY(row0);
	QVERIFY(row1);
	row0->setSelected(true);
	row1->setSelected(true);

		//The refusal is a message box, and nobody here to answer it:
		//close it from the event loop exec() spins, and say it came.
	const int commands = m_project->undoStack()->count();
	bool refused{false};
	QTimer::singleShot(0, this, [&]() {
		if (auto *modal = qobject_cast<QMessageBox *>(
					QApplication::activeModalWidget())) {
			refused = true;
			modal->close();
		}
	});
	m_widget->deleteSelection();
	QVERIFY(refused);

		//A bridged row is refused the way the drawing editor refuses
		//it, and nothing at all is deleted then -- not even the row
		//which is not bridged itself, and not one undo step is made.
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(m_project->undoStack()->count(), commands);

	x10->unBridge({reals.at(0), reals.at(1)});
	m_widget->deleteSelection();

	QCOMPARE(x10->physicalTerminalCount(), 2);
	undoLastPush();
	QCOMPARE(x10->physicalTerminalCount(), 4);
}

void tst_deleteterminalstrips::deletesOnlyTheDrawingWhenAsked()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto reals = x10->realTerminals();
	auto real = reals.first();
	auto *element = real->element();
	QVERIFY(element);
	auto *diagram = element->diagram();
	QVERIFY(diagram);

	diagram->clearSelection();
	element->setSelected(true);
	DiagramContent content(diagram);
	QVERIFY(TerminalStripDeletion::hasDrawnTerminal(content));
	const auto rows = TerminalStripDeletion::rowsOf(content);
	QCOMPARE(rows.count(), 1);
	QCOMPARE(rows.constFirst().second.count(), 1);

		//The drawing alone goes, the way the dialog's "from the
		//drawing only" answers: the row stays in the strip, and the
		//element is kept alive by the command on the stack so that
		//undo() finds it back where it was.
	diagram->undoStack().push(TerminalStripDeletion::drawingDeletionCommand(
								  diagram, content, {}, {}, {}, QStringLiteral("t")));
	QVERIFY(!onItsFolio(element));
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(real->element(), element);

	diagram->undoStack().undo();

	QVERIFY(onItsFolio(element));
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(real->element(), element);
	flushTheTrees();
}

void tst_deleteterminalstrips::deletesTheRowFromTheProjectWhenAsked()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto reals = x10->realTerminals();
	auto real = reals.first();
	const auto uuid_ = real->physicalTerminal()->uuid();
	auto *element = real->element();
	QVERIFY(element);
	auto *diagram = element->diagram();
	QVERIFY(diagram);

	diagram->clearSelection();
	element->setSelected(true);
	DiagramContent content(diagram);
	const auto rows = TerminalStripDeletion::rowsOf(content);
	QCOMPARE(rows.count(), 1);

		//The whole project goes, the way the dialog's "from the
		//project" answers: the symbol leaves the folio, the row leaves
		//the strip, and one undo step gives both back.
	diagram->undoStack().push(TerminalStripDeletion::drawingDeletionCommand(
								  diagram, content, {}, {}, rows, QStringLiteral("t")));
	QVERIFY(!onItsFolio(element));
	QCOMPARE(x10->physicalTerminalCount(), 3);
	QVERIFY(!x10->physicalTerminal(uuid_));

	diagram->undoStack().undo();

	QCOMPARE(x10->physicalTerminalCount(), 4);
	QVERIFY(onItsFolio(element));
	QCOMPARE(x10->realTerminals().first()->element(), element);
	flushTheTrees();
}

void tst_deleteterminalstrips::saysTheStripsInAndOutThroughTheProjectSignals()
{
	QSignalSpy added(m_project, &QETProject::terminalStripAdded);
	QSignalSpy removed(m_project, &QETProject::terminalStripRemoved);
	QVERIFY(added.isValid());
	QVERIFY(removed.isValid());

	auto *x201 = stripNamed(QStringLiteral("X201"));
	QVERIFY(x201);
	const int added_before = added.count();
	const int removed_before = removed.count();

	auto *macro = TerminalStripDeletion::stripsDeletionMacro(m_project, {x201});
	QVERIFY(macro);
	m_project->undoStack()->push(macro);

		//Every view over the strips follows through these two, whether
		//the strip left through the navigator, through the terminal
		//block manager or through the script API.
	QCOMPARE(removed.count(), removed_before + 1);
	QCOMPARE(removed.constLast().constFirst().value<TerminalStrip *>(), x201);

	undoLastPush();

	QCOMPARE(added.count(), added_before + 1);
	QCOMPARE(added.constLast().constFirst().value<TerminalStrip *>(), x201);
}

void tst_deleteterminalstrips::followsTheStripsInTheManagerTree()
{
	TerminalStripTreeDockWidget dock(m_project);
	auto *tree = dock.findChild<QTreeWidget *>();
	QVERIFY(tree);
	auto findByText_ = [&](const QString &text) {
		return findByText(tree->topLevelItem(0), text);
	};
	QVERIFY(findByText_(QStringLiteral("X201")));
	const int strips_before = m_project->terminalStrip().count();

	auto *x201 = stripNamed(QStringLiteral("X201"));
	QVERIFY(x201);
	auto *macro = TerminalStripDeletion::stripsDeletionMacro(m_project, {x201});
	QVERIFY(macro);
	m_project->undoStack()->push(macro);

		//The dock follows through the signals of the project, with no
		//reload of its own to run: the strip is gone from its tree
		//right away.
	QVERIFY(!findByText_(QStringLiteral("X201")));
	QCOMPARE(m_project->terminalStrip().count(), strips_before - 1);

	undoLastPush();

	QVERIFY(findByText_(QStringLiteral("X201")));
	QCOMPARE(m_project->terminalStrip().count(), strips_before);
}

void tst_deleteterminalstrips::bringsTheBridgesBackWithAnUndoneStripDeletion()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto reals = x10->realTerminals();
	QCOMPARE(reals.count(), 4);
	QVERIFY(x10->setBridge({reals.at(0), reals.at(1)}));
	const auto bridge_ = reals.at(0)->bridge();
	QVERIFY(bridge_);

	auto *macro = TerminalStripDeletion::stripsDeletionMacro(m_project, {x10});
	QVERIFY(macro);
	m_project->undoStack()->push(macro);
	QVERIFY(!m_project->terminalStrip().contains(x10));

	undoLastPush();

		//A strip taken back is whole again, bridges included: they
		//would silently be gone else, and no view would say so.
	QVERIFY(m_project->terminalStrip().contains(x10));
	QCOMPARE(x10->physicalTerminalCount(), 4);
	const auto reals_back = x10->realTerminals();
	QCOMPARE(reals_back.at(0)->bridge(), bridge_);
	QCOMPARE(reals_back.at(1)->bridge(), bridge_);
	QCOMPARE(bridge_->realTerminals().count(), 2);
}

void tst_deleteterminalstrips::cleanupTestCase()
{
	delete m_widget;
	m_widget = nullptr;
}

QET_APP_TEST_MAIN(tst_deleteterminalstrips)
#include "tst_deleteterminalstrips.moc"

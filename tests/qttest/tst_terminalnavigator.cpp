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

#include "ElementsCollection/elementscollectionwidget.h"
#include "TerminalNavigator/ui/terminalnavigatorwidget.h"
#include "TerminalStrip/terminalstrip.h"
#include "elementprovider.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>

/**
	The terminal tab of the elements collection panel shows the strips of
	the project the way the terminal strip editor's dock does: one root
	holding the project with Installation > Location > Strip below it and
	the terminals of a strip below that strip, and a second root holding
	the free terminals. A terminal moved from the free terminals into a
	strip leaves the first root for the second, strips of one location
	are siblings sorted naturally, and the strip selected before a reload
	still is after it.

	No example file carries terminal strips, so the strips used here are
	built from the free terminals examples/industrial.qet does have.
*/
class tst_terminalnavigator : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;
	TerminalNavigatorWidget *m_widget = nullptr;
	QTreeWidget *m_tree = nullptr;

	/// The child of @p item whose text is @p text, else nullptr.
	static QTreeWidgetItem *childByText(const QTreeWidgetItem *item, const QString &text)
	{
		if (!item)
			return nullptr;
		for (int i = 0 ; i < item->childCount() ; ++i) {
			if (item->child(i)->text(0) == text)
				return item->child(i);
		}
		return nullptr;
	}

	/// The child of the tree's root @p index whose text is @p text.
	QTreeWidgetItem *rootChild(int index, const QString &text) const
	{
		return childByText(m_tree->topLevelItem(index), text);
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

	static QStringList childrenTexts(const QTreeWidgetItem *item)
	{
		QStringList texts;
		if (!item)
			return texts;
		for (int i = 0 ; i < item->childCount() ; ++i)
			texts << item->child(i)->text(0);
		return texts;
	}

	/// The strip X10 of the KA / Cabinet A branch, the one the tests
	/// select and reload: nullptr until initTestCase() built it.
	QTreeWidgetItem *stripX10() const
	{
		auto *ka = rootChild(0, QStringLiteral("KA"));
		return childByText(ka, QStringLiteral("Cabinet A"))
				? childByText(childByText(ka, QStringLiteral("Cabinet A")),
							  QStringLiteral("X10"))
				: nullptr;
	}

		//A reload rebuilds every item of the tree, so the root holding
		//the free terminals has to be taken again after each one -- the
		//item held before is deleted.
	QTreeWidgetItem *freeRoot() const
	{
		return m_tree->topLevelItem(1);
	}

	int freeTerminalCount() const
	{
		return freeRoot() ? freeRoot()->childCount() : -1;
	}

private slots:
	void initTestCase();
	void groupsStripsByInstallationAndLocation();
	void movesTerminalFromFreeToStrip();
	void selectsStripAndTerminal();
	void reloadKeepsTheSelectedStrip();
	void followsElementsOfTheFolios();
	void clearsWithoutProject();
	void followsTheActiveProject();
	void cleanupTestCase();
};

void tst_terminalnavigator::initTestCase()
{
		//The signal carries the strip itself, and QSignalSpy needs the
		//pointer type registered to record it.
	qRegisterMetaType<TerminalStrip *>();

	m_project = new QETProject(QStringLiteral(QET_TEST_PROJECT_PATH), this);

		//Two installations, two locations, three strips: X10 is added
		//before X2, so the tree only shows them in the right order if it
		//sorts naturally (X2 < X10, not X10 < X2).
	auto *x10 = m_project->newTerminalStrip(
				QStringLiteral("KA"), QStringLiteral("Cabinet A"), QStringLiteral("X10"));
	auto *x2 = m_project->newTerminalStrip(
				QStringLiteral("KA"), QStringLiteral("Cabinet A"), QStringLiteral("X2"));
	auto *x201 = m_project->newTerminalStrip(
				QStringLiteral("KB"), QStringLiteral("Roof"), QStringLiteral("X201"));

		//No example carries terminal strips, so the terminals below the
		//strips are taken from the ones the project has free.
	auto free_terminals = ElementProvider(m_project).freeTerminal();
	QVERIFY(free_terminals.count() >= 7);
	for (int i = 0 ; i < 4 ; ++i)
		QVERIFY(x10->addTerminal(free_terminals.at(i)));
	for (int i = 4 ; i < 6 ; ++i)
		QVERIFY(x2->addTerminal(free_terminals.at(i)));
	QVERIFY(x201->addTerminal(free_terminals.at(6)));
	QCOMPARE(x10->physicalTerminalCount(), 4);

	m_widget = new TerminalNavigatorWidget;
	m_widget->setProject(m_project);
	m_tree = m_widget->findChild<QTreeWidget *>();
	QVERIFY(m_tree);
		//The project itself and the free terminals are two roots.
	QCOMPARE(m_tree->topLevelItemCount(), 2);
}

void tst_terminalnavigator::groupsStripsByInstallationAndLocation()
{
	auto *root = m_tree->topLevelItem(0);
	QVERIFY(root);
	QVERIFY(root->parent() == nullptr);
	QCOMPARE(root->text(0), m_project->title());

		//Installation KA holds location Cabinet A, which holds both
		//strips as siblings, in natural order.
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY2(ka, qPrintable(QStringLiteral("installations: %1")
								.arg(childrenTexts(root).join(QLatin1Char(',')))));
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	QCOMPARE(childrenTexts(cabinet_a),
			 (QStringList{QStringLiteral("X2"), QStringLiteral("X10")}));

		//Installation KB is a sibling of KA, not below it, and its
		//location and strip are nested the same way.
	auto *kb = rootChild(0, QStringLiteral("KB"));
	QVERIFY(kb);
	QVERIFY(kb->parent() == root);
	QVERIFY(childByText(ka, QStringLiteral("Roof")) == nullptr);
	auto *roof = childByText(kb, QStringLiteral("Roof"));
	QVERIFY(roof);
	QVERIFY(childByText(roof, QStringLiteral("X201")));
	QVERIFY(childByText(roof, QStringLiteral("X10")) == nullptr);

		//Every strip carries its terminals below itself.
	QCOMPARE(childrenTexts(stripX10()).count(), 4);
	QVERIFY(!childrenTexts(stripX10()).filter(QStringLiteral(":")).isEmpty());

		//The free terminals are the second root, and the strips' own
		//terminals are not counted there anymore.
	auto *free_root = m_tree->topLevelItem(1);
	QVERIFY(free_root);
	QVERIFY(free_root->parent() == nullptr);
	QVERIFY(free_root != root);
	QVERIFY(childrenTexts(free_root).count() > 10);
}

void tst_terminalnavigator::movesTerminalFromFreeToStrip()
{
	auto *free_root = m_tree->topLevelItem(1);
	QVERIFY(free_root);

	auto free_terminals = ElementProvider(m_project).freeTerminal();
	QVERIFY(!free_terminals.isEmpty());
	auto *moving = free_terminals.first();
	const QString label = moving->actualLabel();
	QVERIFY(!label.isEmpty());

	const QStringList before = childrenTexts(free_root);
	QVERIFY(before.count(label) >= 1);

	auto *x2_strip = stripNamed(QStringLiteral("X2"));
	QVERIFY(x2_strip);
	auto *x2_item = rootChild(0, QStringLiteral("KA"));
	x2_item = childByText(x2_item, QStringLiteral("Cabinet A"));
	x2_item = childByText(x2_item, QStringLiteral("X2"));
	QVERIFY(x2_item);
	const int strips_before = x2_item->childCount();

	QVERIFY(x2_strip->addTerminal(moving));

	m_widget->reload();

		//The terminal changed sides: one child less below the free
		//terminals, one child more below its strip. Every item of the
		//tree was rebuilt by the reload, so the two roots are taken
		//again -- the ones held before are deleted by now.
	auto *free_after = m_tree->topLevelItem(1);
	QVERIFY(free_after);
	auto *x2_after = rootChild(0, QStringLiteral("KA"));
	x2_after = childByText(x2_after, QStringLiteral("Cabinet A"));
	x2_after = childByText(x2_after, QStringLiteral("X2"));
	QVERIFY(x2_after);

	QCOMPARE(childrenTexts(free_after).count(), before.count() - 1);
	QCOMPARE(x2_after->childCount(), strips_before + 1);
	QVERIFY(childrenTexts(x2_after).contains(label));
}

void tst_terminalnavigator::selectsStripAndTerminal()
{
	QSignalSpy spy(m_widget, &TerminalNavigatorWidget::currentStripChanged);
	QVERIFY(spy.isValid());

	auto *x10 = stripX10();
	QVERIFY(x10);
	m_tree->setCurrentItem(x10);

	QVERIFY(m_widget->currentIsStrip());
	QCOMPARE(m_widget->currentStrip()->name(), QStringLiteral("X10"));
	QCOMPARE(m_widget->currentInstallation(), QStringLiteral("KA"));
	QCOMPARE(m_widget->currentLocation(), QStringLiteral("Cabinet A"));
	QCOMPARE(spy.count(), 1);

		//A terminal below a strip belongs to that strip, but the item
		//selected is not a strip itself -- the dock works the same way.
	QVERIFY(x10->childCount());
	m_tree->setCurrentItem(x10->child(0));
	QVERIFY(!m_widget->currentIsStrip());
	QCOMPARE(m_widget->currentStrip()->name(), QStringLiteral("X10"));
	QVERIFY(spy.count() == 1); // same strip: no change signalled

		//A free terminal belongs to no strip.
	auto *free_root = m_tree->topLevelItem(1);
	QVERIFY(free_root->childCount());
	m_tree->setCurrentItem(free_root->child(0));
	QVERIFY(!m_widget->currentIsStrip());
	QVERIFY(!m_widget->currentStrip());
	QCOMPARE(spy.count(), 2);

		//Deselecting everything leaves no strip either.
	m_tree->setCurrentItem(nullptr);
	QVERIFY(!m_widget->currentStrip());
	QCOMPARE(spy.count(), 3);
}

void tst_terminalnavigator::reloadKeepsTheSelectedStrip()
{
	auto *x2 = rootChild(0, QStringLiteral("KA"));
	x2 = childByText(x2, QStringLiteral("Cabinet A"));
	x2 = childByText(x2, QStringLiteral("X2"));
	QVERIFY(x2);
	m_tree->setCurrentItem(x2);
	QCOMPARE(m_widget->currentStrip()->name(), QStringLiteral("X2"));

	m_widget->reload();

	QVERIFY(m_tree->currentItem());
	QCOMPARE(m_tree->currentItem()->text(0), QStringLiteral("X2"));
	QVERIFY(m_widget->currentIsStrip());
	QCOMPARE(m_widget->currentStrip()->name(), QStringLiteral("X2"));

		//No strip selected: no strip to keep, the tree is just rebuilt.
	m_tree->setCurrentItem(nullptr);
	m_widget->reload();
	QCOMPARE(m_tree->topLevelItemCount(), 2);
	QVERIFY(!m_widget->currentStrip());
}

/**
	A terminal placed on a folio shows up below the free terminals, and
	one taken off leaves again. The two signals reach the navigator one
	per element, so they are collected into a single rebuild; and an
	element of any other kind, which leaves the tree untouched, is not
	asked for a rebuild at all.
*/
void tst_terminalnavigator::followsElementsOfTheFolios()
{
	const int free_before = freeTerminalCount();
	QVERIFY(free_before > 1);

	auto terminals = ElementProvider(m_project).freeTerminal();
	QVERIFY(!terminals.isEmpty());

		//A free terminal without a conductor of its own, so taking it
		//off the folio does not disturb the wiring of the project.
	Element *terminal = nullptr;
	for (auto candidate : terminals) {
		if (candidate->conductors().isEmpty()) {
			terminal = candidate;
			break;
		}
	}
	QVERIFY2(terminal, "a free terminal with no conductor");
	const QString label = terminal->actualLabel();
	QVERIFY(!label.isEmpty());
	Diagram *folio = terminal->diagram();
	QVERIFY(folio);
	QVERIFY(childrenTexts(freeRoot()).contains(label));

		//Taken off the folio it is gone from the tree...
	folio->removeItem(terminal);
	QCoreApplication::processEvents();
	QCOMPARE(freeTerminalCount(), free_before - 1);
	QVERIFY(!childrenTexts(freeRoot()).contains(label));

		//...and back on it, it is back in the tree.
	folio->addItem(terminal);
	QCoreApplication::processEvents();
	QCOMPARE(freeTerminalCount(), free_before);
	QVERIFY(childrenTexts(freeRoot()).contains(label));

		//An element of another kind is reported too, but the tree holds
		//no place for it: nothing about the terminals changed, so the
		//free terminals are counted as they were.
	auto others = ElementProvider(m_project).find(ElementData::Simple);
	QVERIFY(!others.isEmpty());
	Element *other = others.first();
	QVERIFY(other);
	Diagram *other_folio = other->diagram();
	QVERIFY(other_folio);
	const int others_before = other_folio->elements().count();

	other_folio->removeItem(other);
	QCoreApplication::processEvents();
	QCOMPARE(freeTerminalCount(), free_before);

	other_folio->addItem(other);
	QCoreApplication::processEvents();
	QCOMPARE(freeTerminalCount(), free_before);
	QCOMPARE(other_folio->elements().count(), others_before);
}

void tst_terminalnavigator::clearsWithoutProject()
{
	m_widget->setProject();
	QVERIFY(!m_widget->project());
	QCOMPARE(m_tree->topLevelItemCount(), 0);

		//A project destroyed while the widget holds it empties the tree
		//on its own -- no nullptr has to be passed first.
	auto *other = new QETProject(QStringLiteral(QET_TEST_PROJECT_PATH));
	other->newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X301"));
	m_widget->setProject(other);
	QCOMPARE(m_tree->topLevelItemCount(), 2);
	QVERIFY(rootChild(0, QStringLiteral("KC")));

	delete other;
	QVERIFY(!m_widget->project());
	QCOMPARE(m_tree->topLevelItemCount(), 0);

		//A project can be set again after that.
	m_widget->setProject(m_project);
	QCOMPARE(m_tree->topLevelItemCount(), 2);
	QVERIFY(rootChild(0, QStringLiteral("KA")));
}

/**
	The tab follows the project the user is really working on, not the
	project opened last: several projects can be open in one editor, and
	switching their tabs has to switch the tree too.
*/
void tst_terminalnavigator::followsTheActiveProject()
{
	QETDiagramEditor *editor = QetAppTest::editor();
	QVERIFY(editor);

	auto *navigator = editor->findChild<TerminalNavigatorWidget *>();
	QVERIFY2(navigator, "the collection panel has no terminal tab");
	auto *tree = navigator->findChild<QTreeWidget *>();
	QVERIFY(tree);

		//Wired up while the widget is built: the editor is already the
		//ancestor of the collection panel then (diagramEditorAncestorOf
		//walks the parents, it does not need a shown window).
	QVERIFY(editor->newProject());
	auto *first = editor->currentProject();
	QVERIFY(first);
	first->setTitle(QStringLiteral("First project"));
	QCOMPARE(navigator->project(), first);
	QCOMPARE(tree->topLevelItemCount(), 2);

		//Opening a second project makes it the active one, and the tree
		//has to follow it -- the project added last used to win.
	QVERIFY(editor->newProject());
	auto *second = editor->currentProject();
	QVERIFY(second);
	QVERIFY(second != first);
	QCOMPARE(navigator->project(), second);
		//The root holds the project title as the tree was built, so the
		//text is only checked after the next switch, once both titles
		//are set.
	second->setTitle(QStringLiteral("Second project"));

		//Going back to the first project's tab switches the tree back.
	editor->activateProject(first);
	QCOMPARE(editor->currentProject(), first);
	QCOMPARE(navigator->project(), first);
	QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("First project"));
	QVERIFY(tree->topLevelItemCount() == 2);

		//And forward again.
	editor->activateProject(second);
	QCOMPARE(editor->currentProject(), second);
	QCOMPARE(navigator->project(), second);
	QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Second project"));

		//The collection model dropping the shown project takes the tab
		//with it (removeProject), and the next activation fills it from
		//the signal again.
	auto *collection = editor->findChild<ElementsCollectionWidget *>();
	QVERIFY(collection);
	collection->removeProject(second);
	QVERIFY(!navigator->project());
	QCOMPARE(tree->topLevelItemCount(), 0);

	editor->activateProject(first);
	QCOMPARE(navigator->project(), first);
	QCOMPARE(tree->topLevelItemCount(), 2);
}

void tst_terminalnavigator::cleanupTestCase()
{
	delete m_widget;
	m_widget = nullptr;
	m_tree = nullptr;
}

QET_APP_TEST_MAIN(tst_terminalnavigator)
#include "tst_terminalnavigator.moc"

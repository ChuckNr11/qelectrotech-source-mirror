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
#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "elementprovider.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>

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

		//The dialogs of the navigator are modal, so every interaction
		//with one has to run from the event loop exec() spins. This
		//helper opens the strip dialog through the toolbar action, hands
		//it over to @p interact, and closes it again -- a watchdog makes
		//sure a dialog that never shows up fails the caller instead of
		//hanging the test.
		//Returns false when no dialog showed up at all.
		//Whatever @p interact does, the dialog does not survive this:
		//accepting it is up to @p interact itself.
	bool interactWithTheStripDialog(const std::function<void(QDialog *)> &interact)
	{
		bool shown = false;

		QTimer watchdog;
		watchdog.setSingleShot(true);
		watchdog.setInterval(3000);
		connect(&watchdog, &QTimer::timeout, this, []() {
			if (auto *modal = QApplication::activeModalWidget()) {
				modal->close();
			}
		});
		watchdog.start();

		QTimer::singleShot(0, this, [&]() {
			auto *dialog = qobject_cast<QDialog *>(
						QApplication::activeModalWidget());
			if (!dialog) {
				return;
			}
			shown = true;
			interact(dialog);
			if (dialog->isVisible()) {
				dialog->close();
			}
		});
		QMetaObject::invokeMethod(m_widget, "on_m_add_strip_triggered");
		watchdog.stop();

		return shown;
	}

		//The name combo box of the strip dialog, else null. Everything
		//the dialog is built from is reached through its object names,
		//which is what this helper and the tests below go by.
	QComboBox *dialogNameCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_name_cb"));
	}

	QLineEdit *dialogIdField(QDialog *dialog) const
	{
		return dialog->findChild<QLineEdit *>(QStringLiteral("m_id_le"));
	}

	QLabel *dialogNotice(QDialog *dialog) const
	{
		return dialog->findChild<QLabel *>(QStringLiteral("m_notice_lb"));
	}

	QDialogButtonBox *dialogButtons(QDialog *dialog) const
	{
		return dialog->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
	}

	QTreeWidget *dialogPreviewTree(QDialog *dialog) const
	{
		return dialog->findChild<QTreeWidget *>(QStringLiteral("m_preview_tree"));
	}

	QComboBox *dialogSymbolCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_symbol_cb"));
	}

		//What the strip dialog ended up like after one round of typing
		//into it: every scenario of checksTheStripDialog() opens the
		//dialog once, fills it, and reads this back out.
	struct StripDialogState {
		bool shown = false;
		bool ok_enabled = true;
		bool notice_shown = true;
		QString notice;
		QStringList names_offered;
	};

		//Opens the strip dialog, names the strip @p name -- typed when
		//@p typed is true, picked from the list of names otherwise --
		//types @p ids into the ID field, and reports the state of the
		//notice label and of the OK button. The dialog is closed again
		//without being accepted, so nothing reaches the project.
	StripDialogState typeIntoTheStripDialog(const QString &name,
											const QString &ids,
											bool typed);

private slots:
	void initTestCase();
	void groupsStripsByInstallationAndLocation();
	void movesTerminalFromFreeToStrip();
	void selectsStripAndTerminal();
	void reloadKeepsTheSelectedStrip();
	void followsElementsOfTheFolios();
	void createsStripThroughTheToolbar();
	void checksTheStripDialog();
	void previewsTheTerminalsInTheTree();
	void createsTheStripWithItsTerminalsOnOk();
	void keepsPendingTerminalsInXml();
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

/**
	The tab creates strips through its own dialog: the toolbar button opens
	it, prefilled with the plant and the location the user is looking at,
	and the strip lands in the project through its own undo command. That
	command also keeps the tree in step, so one undo takes the strip back
	out of the project *and* out of the tree, and a redo puts it back,
	selected.

	The dialog is modal, so it is filled and accepted from the event loop
	exec() runs; a watchdog closes it if it never shows, which fails the
	test instead of hanging it.
*/
void tst_terminalnavigator::createsStripThroughTheToolbar()
{
		//The branch the dialog has to be prefilled with. The location
		//node is selected, which tests both fields at once.
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	const int strips_in_branch = cabinet_a->childCount();

	const int strips_before = m_project->terminalStrip().count();
	const int undo_count_before = m_project->undoStack()->count();
	const int undo_index_before = m_project->undoStack()->index();

	m_tree->setCurrentItem(cabinet_a);

	QString installation_shown, location_shown;
	QStringList installations_offered, locations_offered, names_offered;
	bool notice_shown = false, name_offered = false;
	bool ok_without_name = true, ok_with_name = false;

	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		if (auto *installation = dialog->findChild<QComboBox *>(
					QStringLiteral("m_installation_cb"))) {
			installation_shown = installation->currentText();
			for (int i = 0 ; i < installation->count() ; ++i)
				installations_offered << installation->itemText(i);
		}
		if (auto *location = dialog->findChild<QComboBox *>(
					QStringLiteral("m_location_cb"))) {
			location_shown = location->currentText();
			for (int i = 0 ; i < location->count() ; ++i)
				locations_offered << location->itemText(i);
		}
		if (auto *names = dialogNameCombo(dialog)) {
			for (int i = 0 ; i < names->count() ; ++i)
				names_offered << names->itemText(i);
			name_offered = names->findText(QStringLiteral("X10")) != -1;
			notice_shown = dialogNotice(dialog)->isVisible();
			ok_without_name = dialogButtons(dialog)
						->button(QDialogButtonBox::Ok)->isEnabled();

				//A name of his own, which is what the user types
				//when he asks for a brand new strip.
			QTest::keyClicks(names->lineEdit(), QStringLiteral("X77"));
			ok_with_name = dialogButtons(dialog)
						->button(QDialogButtonBox::Ok)->isEnabled();
		}
		dialog->accept();
	});

		//The dialog opened with the branch the user was looking at...
	QVERIFY2(dialog_found, "the strip dialog was shown and accepted");
	QCOMPARE(installation_shown, QStringLiteral("KA"));
	QCOMPARE(location_shown, QStringLiteral("Cabinet A"));

		//The two fields offer what the project already uses, so the
		//same plant and location are picked rather than typed a second
		//time -- the navigator groups by them, and two look-alike
		//plants are two branches that are hard to tell apart.
	QVERIFY2(installations_offered.contains(QStringLiteral("KA")),
			 qPrintable(installations_offered.join(QLatin1Char(','))));
	QVERIFY(installations_offered.contains(QStringLiteral("KB")));
	QVERIFY(locations_offered.contains(QStringLiteral("Cabinet A")));
	QVERIFY(locations_offered.contains(QStringLiteral("Roof")));

		//The name field offers the strips of that very branch, so an
		//existing one is added to, and a new one is named in a way
		//that does not double an existing one.
	QVERIFY2(name_offered, qPrintable(names_offered.join(QLatin1Char(','))));

		//Nothing is announced while nothing is wrong, and the OK button
		//only frees up once the strip has a name.
	QVERIFY2(!notice_shown, "the notice label starts hidden");
	QVERIFY2(!ok_without_name, "the OK button starts disabled: no name yet");
	QVERIFY2(ok_with_name, "the OK button frees up once a name is typed");

		//...and the strip it built is in the project, in the tree, and
		//selected.
	QCOMPARE(m_project->terminalStrip().count(), strips_before + 1);
	QCOMPARE(m_project->undoStack()->count(), undo_count_before + 1);
	QCOMPARE(m_project->undoStack()->index(), undo_index_before + 1);

	auto *new_strip = m_widget->currentStrip();
	QVERIFY(new_strip);
	QCOMPARE(new_strip->installation(), QStringLiteral("KA"));
	QCOMPARE(new_strip->location(), QStringLiteral("Cabinet A"));

	cabinet_a = childByText(rootChild(0, QStringLiteral("KA")),
							QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	QCOMPARE(cabinet_a->childCount(), strips_in_branch + 1);

		//One undo takes it back out of the project and out of the tree.
		//Nothing the project signals would do that for a strip -- it is
		//no folio item -- so the command reloads the tree itself, and
		//the selection goes with the strip it can no longer show.
	m_project->undoStack()->undo();
	QCOMPARE(m_project->terminalStrip().count(), strips_before);
	QCOMPARE(m_project->undoStack()->index(), undo_index_before);
	QVERIFY(m_widget->currentStrip() == nullptr);
	cabinet_a = childByText(rootChild(0, QStringLiteral("KA")),
							QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	QCOMPARE(cabinet_a->childCount(), strips_in_branch);

		//A redo brings it back, and selects it again.
	m_project->undoStack()->redo();
	QCOMPARE(m_project->terminalStrip().count(), strips_before + 1);
	QVERIFY(m_widget->currentStrip() == new_strip);
	cabinet_a = childByText(rootChild(0, QStringLiteral("KA")),
							QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	QCOMPARE(cabinet_a->childCount(), strips_in_branch + 1);

		//Leave the project the way this test found it.
	m_project->undoStack()->undo();
	QCOMPARE(m_project->terminalStrip().count(), strips_before);
	QCOMPARE(m_project->undoStack()->index(), undo_index_before);
}

/**
	Opens the strip dialog of the navigator, names the strip @p name -- typed
	when @p typed is true, picked from the list of names the branch already
	offers otherwise -- types @p ids into the ID field, and reports what the
	notice label and the OK button ended up like.
	The dialog is never accepted, so the project is left alone: whatever
	these checks say, no strip is created.
*/
tst_terminalnavigator::StripDialogState tst_terminalnavigator::typeIntoTheStripDialog(
		const QString &name, const QString &ids, bool typed)
{
	StripDialogState state;
	state.shown = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *notice = dialogNotice(dialog);
		auto *buttons = dialogButtons(dialog);
		if (!names || !id_field || !notice || !buttons) {
			return;
		}
		auto *ok = buttons->button(QDialogButtonBox::Ok);
		if (!ok) {
			return;
		}

		for (int i = 0 ; i < names->count() ; ++i) {
			state.names_offered << names->itemText(i);
		}

		if (typed) {
				//Typing into the line edit of an editable combo box
				//selects nothing, which is what tells a name typed by
				//the user apart from a name picked from the list.
			QTest::keyClicks(names->lineEdit(), name);
		} else {
				//Picking an entry of the list names an already
				//existing strip, to which the terminals are added.
				//A click in the popup of the combo box selects that
				//entry, which is what this does: setCurrentText()
				//alone would only fill the line edit, exactly like
				//typing would.
			names->setCurrentIndex(names->findText(name));
		}
		id_field->setText(ids);

		state.notice = notice->text();
		state.notice_shown = notice->isVisible();
		state.ok_enabled = ok->isEnabled();
	});

	return state;
}

/**
	The dialog of the navigator validates what the user types before it lets
	him create anything, and reports every finding in its notice label:
	- the ID list has to be readable and free of duplicates;
	- the IDs must not be used already by the strip of that very node;
	- the label 'strip name:ID' must not belong to an independent terminal.
	Nothing is checked about the name: a name which already belongs to a
	strip of the branch leads to that strip, whose terminals are extended,
	so neither typing it nor picking it announces anything.
	The OK button stays disabled while there is a single finding.
*/
void tst_terminalnavigator::checksTheStripDialog()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	const int strips_before = m_project->terminalStrip().count();
	const int undo_count_before = m_project->undoStack()->count();

		//X2 and X10 are the strips of that branch, X10 holding the four
		//terminals initTestCase() gave it -- the first of which sits at
		//the position 1 of the node.
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const int terminals_in_x10 = x10->physicalTerminalCount();
	QVERIFY(terminals_in_x10 > 0);

		//A dialog left alone says nothing, and offers nothing to create:
		//there is no strip without a name.
	auto state = typeIntoTheStripDialog(QString(), QString(), false);
	QVERIFY2(state.shown, "the strip dialog was shown");
	QCOMPARE(state.names_offered,
			 (QStringList{QStringLiteral("X2"), QStringLiteral("X10")}));
	QVERIFY2(!state.notice_shown, "the notice label starts hidden");
	QVERIFY2(!state.ok_enabled, "the OK button stays disabled without a name");

		//A name typed by hand leads to the very same strip a name picked
		//from the list leads to: nothing is announced, and the OK button
		//frees up -- the terminals are added to that strip, and a strip
		//which does not exist yet is created.
	state = typeIntoTheStripDialog(QStringLiteral("X10"), QString(), true);
	QVERIFY(state.shown);
	QVERIFY2(state.notice.isEmpty(), qPrintable(state.notice));
	QVERIFY2(!state.notice_shown, "a name which exists already announces nothing");
	QVERIFY2(state.ok_enabled,
			 "the OK button frees up on a name which exists already");

	state = typeIntoTheStripDialog(QStringLiteral("X10"), QString(), false);
	QVERIFY(state.shown);
	QVERIFY2(state.notice.isEmpty(), qPrintable(state.notice));
	QVERIFY2(!state.notice_shown, "picking a name announces nothing");
	QVERIFY(state.ok_enabled);

		//The ID list may not repeat an ID.
	state = typeIntoTheStripDialog(QStringLiteral("X10"), QStringLiteral("1,1"), false);
	QVERIFY(state.shown);
	QCOMPARE(state.notice, QStringLiteral("ID 1 is entered twice"));
	QVERIFY2(!state.ok_enabled, "the OK button stays disabled on a repeated ID");

		//The ID list may not be readable, ranges included.
	state = typeIntoTheStripDialog(QStringLiteral("X10"), QStringLiteral("1-2-3"), false);
	QVERIFY(state.shown);
	QCOMPARE(state.notice, QStringLiteral("Invalid ID list: 1-2-3"));
	QVERIFY(!state.ok_enabled);

		//An ID the strip of the node already holds, one message per ID
		//so the user can fix them all at once.
	state = typeIntoTheStripDialog(QStringLiteral("X10"), QStringLiteral("1"), false);
	QVERIFY(state.shown);
	QCOMPARE(state.notice,
			 QStringLiteral("ID 1 already exists in terminal block X10"));
	QVERIFY(!state.ok_enabled);

	state = typeIntoTheStripDialog(QStringLiteral("X10"), QStringLiteral("1-2"), false);
	QVERIFY(state.shown);
	QCOMPARE(state.notice,
			 QStringLiteral("ID 1 already exists in terminal block X10\n"
							"ID 2 already exists in terminal block X10"));

		//An ID the strip does not hold is fine, and frees the OK button.
	state = typeIntoTheStripDialog(QStringLiteral("X10"),
								   QString::number(terminals_in_x10 + 1), false);
	QVERIFY(state.shown);
	QVERIFY2(state.notice.isEmpty(), qPrintable(state.notice));
	QVERIFY(state.ok_enabled);

		//The label 'strip name:ID' must not belong to an independent
		//terminal: such a terminal is about to take the very same name.
	auto free_terminals = ElementProvider(m_project).freeTerminal();
	QString free_label, free_name, free_id;
	for (auto *terminal : free_terminals) {
		const auto label = terminal->actualLabel();
		const int colon = label.lastIndexOf(QLatin1Char(':'));
		const auto id = label.mid(colon + 1);
		if (colon <= 0 || id.isEmpty()) {
			continue;
		}
		bool readable = id.size() <= 9;
		for (const auto letter : id) {
			readable = readable && letter.isLetterOrNumber();
		}
		if (!readable) {
			continue;
		}
		free_label = label;
		free_name = label.left(colon);
		free_id = id;
		break;
	}
	if (free_label.isEmpty()) {
		QSKIP("no free terminal of the test project is labelled 'name:id'");
	}
	if (stripNamed(free_name)) {
		QSKIP("the free terminal picked for this check is named like a strip");
	}

	state = typeIntoTheStripDialog(free_name, free_id, true);
	QVERIFY(state.shown);
	QCOMPARE(state.notice,
			 QStringLiteral("Label %1 is already used by an independent terminal")
				 .arg(free_label));
	QVERIFY2(!state.ok_enabled, "the OK button stays disabled on a taken label");

		//Not one of those dialogs was accepted, so the project is still
		//the one this test started from.
	QCOMPARE(m_project->terminalStrip().count(), strips_before);
	QCOMPARE(m_project->undoStack()->count(), undo_count_before);
}

/**
	The strip dialog previews the terminals the OK button would create: the
	strip as the root item of the tree below the description, one child per
	ID of the ID field, updated while the user types.
	A terminal which is not drawn in any folio yet has no symbol to show,
	and the missing icon is how the trees tell it apart from a drawn one --
	the very marking the navigator gives those terminals later on.
	The children carry the symbol to draw them with in Qt::UserRole, which
	is what a drag out of that tree would read.

	The tree lets the user pick children one by one, in ranges or all at
	once, with the mouse or the keyboard (ExtendedSelection).
*/
void tst_terminalnavigator::previewsTheTerminalsInTheTree()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	bool tree_shown = false, root_has_icon = true, children_without_icon = true;
	QString root_text_before, root_text_after, symbol_chosen;
	QStringList children_after, symbols;
	int children_on_broken_list = -1, selected_children = 0;

	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		auto *symbols_cb = dialogSymbolCombo(dialog);
		if (!names || !id_field || !tree || !symbols_cb) {
			return;
		}
		tree_shown = tree->isVisible();

			//The dialog preselects the terminal symbol the project
			//already draws its terminals with, so the children carry
			//that symbol around with them.
		symbol_chosen = symbols_cb->itemData(symbols_cb->currentIndex()).toString();

		auto *root_item = tree->topLevelItem(0);
		if (root_item) {
			root_text_before = root_item->text(0);
			root_has_icon = !root_item->icon(0).isNull();
		}

		QTest::keyClicks(names->lineEdit(), QStringLiteral("X77"));
		id_field->setText(QStringLiteral("1-3"));

		root_item = tree->topLevelItem(0);
		if (root_item) {
			root_text_after = root_item->text(0);
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				auto *child = root_item->child(i);
				children_after << child->text(0);
				symbols << child->data(0, Qt::UserRole).toString();
				children_without_icon = children_without_icon && child->icon(0).isNull();
			}
		}

			//A list which can not be read leaves the last known
			//children alone: a typo does not empty the preview.
		id_field->setText(QStringLiteral("1-2-3"));
		root_item = tree->topLevelItem(0);
		children_on_broken_list = root_item ? root_item->childCount() : -1;

			//A selection the user can build with the mouse or the
			//keyboard, and which survives the typing of a new ID.
		id_field->setText(QStringLiteral("1-3"));
		root_item = tree->topLevelItem(0);
		if (root_item && root_item->childCount() == 3) {
			tree->setCurrentItem(root_item->child(0));
			root_item->child(2)->setSelected(true);
		}
		root_item = tree->topLevelItem(0);
		for (int i = 0 ; i < (root_item ? root_item->childCount() : 0) ; ++i) {
			if (root_item->child(i)->isSelected()) {
				++selected_children;
			}
		}
	});

	QVERIFY2(dialog_found, "the strip dialog was shown");
	QVERIFY2(tree_shown, "the preview tree is shown, below the description");
	QVERIFY2(root_text_before.isEmpty(), "the root item starts with the empty name");
	QVERIFY2(root_has_icon, "the root item shows a terminal strip");
	QCOMPARE(root_text_after, QStringLiteral("X77"));
	QCOMPARE(children_after, (QStringList{QStringLiteral("1"),
										  QStringLiteral("2"),
										  QStringLiteral("3")}));
	QVERIFY2(children_without_icon,
			 "a terminal which is not drawn yet has no symbol in the tree");
	QVERIFY2(!symbol_chosen.isEmpty(),
			 "the dialog preselects the terminal symbol the project draws with");
	QCOMPARE(symbols, (QStringList{symbol_chosen, symbol_chosen, symbol_chosen}));
	QCOMPARE(children_on_broken_list, 3);
	QCOMPARE(selected_children, 2);
}

/**
	The OK button creates the terminals the dialog describes: one per ID,
	labelled 'strip name:ID', and none of them drawn in any folio yet --
	they hold no element, only the label and the symbol the user picked.
	Everything goes into a single macro, so one undo takes the terminals
	and the strip back out again, and a redo puts them back.
	A name which belongs to a strip of the branch adds to that strip
	instead of creating a new one.
*/
void tst_terminalnavigator::createsTheStripWithItsTerminalsOnOk()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	const int strips_before = m_project->terminalStrip().count();
	const int undo_index_before = m_project->undoStack()->index();
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const int terminals_in_x10 = x10->physicalTerminalCount();

		//A name of his own creates the strip and the two terminals of
		//the ID field, neither of them drawn anywhere.
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		if (!names || !id_field) {
			return;
		}
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X77"));
		id_field->setText(QStringLiteral("1-2"));
		dialog->accept();
	});

	QVERIFY2(dialog_found, "the strip dialog was shown and accepted");
	QCOMPARE(m_project->terminalStrip().count(), strips_before + 1);
		//The index of the stack and not its count: a push drops whatever
		//the stack held as undone, so the count may well stay put.
	QCOMPARE(m_project->undoStack()->index(), undo_index_before + 1);

	auto *new_strip = m_widget->currentStrip();
	QVERIFY(new_strip);
	QCOMPARE(new_strip->name(), QStringLiteral("X77"));
	QCOMPARE(new_strip->physicalTerminalCount(), 2);
	for (int i = 0 ; i < new_strip->physicalTerminalCount() ; ++i) {
		const auto physical = new_strip->physicalTerminal(i);
		QCOMPARE(physical->realTerminalCount(), 1);
		const auto terminal = physical->realTerminals().at(0);
		QVERIFY2(!terminal->isElement(), "the terminal is not drawn in any folio yet");
		QCOMPARE(terminal->label(), QStringLiteral("X77:%1").arg(i + 1));
	}

		//The tree shows them the same way: their label, and no symbol.
	cabinet_a = childByText(rootChild(0, QStringLiteral("KA")),
							QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	auto *strip_item = childByText(cabinet_a, QStringLiteral("X77"));
	QVERIFY(strip_item);
	QCOMPARE(childrenTexts(strip_item),
			 (QStringList{QStringLiteral("X77:1"), QStringLiteral("X77:2")}));
	QVERIFY2(strip_item->child(0)->icon(0).isNull(),
			 "a terminal which is not drawn yet has no symbol in the tree");

		//One undo takes the terminals and the strip back out...
	m_project->undoStack()->undo();
	QCOMPARE(m_project->terminalStrip().count(), strips_before);
	QCOMPARE(m_project->undoStack()->index(), undo_index_before);
	QCOMPARE(new_strip->physicalTerminalCount(), 0);

		//...and a redo puts them back.
	m_project->undoStack()->redo();
	QCOMPARE(m_project->terminalStrip().count(), strips_before + 1);
	QCOMPARE(new_strip->physicalTerminalCount(), 2);

		//A name which belongs to a strip of the branch adds to that
		//strip, which is then not created a second time.
	const bool dialog_found_again = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		if (!names || !id_field) {
			return;
		}
		names->setCurrentIndex(names->findText(QStringLiteral("X10")));
		id_field->setText(QStringLiteral("9"));
		dialog->accept();
	});

	QVERIFY(dialog_found_again);
	QCOMPARE(m_project->terminalStrip().count(), strips_before + 1);
	QCOMPARE(x10->physicalTerminalCount(), terminals_in_x10 + 1);
	const auto added = x10->physicalTerminal(terminals_in_x10);
	QVERIFY(added);
	QCOMPARE(added->realTerminals().at(0)->label(), QStringLiteral("X10:9"));
	QVERIFY2(!added->realTerminals().at(0)->isElement(),
			 "the terminal added to an existing strip is not drawn either");

		//Leave the project the way this test found it.
	m_project->undoStack()->undo();
	QCOMPARE(x10->physicalTerminalCount(), terminals_in_x10);
	m_project->undoStack()->undo();
	QCOMPARE(m_project->terminalStrip().count(), strips_before);
	QCOMPARE(m_project->undoStack()->index(), undo_index_before);
}

/**
	A terminal which is not drawn in any folio yet travels through the XML
	of the project: it carries an identity of its own, its label and the
	symbol it should be drawn with -- which is what keeps it alive through a
	save and a load, unlike an element-less terminal of an older project,
	which holds nothing worth remembering and is still dropped.
*/
void tst_terminalnavigator::keepsPendingTerminalsInXml()
{
	QETProject other(QStringLiteral(QET_TEST_PROJECT_PATH));
	auto *strip = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X900"));
	QVERIFY(strip);
	QVERIFY(strip->addTerminal(
				RealTerminal::createPending(QStringLiteral("X900:1"),
											QStringLiteral("embed://terminal.elmt"))));

	QDomDocument document;
	auto xml_strip = strip->toXml(document);
	const auto xml_reals = xml_strip.elementsByTagName(RealTerminal::xmlTagName());
	QCOMPARE(xml_reals.size(), 1);
	const auto xml_real = xml_reals.at(0).toElement();
	QVERIFY2(!xml_real.attribute(QStringLiteral("terminal_uuid")).isEmpty(),
			 "a terminal which is not drawn carries an identity of its own");
	QCOMPARE(xml_real.attribute(QStringLiteral("label")), QStringLiteral("X900:1"));
	QCOMPARE(xml_real.attribute(QStringLiteral("symbol")),
			 QStringLiteral("embed://terminal.elmt"));
	QVERIFY2(xml_real.attribute(QStringLiteral("element_uuid")).isEmpty(),
			 "it holds no element, so it names no element either");

		//Reading it back gives a terminal of the same kind again, with
		//the same identity -- which is what a bridge addressing it by
		//its uuid needs.
	auto *restored = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X900bis"));
	QVERIFY(restored);
	QVERIFY(restored->fromXml(xml_strip));
	const auto terminals = restored->realTerminals();
	QCOMPARE(terminals.count(), 1);
	QVERIFY(!terminals.at(0)->isElement());
	QCOMPARE(terminals.at(0)->label(), QStringLiteral("X900:1"));
	QCOMPARE(terminals.at(0)->symbol(), QStringLiteral("embed://terminal.elmt"));
	QCOMPARE(restored->realTerminalForUuid(terminals.at(0)->uuid()),
			 terminals.at(0));

		//An element-less terminal which carries no identity of its own
		//is what an element deleted in an older version left behind, and
		//is still dropped: making a pending terminal out of it would
		//invent a terminal the user never asked for. fromXml() adds to
		//what a strip already holds, so this one goes into a strip of
		//its own -- else the drop could not be told from the terminals
		//read a moment ago.
	QDomDocument orphan_document;
	auto orphan_strip = orphan_document.createElement(TerminalStrip::xmlTagName());
	auto orphan_layout = orphan_document.createElement(QStringLiteral("layout"));
	auto orphan_physical = orphan_document.createElement(PhysicalTerminal::xmlTagName());
	orphan_physical.appendChild(orphan_document.createElement(RealTerminal::xmlTagName()));
	orphan_layout.appendChild(orphan_physical);
	orphan_strip.appendChild(orphan_layout);

	auto *orphan_target = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X900ter"));
	QVERIFY(orphan_target);
	QVERIFY(orphan_target->fromXml(orphan_strip));
	QVERIFY2(orphan_target->realTerminals().isEmpty(),
			 "an element-less terminal without an identity of its own is left out");
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

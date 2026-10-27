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
#include "TerminalNavigator/diagramevent/diagrameventaddterminal.h"
#include "TerminalNavigator/ui/terminalnavigatorwidget.h"
#include "TerminalNavigator/ui/terminalnavigatorwidgettree.h"
#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "elementprovider.h"
#include "properties/elementdata.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"
#include "undocommand/changeelementinformationcommand.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QRadioButton>
#include <QScopedPointer>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>

#include <memory>

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

/**
	The context menu of the tab is put together for the item under the
	cursor, and only the slot which shows it looks at the pieces
	(@sa TerminalNavigatorWidget::buildContextMenu). The test reads those
	pieces without a real menu in the way, which takes the protected
	method -- and so a subclass of the tab, one of its own.
*/
class MenuNavigator : public TerminalNavigatorWidget
{
	public:
		using TerminalNavigatorWidget::TerminalNavigatorWidget;
		using TerminalNavigatorWidget::buildContextMenu;
};

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

		//The marker of the first column of a row which shows a terminal
		//the strip already holds: such a row is display only, carries no
		//properties of its own and can not be marked at all
		//(@sa AddTerminalStripDialog::ExistingTerminalRole).
	static constexpr int existing_terminal_role = Qt::UserRole + 2;

	/// @return true when @p item shows a terminal the strip holds already.
	static bool showsWhatExists(const QTreeWidgetItem *item)
	{
		return item && item->data(0, existing_terminal_role).toBool();
	}

		//The number an editable size combo box holds: its text, up to
		//the first character which is neither a digit nor the decimal
		//separator -- which is a dot or a comma, since the very value
		//the dialog writes keeps the dot and the text it shows follows
		//the locale (@sa TerminalPropertiesDialog).
	static double typedNumber(const QString &text)
	{
		QString digits;
		for (const auto character : text) {
			if (character.isDigit()
				|| ((character == QLatin1Char('.') || character == QLatin1Char(','))
					&& !digits.isEmpty())) {
				digits += character;
				continue;
			}
			break;
		}
		digits.replace(QLatin1Char(','), QLatin1Char('.'));
		return digits.toDouble();
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

	/// The strip X2, the second strip of the KA / Cabinet A branch.
	QTreeWidgetItem *stripItemX2() const
	{
		auto *ka = rootChild(0, QStringLiteral("KA"));
		auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
		return childByText(cabinet_a, QStringLiteral("X2"));
	}

		//The marks of a selection are weighed one after the other, so
		//every scenario of the rules below starts with a single mark:
		//@p item alone, and the current item beside it the way a click
		//leaves it.
	void selectOnly(QTreeWidgetItem *item)
	{
		QVERIFY(item);
		m_tree->clearSelection();
		m_tree->setCurrentItem(item);
		item->setSelected(true);
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

		//Every dialog of the navigator is modal, so every interaction
		//with one has to run from the event loop exec() spins. This
		//helper triggers @p trigger, hands the dialog which shows up
		//over to @p interact, and closes it again -- a watchdog makes
		//sure a dialog that never shows up fails the caller instead of
		//hanging the test. @p ignore is a dialog which is open already
		//and is not what this is about.
		//Returns false when no dialog showed up at all.
		//Whatever @p interact does, the dialog does not survive this: it
		//is accepted when @p accept is true, since what the caller does
		//with the values it shows is the very thing most tests below are
		//about -- @p interact only has to not close it itself for that.
		//Both timers are local ones: the dialog is opened from inside
		//@p trigger, which does not return before it is closed again, so
		//the interaction always runs while this is on the stack -- and a
		//trigger which opened nothing takes the timers down with it
		//instead of leaving them behind for the dialog of the next test.
	bool interactWithModalDialog(QAction *trigger,
	                             QDialog *ignore,
	                             const std::function<void(QDialog *)> &interact,
	                             bool accept = true)
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

		QTimer runner;
		runner.setSingleShot(true);
		runner.setInterval(0);
		connect(&runner, &QTimer::timeout, this, [&]() {
			auto *dialog = qobject_cast<QDialog *>(
						QApplication::activeModalWidget());
				//A dialog which is open already and is not what this is
				//about shows up here the very same way.
			if (!dialog || dialog == ignore) {
				return;
			}
			shown = true;
			interact(dialog);
			if (dialog->isVisible()) {
				if (accept) {
					dialog->accept();
				} else {
					dialog->close();
				}
			}
		});
		runner.start();

		if (trigger) {
			trigger->trigger();
		}
		watchdog.stop();

		return shown;
	}

		//The dialog of the properties is modal over the strip dialog, so
		//it is opened and closed the same way
		//(@sa interactWithModalDialog). Returns false when no dialog
		//showed up at all, which is what the button of the preview
		//toolbar does as soon as no terminal is marked.
	bool interactWithThePropertiesDialog(
				QDialog *strip_dialog,
				const std::function<void(QDialog *)> &interact,
				bool accept = true)
	{
		return interactWithModalDialog(
					dialogEditTerminalsAction(strip_dialog),
					strip_dialog,
					interact,
					accept);
	}

		//The dialog of the properties the button of the navigator's
		//toolbar opens for the terminals marked in the tree, reached the
		//very same way (@sa TerminalNavigatorWidget::editSelection).
		//Returns false when nothing is marked worth that dialog, which
		//is what keeps the button greyed out.
	bool interactWithTheNavigatorPropertiesDialog(
				const std::function<void(QDialog *)> &interact,
				bool accept = true)
	{
		return interactWithModalDialog(navigatorPropertiesAction(),
		                               nullptr,
		                               interact,
		                               accept);
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

		//The widget finders below reach into the dialog of the properties
		//(@sa TerminalPropertiesDialog), which the strip dialog opens for
		//the terminals the user marked. Its fields are the ones the
		//terminal group box of the element editor has, plus the symbol
		//and the flag which keeps a terminal out of the bill of materials.
	QComboBox *propertiesSymbolCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_symbol_cb"));
	}

	QComboBox *propertiesTypeCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_terminal_type_cb"));
	}

	QComboBox *propertiesFunctionCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_terminal_func_cb"));
	}

	QComboBox *propertiesSectionCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_terminal_section_cb"));
	}

	QComboBox *propertiesWidthCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_terminal_width_cb"));
	}

	QComboBox *propertiesColorCombo(QDialog *dialog) const
	{
		return dialog->findChild<QComboBox *>(QStringLiteral("m_terminal_color_cb"));
	}

	QCheckBox *propertiesExcludeFromBom(QDialog *dialog) const
	{
		return dialog->findChild<QCheckBox *>(QStringLiteral("m_exclude_from_bom_cb"));
	}

	QCheckBox *propertiesAutoNumberLocked(QDialog *dialog) const
	{
		return dialog->findChild<QCheckBox *>(QStringLiteral("m_auto_num_locked_cb"));
	}

	QCheckBox *propertiesPotentialIsolating(QDialog *dialog) const
	{
		return dialog->findChild<QCheckBox *>(QStringLiteral("m_potential_isolating_cb"));
	}

		//The action the button of the toolbar and the entry of the
		//context menu of the navigator share, and what it is for: the
		//dialog of the properties of the terminals marked in the tree
		//(@sa TerminalNavigatorWidget::editSelection).
	QAction *navigatorPropertiesAction() const
	{
		return m_widget->findChild<QAction *>(
					QStringLiteral("m_edit_selection"));
	}

		/// @return the terminals a row of the tree stands for: one row
		/// is one physical terminal of @p strip, which holds one
		/// terminal per level, and the uuid it keeps is the one of that
		/// physical terminal (@sa TerminalNavigatorWidget::
		/// addTerminalStrip).
	QVector<QSharedPointer<RealTerminal>> terminalsOfRow(
			TerminalStrip *strip, QTreeWidgetItem *row) const
	{
		const QVector<QSharedPointer<RealTerminal>> none;
		if (!strip || !row) {
			return none;
		}
		const auto physical = strip->physicalTerminal(
					row->data(0, Qt::UserRole + 1).toUuid());
		return physical ? physical->realTerminals() : none;
	}

		//The two radio buttons which say how the terminals to add are
		//named -- the number of them, or the IDs typed -- and the two
		//inputs they enable (@sa AddTerminalStripDialog::
		//onTerminalsModeChanged).
	QRadioButton *dialogIdRadio(QDialog *dialog) const
	{
		return dialog->findChild<QRadioButton *>(QStringLiteral("m_id_rb"));
	}

	QRadioButton *dialogNumberRadio(QDialog *dialog) const
	{
		return dialog->findChild<QRadioButton *>(QStringLiteral("m_number_rb"));
	}

	QSpinBox *dialogNumberSpin(QDialog *dialog) const
	{
		return dialog->findChild<QSpinBox *>(QStringLiteral("m_number_sb"));
	}

		//The action behind the button of the preview toolbar: it works on
		//the terminals marked in the tree and opens their dialog
		//(@sa AddTerminalStripDialog::onEditTerminalsTriggered).
	QAction *dialogEditTerminalsAction(QDialog *dialog) const
	{
		return dialog->findChild<QAction *>(QStringLiteral("m_edit_terminals"));
	}

		//The dialog opens on the number of terminals; this is how a test
		//says it wants the IDs typed by hand instead, which enables the
		//ID field and lets the preview tree follow it.
	void useTypedIds(QDialog *dialog) const
	{
		if (auto *ids_rb = dialogIdRadio(dialog)) {
			ids_rb->setChecked(true);
		}
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
	void followsEditedTerminalsInTheTree();
	void createsStripThroughTheToolbar();
	void checksTheStripDialog();
	void previewsTheTerminalsInTheTree();
	void createsTheStripWithItsTerminalsOnOk();
	void startsLockedByDefault();
	void listsEveryTerminalSymbolAtOnce();
	void preFillsTheValuesFromTheLastTerminal();
	void appliesThePropertiesToTheMarkedRowsOnly();
	void keepsUntouchedRowsAtTheirDefault();
	void createsTheTerminalsWithTheChosenProperties();
	void opensItsPropertiesForTheTerminalsMarked();
	void keepsItsPropertiesOutOfTheIndependentTerminals();
	void givesThePropertiesToEveryTerminalMarked();
	void keepsPendingTerminalsInXml();
	void keepsTheDataOfTerminalsWhichAreNotDrawnInXml();
	void keepsSavedSymbolsWithoutProjectIds();
	void dragsPendingTerminalsOnly();
	void dragsTheSelectionInTreeOrder();
	void marksOnlyWithinOneSideOfTheTree();
	void marksOnlyWithinOneStrip();
	void offersNewStripOutsideTheFreeTerminals();
	void stripButtonFollowsTheMarkedSide();
	void preFillsNameFromTheStripAndShowsItsTerminals();
	void radioButtonsEnableTheirField();
	void spinBoxTakesWholeNumbersFromZeroToNinetyNine();
	void addsAsManyTerminalsAsTheNumberSays();
	void appendsTheNumberedTerminalsAfterWhatTheStripHolds();
	void skipsTheIdsAFreeTerminalHolds();
	void marksTheTerminalsItAdds();
	void opensItsDialogForTheMarkedTerminals();
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
	The rows of the tree are named by the symbols they are drawn with: a
	terminal edited on a folio renames its row, and a strip renamed in the
	terminal block manager renames its branch. Both arrive through the
	project (@sa QETProject::elementDataChanged, terminalStripChanged) and
	both rebuild the tree the way an element arriving does -- through the
	timer the event loop turns.
*/
void tst_terminalnavigator::followsEditedTerminalsInTheTree()
{
		//A terminal of a strip, renamed the way the properties dialog
		//renames it: through the undo command, so one undo names it
		//back. A terminal which follows a numbering scheme is named by
		//its formula and not by what is typed here, so the row to pick
		//is one named by its label alone.
	auto *x10_strip = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10_strip);
	QSharedPointer<RealTerminal> real_t;
	for (int i = 0 ; i < x10_strip->physicalTerminalCount() && real_t.isNull() ; ++i)
	{
		const auto candidate = x10_strip->physicalTerminal(i)->realTerminals().first();
		auto *candidate_element = candidate ? candidate->element() : nullptr;
		if (candidate_element
			&& candidate_element->elementInformations().value(QStringLiteral("formula")).toString().isEmpty()) {
			real_t = candidate;
		}
	}
	QVERIFY2(real_t, "a terminal named by its label, not by a formula");
	auto *element = real_t->element();
	QVERIFY(element);

	const QString before = real_t->label();
	QVERIFY(!before.isEmpty());
	QVERIFY(childrenTexts(stripX10()).contains(before));

	auto old_info = element->elementInformations();
	auto new_info = old_info;
	new_info.addValue(QStringLiteral("label"), QStringLiteral("X10:42"));
	m_project->undoStack()->push(new ChangeElementInformationCommand(element, old_info, new_info));
	QCoreApplication::processEvents();

	QCOMPARE(real_t->label(), QStringLiteral("X10:42"));
	QVERIFY(!childrenTexts(stripX10()).contains(before));
	QVERIFY(childrenTexts(stripX10()).contains(QStringLiteral("X10:42")));

		//One undo takes the name back and the tree with it.
	m_project->undoStack()->undo();
	QCoreApplication::processEvents();
	QCOMPARE(real_t->label(), before);
	QVERIFY(childrenTexts(stripX10()).contains(before));
	QVERIFY(!childrenTexts(stripX10()).contains(QStringLiteral("X10:42")));

		//A strip renamed is a branch renamed, its own item and nothing
		//else: the rows below it are named after their terminals. The
		//terminal block manager hands the whole data of the strip over
		//after renaming it (ChangeTerminalStripData -> setData); the
		//setter alone says nothing of the change.
	auto *x201 = stripNamed(QStringLiteral("X201"));
	QVERIFY(x201);
	auto *roof = rootChild(0, QStringLiteral("KB"));
	roof = childByText(roof, QStringLiteral("Roof"));
	QVERIFY(roof);
	auto *strip_item = childByText(roof, QStringLiteral("X201"));
	QVERIFY(strip_item);
	const QStringList rows = childrenTexts(strip_item);

	x201->setName(QStringLiteral("X202"));
	x201->setData(x201->data());
	QCoreApplication::processEvents();

	roof = rootChild(0, QStringLiteral("KB"));
	roof = childByText(roof, QStringLiteral("Roof"));
	QVERIFY(roof);
	QVERIFY(childByText(roof, QStringLiteral("X202")));
	QVERIFY(childByText(roof, QStringLiteral("X201")) == nullptr);
	QCOMPARE(childrenTexts(childByText(roof, QStringLiteral("X202"))), rows);

		//Named back, the branch is back under its old name with the
		//same rows -- and the project is left the way the tests after
		//this one find it.
	x201->setName(QStringLiteral("X201"));
	x201->setData(x201->data());
	QCoreApplication::processEvents();

	roof = rootChild(0, QStringLiteral("KB"));
	roof = childByText(roof, QStringLiteral("Roof"));
	QVERIFY(roof);
	QVERIFY(childByText(roof, QStringLiteral("X201")));
	QVERIFY(childByText(roof, QStringLiteral("X202")) == nullptr);
	QCOMPARE(childrenTexts(childByText(roof, QStringLiteral("X201"))), rows);

		//The tests after this one take the undo stack as they found it:
		//empty, and holding nothing undone. What this one pushed and
		//undone is dropped here on purpose -- a push drops the undone
		//commands of a stack, and the counts below would stop matching
		//(@sa createsTheStripWithItsTerminalsOnOk).
	m_project->undoStack()->clear();
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

			//The dialog opens on the number of terminals: these IDs are
			//typed by hand, which is what the other radio button says.
		useTypedIds(dialog);

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
	ID of the ID field, updated while the user types, the ID in the first
	column and the name of the symbol it is drawn with in the second one.
	A terminal which is not drawn in any folio yet has no symbol to show,
	and the missing icon is how the trees tell it apart from a drawn one --
	the very marking the navigator gives those terminals later on.
	The children carry the symbol to draw them with in Qt::UserRole, which
	is what a drag out of that tree would read.

	What a terminal starts with is what the dialog of the properties would
	show for it: the symbol, the sizes and the color of the last terminal
	the strip already holds, when the name field designates a strip which
	holds any -- and nothing at all for a strip which does not exist yet,
	since there is nothing to take them from (@sa AddTerminalStripDialog::
	defaultsForNewTerminals). The tree lets the user pick children one by
	one, in ranges or all at once, with the mouse or the keyboard
	(ExtendedSelection), and the dialog of the properties gives the values
	it shows to the children marked -- and to those only, the others
	keeping what they were given.
*/
void tst_terminalnavigator::previewsTheTerminalsInTheTree()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	bool tree_shown = false, root_has_icon = true, children_without_icon = true;
	bool symbol_column_shown = false, properties_opened = false;
	QString root_text_before, root_text_after, default_of_x10;
	QStringList children_after, symbols_new_strip, symbols_in_column;
	QStringList symbols_of_x10, symbols_after_dialog, symbols_after_new_row;
	int children_on_broken_list = -1, selected_children = 0;

	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !id_field || !tree) {
			return;
		}
		useTypedIds(dialog);
		tree_shown = tree->isVisible();
		symbol_column_shown = tree->columnCount() == 2 && !tree->isHeaderHidden();

		auto *root_item = tree->topLevelItem(0);
		if (root_item) {
			root_text_before = root_item->text(0);
			root_has_icon = !root_item->icon(0).isNull();
		}

			//A strip which does not exist yet holds no terminal to take
			//anything from: the terminals typed next carry no symbol at
			//all, and the dialog of the properties is where they get one.
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X77"));
		id_field->setText(QStringLiteral("1-3"));

		root_item = tree->topLevelItem(0);
		if (root_item) {
			root_text_after = root_item->text(0);
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				auto *child = root_item->child(i);
				children_after << child->text(0);
				symbols_new_strip << child->data(0, Qt::UserRole).toString();
				symbols_in_column << child->text(1);
				children_without_icon = children_without_icon && child->icon(0).isNull();
			}
		}

			//A list which can not be read leaves the last known
			//children alone: a typo does not empty the preview.
		id_field->setText(QStringLiteral("1-2-3"));
		root_item = tree->topLevelItem(0);
		children_on_broken_list = root_item ? root_item->childCount() : -1;

			//A strip which does exist is what the terminals typed next
			//take their values from: X10 holds terminals drawn with a
			//symbol, and the rows below them carry that very symbol, in
			//the second column as well. The rows of what X10 holds are
			//display only and are left out of all of this
			//(@sa fillExistingTerminals).
		id_field->setText(QStringLiteral("1-3"));
		names->setCurrentIndex(names->findText(QStringLiteral("X10")));
		root_item = tree->topLevelItem(0);
		if (root_item) {
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				auto *child = root_item->child(i);
				if (showsWhatExists(child)) {
					continue;
				}
				symbols_of_x10 << child->data(0, Qt::UserRole).toString();
			}
		}
		if (!symbols_of_x10.isEmpty()) {
			default_of_x10 = symbols_of_x10.first();
		}

			//The dialog of the properties gives the terminals marked in
			//the tree what it shows -- and those only: the middle row of
			//the three is the one which is left alone, and the symbol
			//given to the other two is the one which says nothing.
		root_item = tree->topLevelItem(0);
		if (root_item) {
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				auto *child = root_item->child(i);
				if (showsWhatExists(child)) {
					continue;
				}
				child->setSelected(child->text(0) != QStringLiteral("2"));
			}
		}
		properties_opened = interactWithThePropertiesDialog(
					dialog, [&](QDialog *properties) {
			auto *symbols_cb = propertiesSymbolCombo(properties);
			if (symbols_cb) {
				symbols_cb->setCurrentIndex(0);		//(no symbol)
			}
		});

		root_item = tree->topLevelItem(0);
		if (root_item) {
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				auto *child = root_item->child(i);
				if (showsWhatExists(child)) {
					continue;
				}
				symbols_after_dialog << child->data(0, Qt::UserRole).toString();
			}
		}

			//A terminal typed next is worth the values of the last
			//terminal of the strip again: only the two rows the dialog
			//worked on keep what it gave them.
		id_field->setText(QStringLiteral("1-4"));
		root_item = tree->topLevelItem(0);
		if (root_item) {
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				auto *child = root_item->child(i);
				if (showsWhatExists(child)) {
					continue;
				}
				symbols_after_new_row << child->data(0, Qt::UserRole).toString();
			}
		}

			//A selection the user can build with the mouse or the
			//keyboard, and which survives the typing of a new ID: the
			//first and the last of the terminals to be created, never
			//the rows of what the strip holds already -- those are
			//display only and can not be marked at all.
		id_field->setText(QStringLiteral("1-3"));
		tree->clearSelection();
		root_item = tree->topLevelItem(0);
		int first_new = -1, last_new = -1;
		for (int i = 0 ; i < (root_item ? root_item->childCount() : 0) ; ++i) {
			if (showsWhatExists(root_item->child(i))) {
				continue;
			}
			if (first_new < 0) {
				first_new = i;
			}
			last_new = i;
		}
		if (first_new >= 0) {
			root_item->child(first_new)->setSelected(true);
			root_item->child(last_new)->setSelected(true);
		}
		root_item = tree->topLevelItem(0);
		for (int i = 0 ; i < (root_item ? root_item->childCount() : 0) ; ++i) {
			if (!showsWhatExists(root_item->child(i))
				&& root_item->child(i)->isSelected()) {
				++selected_children;
			}
		}
	});

	QVERIFY2(dialog_found, "the strip dialog was shown");
	QVERIFY2(tree_shown, "the preview tree is shown, below the description");
	QVERIFY2(symbol_column_shown,
			 "the second column of the preview shows the symbol of each terminal");
	QVERIFY2(root_text_before.isEmpty(), "the root item starts with the empty name");
	QVERIFY2(root_has_icon, "the root item shows a terminal strip");
	QCOMPARE(root_text_after, QStringLiteral("X77"));
	QCOMPARE(children_after, (QStringList{QStringLiteral("1"),
										  QStringLiteral("2"),
										  QStringLiteral("3")}));
	QVERIFY2(children_without_icon,
			 "a terminal which is not drawn yet has no symbol in the tree");
	QVERIFY2(symbols_new_strip == (QStringList{QString(), QString(), QString()}),
			 "a strip which does not exist yet gives its terminals no symbol");
	QVERIFY2(symbols_in_column == (QStringList{QString(), QString(), QString()}),
			 "the second column of a terminal without a symbol is empty");
	QCOMPARE(children_on_broken_list, 3);
	QVERIFY2(!default_of_x10.isEmpty(),
			 "the terminals typed next start with the symbol of the last terminal of the strip");
	QVERIFY2(properties_opened,
			 "the button of the toolbar opens the dialog of the properties");
	QCOMPARE(symbols_after_dialog, (QStringList{QString(), default_of_x10, QString()}));
	QVERIFY2(symbols_after_new_row == (QStringList{QString(), default_of_x10,
												  QString(), default_of_x10}),
			 "only the terminals the dialog worked on keep what it gave them");
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
		//the ID field, neither of them drawn anywhere. Each of them
		//carries what the dialog of the marked terminals gave it: the
		//first one keeps the symbol the user picked there, the second
		//one is given none.
	QString symbol_kept;
	bool first_dialog_shown = false, second_dialog_shown = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !id_field || !tree) {
			return;
		}
		useTypedIds(dialog);
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X77"));
		id_field->setText(QStringLiteral("1-2"));

			//Both rows are marked the moment they appear, so both are
			//about the same click on the button of the toolbar: the
			//dialog lists the symbols of the project and of the standard
			//collection, and the first one of them is what the user
			//picks for them here.
		first_dialog_shown = interactWithThePropertiesDialog(
					dialog, [&](QDialog *properties) {
			auto *symbols_cb = propertiesSymbolCombo(properties);
			if (!symbols_cb || symbols_cb->count() < 2) {
				return;
			}
			symbol_kept = symbols_cb->itemData(1).toString();
			symbols_cb->setCurrentIndex(1);
		});

			//The second row alone is marked this time, and is given no
			//symbol at all -- the first one keeps what the click above
			//gave it, since the dialog works on the marked rows only.
		auto *root_item = tree->topLevelItem(0);
		if (root_item && root_item->childCount() == 2) {
			root_item->child(0)->setSelected(false);
			root_item->child(1)->setSelected(true);
		}
		second_dialog_shown = interactWithThePropertiesDialog(
					dialog, [&](QDialog *properties) {
			if (auto *symbols_cb = propertiesSymbolCombo(properties)) {
				symbols_cb->setCurrentIndex(0);		//(no symbol)
			}
		});
		dialog->accept();
	});

	QVERIFY2(dialog_found, "the strip dialog was shown and accepted");
	QVERIFY2(first_dialog_shown && second_dialog_shown,
			 "the button of the toolbar opens the dialog of the properties");
	QVERIFY2(!symbol_kept.isEmpty(),
			 "the dialog of the properties lists the symbols of the project");
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
		QCOMPARE(terminal->symbol(),
				 i == 0 ? symbol_kept : QString());
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
		useTypedIds(dialog);
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
	What the dialog of the properties shows first is what a terminal which
	was never worked on is worth: the generic type and the generic function
	-- the two values which leave a terminal the ones its symbol is drawn
	with -- and the automatic numbering locked, which is what the user
	wants from the terminals he creates. The two other flags are off. The
	sizes and the color are the ones of the last terminal the strip holds,
	when there is one, since those are the values a user picked once and is
	the most likely to want again
	(@sa TerminalPropertiesDialog, AddTerminalStripDialog::
	defaultsForNewTerminals).
*/
void tst_terminalnavigator::startsLockedByDefault()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	int type_value = -1, function_value = -1;
	bool locked = false, isolating = false, from_bom = false;
	bool group_box_shown = false, symbol_row_shown = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *spin = dialogNumberSpin(dialog);
		if (!spin) {
			return;
		}
		spin->setValue(1);

		interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			auto *type_cb = propertiesTypeCombo(properties);
			auto *function_cb = propertiesFunctionCombo(properties);
			if (!type_cb || !function_cb) {
				return;
			}
			type_value = type_cb->currentData().toInt();
			function_value = function_cb->currentData().toInt();
			group_box_shown = properties->findChild<QGroupBox *>(
						QStringLiteral("m_terminal_gb")) != nullptr;
			symbol_row_shown = propertiesSymbolCombo(properties) != nullptr
					&& properties->findChild<QLabel *>(
						QStringLiteral("label_symbol")) != nullptr;
			if (auto *check = propertiesAutoNumberLocked(properties)) {
				locked = check->isChecked();
			}
			if (auto *check = propertiesPotentialIsolating(properties)) {
				isolating = check->isChecked();
			}
			if (auto *check = propertiesExcludeFromBom(properties)) {
				from_bom = check->isChecked();
			}
		});
	});

	QVERIFY(dialog_found);
	QVERIFY2(group_box_shown, "the fields of the element editor are grouped as there");
	QVERIFY2(symbol_row_shown, "the symbol a terminal is drawn with is asked for as well");
	QCOMPARE(type_value, int(ElementData::TTGeneric));
	QCOMPARE(function_value, int(ElementData::TFGeneric));
	QVERIFY2(locked, "the dialog opens with the automatic numbering locked");
	QVERIFY2(!isolating && !from_bom,
			 "the two other flags are off for a terminal never worked on");
}

/**
	The symbol combo box of the dialog of the properties holds every
	terminal symbol the moment the dialog opens: the symbols of the project
	are read by the list both dialogs share, and the ones of the standard
	collection are read in the background and added when they are in
	(@sa TerminalSymbolList). The first row of that list is the one which
	says that a terminal is drawn with no symbol at all, and what the user
	types completes a name anywhere in it -- not only at its start.
*/
void tst_terminalnavigator::listsEveryTerminalSymbolAtOnce()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	int listed = -1;
	QString first_data, first_text;
	bool no_symbol_row = false, completes_anywhere = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *spin = dialogNumberSpin(dialog);
		if (!spin) {
			return;
		}
		spin->setValue(1);

		interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			auto *symbols_cb = propertiesSymbolCombo(properties);
			if (!symbols_cb) {
				return;
			}
			listed = symbols_cb->count();
			no_symbol_row = symbols_cb->itemData(0).toString().isEmpty()
					&& !symbols_cb->itemText(0).isEmpty();
			if (listed > 1) {
				first_data = symbols_cb->itemData(1).toString();
				first_text = symbols_cb->itemText(1);
			}
			if (auto *completer = symbols_cb->completer()) {
				completes_anywhere = completer->filterMode() == Qt::MatchContains
						&& completer->caseSensitivity() == Qt::CaseInsensitive;
			}
		});
	});

	QVERIFY(dialog_found);
	QVERIFY2(no_symbol_row, "the list starts with the row which says 'no symbol'");
	QVERIFY2(listed > 1,
			 "the symbols of the project are listed without any interaction");
	QVERIFY2(!first_data.isEmpty() && !first_text.isEmpty(),
			 "the first symbol listed is named and carries a location");
	QVERIFY2(first_data.startsWith(QStringLiteral("embed://"))
			 || first_data.startsWith(QStringLiteral("common://")),
			 "the symbols come as the plain path of their collection, "
			 "with no runtime project id before it");
	QVERIFY2(completes_anywhere,
			 "what is typed anywhere in a name completes a symbol");
}

/**
	What the dialog of the properties gives the terminals it works on is
	what the OK button creates them with -- the symbol, the sizes and the
	color of the last terminal the strip already holds, read back out of
	the dialog of the very strip. Those values are written the way the
	editor of an element writes them, so that the terminal strip editor
	shows them for those terminals alike.
*/
void tst_terminalnavigator::preFillsTheValuesFromTheLastTerminal()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

		//A strip of its own, with two terminals the user gives a symbol,
		//a section, a width and a color -- none of them drawn anywhere.
	QString chosen_symbol, chosen_color;
	bool properties_opened = false;
	const bool created = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !id_field || !tree) {
			return;
		}
		useTypedIds(dialog);
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X88"));
		id_field->setText(QStringLiteral("1-2"));

		properties_opened = interactWithThePropertiesDialog(
					dialog, [&](QDialog *properties) {
			auto *symbols_cb = propertiesSymbolCombo(properties);
			auto *section_cb = propertiesSectionCombo(properties);
			auto *width_cb = propertiesWidthCombo(properties);
			auto *color_cb = propertiesColorCombo(properties);
			if (!symbols_cb || !section_cb || !width_cb || !color_cb) {
				return;
			}
			symbols_cb->setCurrentIndex(1);		//the first symbol listed
			chosen_symbol = symbols_cb->itemData(1).toString();
				//Both sizes are typed, which the editable combo boxes
				//take as well as anything out of their list.
			section_cb->setCurrentText(QStringLiteral("2.5 mm²"));
			width_cb->setCurrentText(QStringLiteral("1.5 mm"));
			color_cb->setCurrentIndex(1);
			chosen_color = color_cb->itemData(1).toString();
		});
		if (properties_opened) {
			dialog->accept();
		}
	});

	QVERIFY2(created, "the strip dialog was shown and accepted");
	QVERIFY2(properties_opened, "the dialog of the properties was shown");
	QVERIFY2(!chosen_symbol.isEmpty(),
			 "the dialog of the properties lists the symbols of the project");
	QVERIFY2(!chosen_color.isEmpty(), "the list of colors holds a color to pick");

	auto *x88 = stripNamed(QStringLiteral("X88"));
	QVERIFY(x88);
	QCOMPARE(x88->physicalTerminalCount(), 2);

		//What those terminals carry is what the next terminals of that
		//strip start with, both in the rows of the preview tree and in
		//the dialog itself.
	QString read_symbol, read_color, read_column;
	QList<QTreeWidgetItem *> new_rows;
	double read_section = -1, read_width = -1;
	const bool reopened = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !id_field || !tree) {
			return;
		}
		useTypedIds(dialog);
		names->setCurrentIndex(names->findText(QStringLiteral("X88")));
		id_field->setText(QStringLiteral("3-4"));

			//X88 holds the two terminals of the first phase, and their
			//rows are display only: what the dialog is about are the
			//two rows below them.
		auto *root_item = tree->topLevelItem(0);
		for (int i = 0 ; i < (root_item ? root_item->childCount() : 0) ; ++i) {
			auto *child = root_item->child(i);
			if (showsWhatExists(child)) {
				continue;
			}
			new_rows << child;
		}
		if (new_rows.count() == 2) {
			read_column = new_rows.at(0)->text(1);
		}

		interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			auto *symbols_cb = propertiesSymbolCombo(properties);
			auto *section_cb = propertiesSectionCombo(properties);
			auto *width_cb = propertiesWidthCombo(properties);
			auto *color_cb = propertiesColorCombo(properties);
			if (!symbols_cb || !section_cb || !width_cb || !color_cb) {
				return;
			}
			read_symbol = symbols_cb->itemData(symbols_cb->currentIndex()).toString();
			read_section = typedNumber(section_cb->currentText());
			read_width = typedNumber(width_cb->currentText());
			read_color = color_cb->itemData(color_cb->currentIndex()).toString();
		});
	});

	QVERIFY2(reopened, "the strip dialog was shown again");
	QCOMPARE(read_symbol, chosen_symbol);
	QCOMPARE(read_section, 2.5);
	QCOMPARE(read_width, 1.5);
	QCOMPARE(read_color, chosen_color);
	QVERIFY2(!read_column.isEmpty(),
			 "the second column of the row names the symbol of that terminal");
}

/**
	The dialog of the properties gives its values to the terminals marked
	in the preview tree -- and to those only. The rows it leaves alone keep
	what they were worth, which is what the second column keeps showing:
	never silently changing what a terminal carries is the whole point of
	the marking (@sa AddTerminalStripDialog::openTerminalsDialog).
*/
void tst_terminalnavigator::appliesThePropertiesToTheMarkedRowsOnly()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	QString chosen_symbol, middle_row;
	QStringList symbols_before, symbols_after;
	bool opened = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !id_field || !tree) {
			return;
		}
		useTypedIds(dialog);
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X81"));
		id_field->setText(QStringLiteral("1-3"));

		auto *root_item = tree->topLevelItem(0);
		if (!root_item || root_item->childCount() != 3) {
			return;
		}
		for (int i = 0 ; i < root_item->childCount() ; ++i) {
			symbols_before << root_item->child(i)->data(0, Qt::UserRole).toString();
		}
		middle_row = symbols_before.value(1);

			//The rows are marked the moment they appear, so the middle
			//one is deselected here: the first and the third are what
			//the dialog is about.
		root_item->child(1)->setSelected(false);

		opened = interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			auto *symbols_cb = propertiesSymbolCombo(properties);
			if (!symbols_cb || symbols_cb->count() < 2) {
				return;
			}
			chosen_symbol = symbols_cb->itemData(1).toString();
			symbols_cb->setCurrentIndex(1);
		});

		root_item = tree->topLevelItem(0);
		for (int i = 0 ; i < root_item->childCount() ; ++i) {
			symbols_after << root_item->child(i)->data(0, Qt::UserRole).toString();
		}
	});

	QVERIFY(dialog_found);
	QVERIFY2(opened, "the dialog of the properties was shown");
	QVERIFY(!chosen_symbol.isEmpty());
	QCOMPARE(symbols_before.size(), 3);
	QCOMPARE(symbols_after.size(), 3);
	QCOMPARE(symbols_after.at(0), chosen_symbol);
	QCOMPARE(symbols_after.at(2), chosen_symbol);
	QVERIFY2(symbols_after.at(1) == middle_row,
			 "the terminal the dialog was not about keeps what it was worth");
}

/**
	A terminal the dialog of the properties was never asked about keeps the
	values of the moment, and the dialog opens on those very values when
	that terminal is marked later on -- never on the values the user gave
	the other ones. What such a terminal is worth follows the strip the
	dialog is about, which the name field can change at any time.
*/
void tst_terminalnavigator::keepsUntouchedRowsAtTheirDefault()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	QString first_before, first_after, second_before, second_dialog_symbol;
	bool opened = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !id_field || !tree) {
			return;
		}
		useTypedIds(dialog);
		names->setCurrentIndex(names->findText(QStringLiteral("X10")));
		id_field->setText(QStringLiteral("1-2"));

			//X10 holds terminals of its own, and their rows are display
			//only: what this test is about are the two rows below them.
		QList<QTreeWidgetItem *> rows;
		auto *root_item = tree->topLevelItem(0);
		for (int i = 0 ; i < (root_item ? root_item->childCount() : 0) ; ++i) {
			if (!showsWhatExists(root_item->child(i))) {
				rows << root_item->child(i);
			}
		}
		if (rows.count() != 2) {
			return;
		}
		first_before = rows.at(0)->data(0, Qt::UserRole).toString();
		second_before = rows.at(1)->data(0, Qt::UserRole).toString();

			//The first terminal is given no symbol at all, which is
			//what the first row of the list says -- the second one is
			//left alone, and keeps the symbol of the last terminal of
			//the strip.
		rows.at(1)->setSelected(false);
		opened = interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			if (auto *symbols_cb = propertiesSymbolCombo(properties)) {
				symbols_cb->setCurrentIndex(0);		//(no symbol)
			}
		});

			//Marking the terminal which was left alone opens the dialog
			//on the values of the moment again.
		rows.clear();
		root_item = tree->topLevelItem(0);
		for (int i = 0 ; i < (root_item ? root_item->childCount() : 0) ; ++i) {
			if (!showsWhatExists(root_item->child(i))) {
				rows << root_item->child(i);
			}
		}
		if (rows.count() != 2) {
			return;
		}
		first_after = rows.at(0)->data(0, Qt::UserRole).toString();
		rows.at(0)->setSelected(false);
		rows.at(1)->setSelected(true);
		interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			if (auto *symbols_cb = propertiesSymbolCombo(properties)) {
				second_dialog_symbol = symbols_cb->itemData(
							symbols_cb->currentIndex()).toString();
			}
		});
	});

	QVERIFY(dialog_found);
	QVERIFY2(opened, "the dialog of the properties was shown");
	QVERIFY2(!second_before.isEmpty(),
			 "the terminals of X10 are drawn with a symbol, so the new ones start with it");
	QVERIFY2(first_before == second_before,
			 "both terminals start with the values of the last terminal of the strip");
	QVERIFY2(first_after.isEmpty(),
			 "the terminal the dialog worked on keeps what it was given");
	QVERIFY2(second_dialog_symbol == second_before,
			 "the terminal the dialog was never about is worth the values of the moment");
}

/**
	What the dialog of the properties shows is what the terminals it works
	on are created with: type, function, section, color and flags, written
	the very way the editor of an element writes them -- which is what
	makes the terminal strip editor show them for those terminals, and what
	a drawn terminal carries over into the element it is drawn with
	(@sa TerminalProperties::applyTo,
	BindTerminalCommand::takenOverData).
*/
void tst_terminalnavigator::createsTheTerminalsWithTheChosenProperties()
{
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	QVERIFY(cabinet_a);
	m_tree->setCurrentItem(cabinet_a);

	QString color_spec;
	bool opened = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *id_field = dialogIdField(dialog);
		if (!names || !id_field) {
			return;
		}
		useTypedIds(dialog);
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X82"));
		id_field->setText(QStringLiteral("1-2"));

		opened = interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
			auto *type_cb = propertiesTypeCombo(properties);
			auto *function_cb = propertiesFunctionCombo(properties);
			auto *section_cb = propertiesSectionCombo(properties);
			auto *color_cb = propertiesColorCombo(properties);
			if (!type_cb || !function_cb || !section_cb || !color_cb) {
				return;
			}
			type_cb->setCurrentIndex(type_cb->findText(QStringLiteral("Fuse")));
			function_cb->setCurrentIndex(
						function_cb->findText(QStringLiteral("Phase")));
			section_cb->setCurrentText(QStringLiteral("2.5 mm²"));
			color_cb->setCurrentIndex(1);
			color_spec = color_cb->itemData(1).toString();
			if (auto *bom_cb = propertiesExcludeFromBom(properties)) {
				bom_cb->setChecked(true);
			}
			if (auto *locked_cb = propertiesAutoNumberLocked(properties)) {
				locked_cb->setChecked(true);
			}
		});
		if (opened) {
			dialog->accept();
		}
	});

	QVERIFY(dialog_found);
	QVERIFY2(opened, "the dialog of the properties was shown");
	QVERIFY(!color_spec.isEmpty());

	auto *x82 = stripNamed(QStringLiteral("X82"));
	QVERIFY(x82);
	QCOMPARE(x82->physicalTerminalCount(), 2);
	for (int i = 0 ; i < x82->physicalTerminalCount() ; ++i) {
		const auto terminal = x82->physicalTerminal(i)->realTerminals().at(0);
		const auto data = terminal->terminalData();
		const auto informations = data.m_informations;

		QVERIFY2(!terminal->isElement(), "the terminal is not drawn in any folio yet");
		QCOMPARE(int(data.terminalType()), int(ElementData::TTFuse));
		QCOMPARE(int(data.terminalFunction()), int(ElementData::TFPhase));
		QCOMPARE(informations.value(RealTerminal::sectionInfoKey()).toString(),
				 QStringLiteral("2.5"));
		QCOMPARE(informations.value(RealTerminal::colorInfoKey()).toString(),
				 color_spec);
		QCOMPARE(informations.value(QStringLiteral("exclude_from_bom")).toString(),
				 QStringLiteral("true"));
		QCOMPARE(informations.value(QStringLiteral("auto_num_locked")).toString(),
				 QStringLiteral("true"));
		QVERIFY2(!informations.value(
					 QStringLiteral("potential_isolating")).isValid(),
				 "a flag the dialog did not set is kept out of the data");
	}
}

/**
	The button of the toolbar and the entry of the context menu are for
	the rows of a terminal strip marked in the tree: what the first of
	them carries is what the dialog of the properties opens on, and what
	its OK button shows is what all of them get
	(@sa TerminalNavigatorWidget::editSelection). The dialog asks for no
	symbol, a terminal which exists already being drawn with the symbol
	of its element (@sa TerminalPropertiesDialog::WithoutSymbol) -- and
	both stay greyed out while nothing is marked at all.
*/
void tst_terminalnavigator::opensItsPropertiesForTheTerminalsMarked()
{
	auto *action = navigatorPropertiesAction();
	auto *x10 = stripX10();
	auto *strip = stripNamed(QStringLiteral("X10"));
	QVERIFY(action);
	QVERIFY(x10);
	QVERIFY(strip);
	QVERIFY(x10->childCount() >= 3);

		//Nothing marked: the dialog has nothing to work on.
	m_tree->clearSelection();
	m_tree->setCurrentItem(nullptr);
	QVERIFY2(!action->isEnabled(),
			 "no terminal marked, no dialog to open");

		//One row of a strip marked, and what that very terminal carries
		//is what the dialog opens on.
	const auto terminals = terminalsOfRow(strip, x10->child(0));
	QVERIFY(!terminals.isEmpty());
	const auto data = terminals.first()->terminalData();
	const auto section_of_data = data.m_informations
			.value(RealTerminal::sectionInfoKey()).toString().toDouble();

	selectOnly(x10->child(0));
	QVERIFY2(action->isEnabled(),
			 "the rows of a terminal strip are worth the dialog");

	bool symbol_row_hidden = false;
	double section = -1;
	int type_value = -1, function_value = -1;
	const bool shown = interactWithTheNavigatorPropertiesDialog(
				[&](QDialog *dialog) {
		const auto symbols_cb = dialog->findChild<QComboBox *>(
					QStringLiteral("m_symbol_cb"));
		const auto symbols_lb = dialog->findChild<QLabel *>(
					QStringLiteral("label_symbol"));
		symbol_row_hidden = symbols_cb && symbols_cb->isHidden()
				&& symbols_lb && symbols_lb->isHidden();
		if (auto *section_cb = propertiesSectionCombo(dialog)) {
			section = typedNumber(section_cb->currentText());
		}
		if (auto *type_cb = propertiesTypeCombo(dialog)) {
			type_value = type_cb->currentData().toInt();
		}
		if (auto *function_cb = propertiesFunctionCombo(dialog)) {
			function_value = function_cb->currentData().toInt();
		}
	}, false);

	QVERIFY(shown);
	QVERIFY2(symbol_row_hidden,
			 "a terminal which exists already is drawn with the symbol "
			 "of its element, so the dialog asks for none");
	QCOMPARE(type_value, int(data.terminalType()));
	QCOMPARE(function_value, int(data.terminalFunction()));
	QCOMPARE(section, section_of_data);

	m_tree->clearSelection();
}

/**
	The independent terminals are what their elements are and belong to
	no terminal strip, so the dialog of the properties could ask them
	nothing: the button of the toolbar and the entry of the context menu
	stay greyed out on that side of the tree, one terminal marked or
	several of them (@sa TerminalNavigatorWidget::updateActionsState).
*/
void tst_terminalnavigator::keepsItsPropertiesOutOfTheIndependentTerminals()
{
	auto *action = navigatorPropertiesAction();
	auto *free_side = freeRoot();
	QVERIFY(action);
	QVERIFY(free_side);
	QVERIFY(free_side->childCount() >= 2);

	selectOnly(free_side->child(0));
	QVERIFY2(!action->isEnabled(),
			 "an independent terminal is what its element is");
	free_side->child(1)->setSelected(true);
	QVERIFY2(!action->isEnabled(),
			 "several independent terminals are no terminal strip either");
	selectOnly(free_side);
	QVERIFY(!action->isEnabled());

		//The entry of the context menu is the very same action, and is
		//there for that side of the tree as well: greyed out, never
		//taken out of the menu (@sa buildContextMenu).
	MenuNavigator navigator;
	navigator.setProject(m_project);
	auto *navigator_tree = navigator.findChild<QTreeWidget *>();
	auto *navigator_action = navigator.findChild<QAction *>(
				QStringLiteral("m_edit_selection"));
	QVERIFY(navigator_tree);
	QVERIFY(navigator_action);
	auto *navigator_free = navigator_tree->topLevelItem(1);
	QVERIFY(navigator_free);
	QVERIFY(navigator_free->childCount());
	navigator_free->child(0)->setSelected(true);
	QScopedPointer<QMenu> menu(
				navigator.buildContextMenu(navigator_free->child(0)));
	QVERIFY(menu);
	QVERIFY2(menu->actions().contains(navigator_action),
			 "the entry of the context menu is there for the side of "
			 "the independent terminals");
	QVERIFY2(!navigator_action->isEnabled(),
			 "and greyed out the way the button of the toolbar is");

		//A trigger which was never worth anything opens nothing.
	bool opened = false;
	const bool shown = interactWithTheNavigatorPropertiesDialog(
				[&](QDialog *) { opened = true; }, false);
	QVERIFY(!shown);
	QVERIFY(!opened);

	m_tree->clearSelection();
}

/**
	The OK button of the dialog gives every terminal marked the values it
	shows, and leaves the ones which are not marked exactly what they
	were (@sa TerminalNavigatorWidget::editSelection). What a terminal is
	worth is written where it keeps its own -- the element it is drawn
	with, or the copy it carries while it is drawn in no folio yet -- and
	one undo step takes the whole of it back (@sa ChangeTerminalData).
*/
void tst_terminalnavigator::givesThePropertiesToEveryTerminalMarked()
{
	auto *x10 = stripX10();
	auto *strip = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	QVERIFY(strip);
	QVERIFY(x10->childCount() >= 3);

	const auto undo_index_before = m_project->undoStack()->index();
	const auto marked_first = terminalsOfRow(strip, x10->child(0));
	const auto marked_second = terminalsOfRow(strip, x10->child(1));
	const auto untouched = terminalsOfRow(strip, x10->child(2));
	QVERIFY(!marked_first.isEmpty());
	QVERIFY(!marked_second.isEmpty());
	QVERIFY(!untouched.isEmpty());

	const auto section_of = [](const QSharedPointer<RealTerminal> &terminal) {
		return terminal->terminalData().m_informations
				.value(RealTerminal::sectionInfoKey()).toString();
	};
	const auto bom_of = [](const QSharedPointer<RealTerminal> &terminal) {
		return terminal->terminalData().m_informations
				.value(QStringLiteral("exclude_from_bom")).toString();
	};

	const QString first_before = section_of(marked_first.first());
	const QString second_before = section_of(marked_second.first());
	const QString untouched_section = section_of(untouched.first());
	const int untouched_type = int(
				untouched.first()->terminalData().terminalType());

		//The first two rows are marked, the third one is not: it keeps
		//what it carries while the two others are given what the dialog
		//shows.
	selectOnly(x10->child(0));
	x10->child(1)->setSelected(true);

	bool opened = false;
	const bool shown = interactWithTheNavigatorPropertiesDialog(
				[&](QDialog *dialog) {
		opened = true;
		if (auto *type_cb = propertiesTypeCombo(dialog)) {
			type_cb->setCurrentIndex(
						type_cb->findText(QStringLiteral("Fuse")));
		}
		if (auto *section_cb = propertiesSectionCombo(dialog)) {
			section_cb->setCurrentText(QStringLiteral("7.5 mm²"));
		}
		if (auto *bom_cb = propertiesExcludeFromBom(dialog)) {
			bom_cb->setChecked(true);
		}
	});

	QVERIFY(shown);
	QVERIFY(opened);
	for (const auto &terminal : {marked_first.first(), marked_second.first()}) {
		QCOMPARE(int(terminal->terminalData().terminalType()),
				 int(ElementData::TTFuse));
		QCOMPARE(section_of(terminal), QStringLiteral("7.5"));
		QCOMPARE(bom_of(terminal), QStringLiteral("true"));
	}
	QVERIFY2(section_of(untouched.first()) == untouched_section
			 && int(untouched.first()->terminalData().terminalType()) == untouched_type,
			 "the terminal which was not marked keeps what it carried");
	QVERIFY2(bom_of(untouched.first()) != QStringLiteral("true"),
			 "the flag the dialog set is on the terminals marked alone");

		//One click on OK is one step back out of the undo stack.
	m_project->undoStack()->undo();
	QCOMPARE(m_project->undoStack()->index(), undo_index_before);
	QCOMPARE(section_of(marked_first.first()), first_before);
	QCOMPARE(section_of(marked_second.first()), second_before);
	QVERIFY2(bom_of(marked_first.first()) != QStringLiteral("true"),
			 "the undo takes the flag away with everything else");

		//The change and its undo reach the tree through a timer
		//(@sa TerminalNavigatorWidget::onElementChanged), which takes
		//the rows away and builds new ones: let that reload happen here,
		//where no test holds one of those rows in its hand.
	QTest::qWait(50);

	m_tree->clearSelection();
}

/**
	A terminal which is not drawn in any folio yet travels through the XML
	of the project: it carries an identity of its own, its label and the
	symbol it should be drawn with -- and each of those terminals keeps a
	symbol of its own, two terminals of one strip being drawn with two
	different symbols. That is what keeps it alive through a save and a
	load, unlike an element-less terminal of an older project, which holds
	nothing worth remembering and is still dropped.
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
		//Each terminal carries a symbol of its own: two terminals of one
		//strip are drawn with two different symbols.
	QVERIFY(strip->addTerminal(
				RealTerminal::createPending(
					QStringLiteral("X900:2"),
					QStringLiteral("common://cabinet/other_terminal.elmt"))));

	QDomDocument document;
	auto xml_strip = strip->toXml(document);
	const auto xml_reals = xml_strip.elementsByTagName(RealTerminal::xmlTagName());
	QCOMPARE(xml_reals.size(), 2);
	for (int i = 0 ; i < xml_reals.size() ; ++i) {
		const auto xml_real = xml_reals.at(i).toElement();
		QVERIFY2(!xml_real.attribute(QStringLiteral("terminal_uuid")).isEmpty(),
				 "a terminal which is not drawn carries an identity of its own");
		QCOMPARE(xml_real.attribute(QStringLiteral("label")),
				 QStringLiteral("X900:%1").arg(i + 1));
		QCOMPARE(xml_real.attribute(QStringLiteral("symbol")),
				 i == 0 ? QStringLiteral("embed://terminal.elmt")
						: QStringLiteral("common://cabinet/other_terminal.elmt"));
		QVERIFY2(xml_real.attribute(QStringLiteral("element_uuid")).isEmpty(),
				 "it holds no element, so it names no element either");
	}

		//Reading it back gives a terminal of the same kind again, with
		//the same identity -- which is what a bridge addressing it by
		//its uuid needs.
	auto *restored = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X900bis"));
	QVERIFY(restored);
	QVERIFY(restored->fromXml(xml_strip));
	const auto terminals = restored->realTerminals();
	QCOMPARE(terminals.count(), 2);
	for (int i = 0 ; i < terminals.count() ; ++i) {
		QVERIFY(!terminals.at(i)->isElement());
		QCOMPARE(terminals.at(i)->label(), QStringLiteral("X900:%1").arg(i + 1));
		QCOMPARE(terminals.at(i)->symbol(),
				 i == 0 ? QStringLiteral("embed://terminal.elmt")
						: QStringLiteral("common://cabinet/other_terminal.elmt"));
	}
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

/**
	The data the terminal strip editor edits for a terminal which is
	drawn in no folio yet are its own, saved with it the way a spare
	terminal saves its own: type, function, LED and informations -- and
	with those informations the label the terminal carries, since
	label() reads it back from them.

	Nothing of it names an element: the terminal holds no element, so
	its identity stays its own, and reading it back gives a terminal of
	the same kind with the same data, still drawn nowhere, still not in
	the nomenclature of the project -- which counts the spare terminals
	and the terminals drawn on folios only.
*/
void tst_terminalnavigator::keepsTheDataOfTerminalsWhichAreNotDrawnInXml()
{
	QETProject other(QStringLiteral(QET_TEST_PROJECT_PATH));
	auto *strip = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X901"));
	QVERIFY(strip);
	auto pending = RealTerminal::createPending(
				QStringLiteral("X901:1"), QStringLiteral("embed://terminal.elmt"));
	QVERIFY(pending);
	QVERIFY(strip->addTerminal(pending));

		//The terminal strip editor edits the row of that terminal: label,
		//type, function, all of it stored with the terminal itself
	ElementData data;
	data.setTerminalType(ElementData::TTFuse);
	data.setTerminalFunction(ElementData::TFPhase);
	data.setTerminalLED(true);
	data.m_informations.addValue(QStringLiteral("label"), QStringLiteral("X901:7"));
	data.m_informations.addValue(RealTerminal::sectionInfoKey(),
								 QStringLiteral("2.5"));
	pending->setSpareData(data);

	QVERIFY2(pending->isPending(), "the terminal is drawn in no folio");
	QVERIFY(!pending->isSpare());
	QCOMPARE(pending->label(), QStringLiteral("X901:7"));
	QCOMPARE(pending->type(), ElementData::TTFuse);
	QCOMPARE(pending->section(), 2.5);

	QDomDocument document;
	auto xml_strip = strip->toXml(document);
	const auto xml_real = xml_strip.elementsByTagName(RealTerminal::xmlTagName())
						  .at(0).toElement();
	QVERIFY2(!xml_real.attribute(QStringLiteral("terminal_uuid")).isEmpty(),
			 "it holds an identity of its own, not the one of an element");
	QVERIFY(xml_real.attribute(QStringLiteral("element_uuid")).isEmpty());
	QCOMPARE(xml_real.attribute(QStringLiteral("label")), QStringLiteral("X901:7"));
	QCOMPARE(xml_real.attribute(QStringLiteral("type")),
			 ElementData::terminalTypeToString(ElementData::TTFuse));
	QCOMPARE(xml_real.attribute(QStringLiteral("function")),
			 ElementData::terminalFunctionToString(ElementData::TFPhase));
	QVERIFY2(!xml_real.firstChildElement(QStringLiteral("informations")).isNull(),
			 "its informations are saved with it");

	auto *restored = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X901bis"));
	QVERIFY(restored);
	QVERIFY(restored->fromXml(xml_strip));
	const auto terminals = restored->realTerminals();
	QCOMPARE(terminals.count(), 1);
	const auto restored_t = terminals.at(0);
	QVERIFY(restored_t->isPending());
	QVERIFY(!restored_t->isSpare());
	QVERIFY(!restored_t->isElement());
	QCOMPARE(restored_t->uuid(), pending->uuid());
	QCOMPARE(restored_t->symbol(), QStringLiteral("embed://terminal.elmt"));
	QCOMPARE(restored_t->label(), QStringLiteral("X901:7"));
	QCOMPARE(restored_t->type(), ElementData::TTFuse);
	QCOMPARE(restored_t->function(), ElementData::TFPhase);
	QVERIFY(restored_t->isLed());
	QCOMPARE(restored_t->section(), 2.5);
}

/**
	Symbols are saved without the runtime project id of the session which
	wrote them, and read back the same way: such an id is handed out
	afresh in every session and never reused (@sa
	QETApp::registerProject), so 'symbol="project0+embed://..."' of an
	older save names another project, or no project at all, in almost
	every later session -- and a terminal which could not resolve its own
	symbol was never drawn again.

	Both directions are checked here. A terminal built with such an old
	string keeps the plain path, since createPending() is the one place
	symbols are set; what goes into the file carries no id either, so a
	project saved once by this code heals itself for every session
	afterwards.
*/
void tst_terminalnavigator::keepsSavedSymbolsWithoutProjectIds()
{
	QETProject other(QStringLiteral(QET_TEST_PROJECT_PATH));
	auto *strip = other.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X902"));
	QVERIFY(strip);

		//A symbol the way an older version saved it: the id of the
		//session which wrote it still in front of the path
	const auto old_symbol_ = QStringLiteral("project0+embed://terminal.elmt");
	auto pending = RealTerminal::createPending(QStringLiteral("X902:1"), old_symbol_);
	QVERIFY(pending);
	QVERIFY2(pending->symbol() == QStringLiteral("embed://terminal.elmt"),
			 "the id of a session is dropped when the terminal is built");
	QVERIFY(strip->addTerminal(pending));

		//What is written into the project carries no id either
	QDomDocument document;
	auto xml_strip = strip->toXml(document);
	const auto xml_real = xml_strip.elementsByTagName(RealTerminal::xmlTagName())
						  .at(0).toElement();
	QCOMPARE(xml_real.attribute(QStringLiteral("symbol")),
			 QStringLiteral("embed://terminal.elmt"));

		//And a project saved by an older version reads back the same
		//way, read path included: fromXml() builds its terminals with
		//createPending(), which drops the id too. The element is built
		//here the way an old file holds one, no project registered at
		//all -- the path is bound back to a project when it is resolved
		//for a placement, never by the reader.
	QDomDocument old_document;
	auto old_elmt = old_document.createElement(RealTerminal::xmlTagName());
	old_elmt.setAttribute(QStringLiteral("terminal_uuid"),
						  QUuid::createUuid().toString());
	old_elmt.setAttribute(QStringLiteral("label"), QStringLiteral("X902:9"));
	old_elmt.setAttribute(QStringLiteral("symbol"), old_symbol_);
	const auto restored = RealTerminal::pendingFromXml(old_elmt);
	QVERIFY(restored);
	QCOMPARE(restored->symbol(), QStringLiteral("embed://terminal.elmt"));
	QCOMPARE(restored->label(), QStringLiteral("X902:9"));
}

/**
	The tree of the tab drags one thing only: a terminal of a strip which
	is not drawn in any folio yet. What a drag carries is asked for
	through pendingTerminalMimeForItem() -- the tree hands the item over
	to the widget, which refuses every item which is not such a terminal
	-- and the payload holds the strip and the terminal by uuid, which is
	what the drop on the folio reads back.

	The item of a strip holds every level of one physical terminal, so
	the flag which offers the drag is on the item only when one of its
	terminals waits for its symbol: a strip whose terminals are all
	drawn answers nothing at all, and drags nothing either.
*/
void tst_terminalnavigator::dragsPendingTerminalsOnly()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto terminals_in_x10 = x10->realTerminals().count();
	auto *strip_item = stripX10();
	QVERIFY(strip_item);
	QCOMPARE(strip_item->childCount(), int(terminals_in_x10));

		//Every terminal of X10 is drawn on a folio: their items carry
		//the icon of a drawn terminal, offer no drag, and the widget
		//answers nothing to a question about them.
	QMimeData mime;
	for (int i = 0 ; i < strip_item->childCount() ; ++i) {
		auto *item = strip_item->child(i);
		QVERIFY(!item->icon(0).isNull());
		QVERIFY2(!(item->flags() & Qt::ItemIsDragEnabled),
				 "a terminal already drawn drags nothing");
		QVERIFY(!m_widget->pendingTerminalMimeForItem(item, &mime));
		QVERIFY(mime.formats().isEmpty());
	}

		//One terminal of the strip which waits for its symbol: its item
		//carries no icon -- the missing icon is what tells the two
		//apart -- offers the drag, and the payload it carries is the
		//strip and the terminal itself, both by uuid.
	auto pending = RealTerminal::createPending(
				QStringLiteral("X10:99"), QStringLiteral("embed://terminal.elmt"));
	QVERIFY(pending);
	QVERIFY(x10->addTerminal(pending));
	m_widget->reload();

	strip_item = stripX10();
	QVERIFY(strip_item);
	QCOMPARE(strip_item->childCount(), int(terminals_in_x10) + 1);
	auto *pending_item = childByText(strip_item, QStringLiteral("X10:99"));
	QVERIFY2(pending_item, "the terminal waiting for its symbol is in the tree");
	QVERIFY(pending_item->icon(0).isNull());
	QVERIFY(pending_item->flags() & Qt::ItemIsDragEnabled);

	QVERIFY(m_widget->pendingTerminalMimeForItem(pending_item, &mime));

	QUuid strip_uuid, terminal_uuid;
	QVERIFY(DiagramEventAddTerminal::readPendingTerminalMime(
				&mime, &strip_uuid, &terminal_uuid));
	QCOMPARE(strip_uuid, x10->uuid());
	QCOMPARE(terminal_uuid, pending->uuid());
		//The payload is the format of the event it starts, and nothing
		//else: no text behind it for another tool to read by mistake
	QVERIFY(!mime.formats().contains(QStringLiteral("text/plain")));

		//And the strip is left the way this test found it: the terminal
		//taken out again, no undo to keep the project of the tests
		//below as it was. The tree is rebuilt by the reload, so every
		//item held before is deleted and has to be taken again.
	QVERIFY(x10->removeTerminal(pending));
	m_widget->reload();
	strip_item = stripX10();
	QVERIFY(strip_item);
	QCOMPARE(strip_item->childCount(), int(terminals_in_x10));
	QVERIFY(!m_widget->pendingTerminalMimeForItem(strip_item->child(0), &mime));
}

/**
	Several terminals of the tree are marked to be drawn in one run, and
	the drag carries them all in the order the tree shows them -- top to
	bottom, whatever order the clicks marked them in, which is the order
	the run draws them in.

	The items which hold no terminal to be drawn leave no line of
	payload: a selection may well cover the strip itself, and terminals
	already drawn besides the ones waiting, and still only those waiting
	are carried.
*/
void tst_terminalnavigator::dragsTheSelectionInTreeOrder()
{
		//The selection itself: several items at once, taken with Ctrl
		//and Shift, is what makes a run draggable at all
	QVERIFY2(m_tree->selectionMode() == QAbstractItemView::ExtendedSelection,
			 "the tree takes a selection of several items");
	auto *tree_ = qobject_cast<TerminalNavigatorTree *>(m_tree);
	QVERIFY2(tree_, "the tree of the navigator is the one which drags");

	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	auto *strip_item = stripX10();
	QVERIFY(strip_item);
	const int terminals_in_x10 = strip_item->childCount();

	auto pending_a = RealTerminal::createPending(
				QStringLiteral("X10:98"), QStringLiteral("embed://terminal.elmt"));
	auto pending_b = RealTerminal::createPending(
				QStringLiteral("X10:99"), QStringLiteral("embed://terminal.elmt"));
	QVERIFY(pending_a);
	QVERIFY(pending_b);
	QVERIFY(x10->addTerminal(pending_a));
	QVERIFY(x10->addTerminal(pending_b));
	m_widget->reload();

	strip_item = stripX10();
	QVERIFY(strip_item);
	QCOMPARE(strip_item->childCount(), terminals_in_x10 + 2);
	auto *item_a = childByText(strip_item, QStringLiteral("X10:98"));
	auto *item_b = childByText(strip_item, QStringLiteral("X10:99"));
	QVERIFY2(item_a, "the terminal waiting for its symbol is in the tree");
	QVERIFY2(item_b, "the terminal waiting for its symbol is in the tree");

		//The marks come in bottom first, the way a user picks the
		//lower terminal before going back up to the higher one, and the
		//strip is marked as well: the answer is still the order of the
		//tree, parents before children.
	m_tree->clearSelection();
	item_b->setSelected(true);
	item_a->setSelected(true);
	strip_item->setSelected(true);

		//What the tree order is for those marks, read the way the tree
		//walks itself: the strip first, then its selected children in
		//their own order -- whatever that order is, and whatever order
		//the marks came in.
	QList<QTreeWidgetItem *> expected_children;
	for (int i = 0 ; i < strip_item->childCount() ; ++i) {
		auto *child = strip_item->child(i);
		if (child == item_a || child == item_b) {
			expected_children.append(child);
		}
	}
	QCOMPARE(expected_children.count(), 2);

	const auto ordered = tree_->selectedItemsInOrder();
	QCOMPARE(ordered.count(), 3);
	QVERIFY2(ordered.constFirst() == strip_item,
			 "the tree order puts a parent before its children");
	QVERIFY(ordered.at(1) == expected_children.constFirst());
	QVERIFY(ordered.at(2) == expected_children.constLast());

		//The payload follows that very order, and only the two
		//terminals which wait for their symbol leave a line: the strip
		//which is marked above them drags nothing of its own.
	QMimeData mime;
	QVERIFY(m_widget->pendingTerminalsMimeForItems(ordered, &mime));

	QVector<QPair<QUuid, QUuid>> carried;
	QVERIFY(DiagramEventAddTerminal::readPendingTerminalsMime(&mime, &carried));
	QCOMPARE(carried.count(), 2);
	const auto first_pending = expected_children.constFirst() == item_a
			? pending_a : pending_b;
	const auto second_pending = expected_children.constFirst() == item_a
			? pending_b : pending_a;
	QCOMPARE(carried.at(0).first, x10->uuid());
	QCOMPARE(carried.at(0).second, first_pending->uuid());
	QCOMPARE(carried.at(1).first, x10->uuid());
	QCOMPARE(carried.at(1).second, second_pending->uuid());
		//No text behind the payload for another tool to read by mistake
	QVERIFY(!mime.formats().contains(QStringLiteral("text/plain")));

		//And a selection which holds nothing to draw drags nothing: no
		//answer to the tree at all, so no drag is even offered.
	m_tree->clearSelection();
	strip_item->setSelected(true);
	QMimeData empty;
	QVERIFY(!m_widget->pendingTerminalsMimeForItems(
				tree_->selectedItemsInOrder(), &empty));
	QVERIFY(empty.formats().isEmpty());

		//The strip is left the way this test found it: the two
		//terminals taken out again, no undo to keep the project of the
		//tests below as it was. The tree is rebuilt by the reload, so
		//every item held before is deleted and has to be taken again.
	QVERIFY(x10->removeTerminal(pending_a));
	QVERIFY(x10->removeTerminal(pending_b));
	m_widget->reload();
	strip_item = stripX10();
	QVERIFY(strip_item);
	QCOMPARE(strip_item->childCount(), terminals_in_x10);
}

/**
	The marks stay on one side of the tree at a time: the project with
	its strips on one side, the independent terminals on the other. The
	mark which just came in says which side is meant -- it is the one the
	user clicked or the keyboard landed on -- and takes the marks of the
	other side away, the way back the same. Their own root belongs to
	their side as a whole, so it is marked beside one of them.
*/
void tst_terminalnavigator::marksOnlyWithinOneSideOfTheTree()
{
	auto *project_side = m_tree->topLevelItem(0);
	auto *free_side = freeRoot();
	auto *x10 = stripX10();
	auto *free_row = freeRoot() ? freeRoot()->child(0) : nullptr;
	QVERIFY(project_side);
	QVERIFY(free_side);
	QVERIFY(x10);
	QVERIFY(free_row);

		//A strip marked, then an independent terminal beside it: the
		//strip goes, and with it the side it stands for.
	selectOnly(x10);
	free_row->setSelected(true);
	QVERIFY(free_row->isSelected());
	QVERIFY(!x10->isSelected());
	QCOMPARE(m_tree->selectedItems().count(), 1);

		//The same the other way round: the independent terminal goes
		//when the strip is marked afterwards.
	selectOnly(free_row);
	x10->setSelected(true);
	QVERIFY(x10->isSelected());
	QVERIFY(!free_row->isSelected());
	QCOMPARE(m_tree->selectedItems().count(), 1);

		//A row of a strip beside an independent terminal is the same
		//question, only from the other end.
	selectOnly(free_row);
	x10->child(0)->setSelected(true);
	QVERIFY(x10->child(0)->isSelected());
	QVERIFY(!free_row->isSelected());

		//Their own root is the side too: an independent terminal and
		//the root of them are one scope, so both stay marked.
	selectOnly(free_row);
	free_side->setSelected(true);
	QVERIFY(free_row->isSelected());
	QVERIFY(free_side->isSelected());

	m_tree->clearSelection();
}

/**
	Inside the project several items are marked together only within one
	terminal strip: the strip itself with its rows, or rows of it. A mark
	in a second strip takes the marks of the first one away, and a branch
	-- the project, an installation, a location -- is a mark of its own.
*/
void tst_terminalnavigator::marksOnlyWithinOneStrip()
{
	auto *x10 = stripX10();
	auto *x2 = stripItemX2();
	QVERIFY(x10);
	QVERIFY(x2);
	QCOMPARE(x10->childCount(), 4);
	QVERIFY(x2->childCount());

		//Two rows of one strip stay marked together.
	selectOnly(x10->child(0));
	x10->child(1)->setSelected(true);
	QVERIFY(x10->child(0)->isSelected());
	QVERIFY(x10->child(1)->isSelected());

		//The strip with a row of its own: one scope, so both stay.
	selectOnly(x10);
	x10->child(0)->setSelected(true);
	QVERIFY(x10->isSelected());
	QVERIFY(x10->child(0)->isSelected());

		//A row of a second strip takes the marks of the first strip
		//away -- and the strip itself with them.
	selectOnly(x10);
	x2->child(0)->setSelected(true);
	QVERIFY(x2->child(0)->isSelected());
	QVERIFY(!x10->isSelected());

		//An installation is a mark of its own: a strip marked beside
		//it is not kept, the strip was the one just clicked in.
	auto *ka = rootChild(0, QStringLiteral("KA"));
	QVERIFY(ka);
	selectOnly(ka);
	x10->setSelected(true);
	QVERIFY(x10->isSelected());
	QVERIFY(!ka->isSelected());

	m_tree->clearSelection();
}

/**
	"New terminal strip" is offered everywhere but on the side of the
	independent terminals: they belong to no strip, and a strip made
	below their root would stand in their branch for nothing. The two
	other entries are there on both sides.
*/
void tst_terminalnavigator::offersNewStripOutsideTheFreeTerminals()
{
	MenuNavigator navigator;
	navigator.setProject(m_project);
	auto *tree = navigator.findChild<QTreeWidget *>();
	QVERIFY(tree);
	auto *add_strip = navigator.findChild<QAction *>(
				QStringLiteral("m_add_strip"));
	QVERIFY(add_strip);

	auto *free_side = tree->topLevelItem(1);
	QVERIFY(free_side);
	QVERIFY(free_side->childCount());

		//The free terminals and the root of them: no strip is made
		//below either of them, and the three other entries are there.
	for (auto *item : {free_side, free_side->child(0)})
	{
		QScopedPointer<QMenu> menu(navigator.buildContextMenu(item));
		QVERIFY(menu);
		QVERIFY(!menu->actions().contains(add_strip));
		QCOMPARE(menu->actions().count(), 3);
	}

		//The project side, root branch and row alike: the strip is
		//offered everywhere.
	auto *project_side = tree->topLevelItem(0);
	auto *ka = childByText(project_side, QStringLiteral("KA"));
	auto *cabinet_a = childByText(ka, QStringLiteral("Cabinet A"));
	auto *x10 = childByText(cabinet_a, QStringLiteral("X10"));
	QVERIFY(project_side);
	QVERIFY(ka);
	QVERIFY(x10);
	QVERIFY(x10->childCount());
	for (auto *item : {project_side, ka, cabinet_a, x10, x10->child(0)})
	{
		QScopedPointer<QMenu> menu(navigator.buildContextMenu(item));
		QVERIFY(menu);
		QVERIFY(menu->actions().contains(add_strip));
		QCOMPARE(menu->actions().count(), 4);
	}

		//With no item under the cursor at all the current one says
		//which side is meant -- the context menu moves it to the item
		//it is about before it is built.
	tree->setCurrentItem(x10);
	QScopedPointer<QMenu> for_project(navigator.buildContextMenu(nullptr));
	QVERIFY(for_project->actions().contains(add_strip));
	tree->setCurrentItem(free_side->child(0));
	QScopedPointer<QMenu> for_free(navigator.buildContextMenu(nullptr));
	QVERIFY(!for_free->actions().contains(add_strip));
}

/**
	The "New terminal strip" button of the toolbar follows the same
	rules as the context menu: it is greyed out while the independent
	terminals are marked, and on again for the project -- the current
	item alone counts too, since the context menu leaves it behind on
	the item under the cursor without marking anything.
*/
void tst_terminalnavigator::stripButtonFollowsTheMarkedSide()
{
	auto *add_strip = m_widget->findChild<QAction *>(
				QStringLiteral("m_add_strip"));
	QVERIFY(add_strip);
	auto *x10 = stripX10();
	auto *free_side = freeRoot();
	QVERIFY(x10);
	QVERIFY(free_side);
	QVERIFY(free_side->childCount());

		//Nothing marked: the button is for the project as a whole.
	m_tree->clearSelection();
	m_tree->setCurrentItem(nullptr);
	QVERIFY(add_strip->isEnabled());

		//A strip of the project, and a row of it: the button stays.
	selectOnly(x10);
	QVERIFY(add_strip->isEnabled());
	selectOnly(x10->child(0));
	QVERIFY(add_strip->isEnabled());

		//The independent terminals: no strip is made for them.
	selectOnly(free_side->child(0));
	QVERIFY(!add_strip->isEnabled());
	selectOnly(free_side);
	QVERIFY(!add_strip->isEnabled());

		//The current item counts the same way, with nothing marked.
	m_tree->clearSelection();
	m_tree->setCurrentItem(free_side->child(0));
	QVERIFY(!add_strip->isEnabled());
	m_tree->setCurrentItem(x10);
	QVERIFY(add_strip->isEnabled());

	m_tree->clearSelection();
	m_tree->setCurrentItem(nullptr);
}

/**
	Opened from a terminal strip -- the strip itself or one of its
	terminals -- the dialog is about that very strip: its name is in the
	name field, and the preview tree shows what the strip already holds
	ahead of the IDs typed below them. Those rows are greyed out and not
	selectable, which is what keeps them out of the OK button: they exist
	already, and only the IDs typed are created.
	An installation, a location or a free terminal belongs to no strip, and
	leaves the name field empty the way it always did.
*/
void tst_terminalnavigator::preFillsNameFromTheStripAndShowsItsTerminals()
{
	auto *strip_item = stripX10();
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(strip_item);
	QVERIFY(x10);
	const int terminals_in_x10 = x10->physicalTerminalCount();
	QVERIFY(terminals_in_x10 > 0);
	const auto first_terminal = x10->physicalTerminal(0)->realTerminals().at(0);
		//The rows are read the way the dialog builds them: the ID of
		//the terminal -- everything up to and including the last colon
		//of its label goes, the root item of the preview tree saying
		//which strip they belong to already --, and the name of the
		//symbol it is drawn with, empty when it is drawn with none.
	const auto first_label = first_terminal->label();
	const auto first_id = first_label.mid(
				first_label.lastIndexOf(QLatin1Char(':')) + 1);
	QVERIFY2(!first_id.contains(QLatin1Char(':')),
			 "the row of a terminal carries its ID alone");
		//The terminals of the fixture are drawn on the folios of the
		//project, and a drawn terminal keeps no symbol of its own: the
		//element it is drawn with is the one to ask, the very way the
		//dialog asks it -- so there is a name to read out of it.
	const auto *first_element = first_terminal->element();
	QVERIFY2(first_element,
			 "the terminals of the fixture are drawn with a symbol");
	const auto first_symbol_name = first_element->location().name();
	QVERIFY(!first_symbol_name.isEmpty());

	auto *free_side = freeRoot();
	QVERIFY(free_side);
	QVERIFY(free_side->childCount());

		//What one round of the dialog showed: the name field, and the
		//shape of the preview tree with the ID typed -- if @p typed is
		//true at all.
	struct DialogState {
		bool shown = false;
		QString name;
		int rows = -1;
		int selectable = -1;
		int colon_rows = -1;
		QString first_row;
		QString first_symbol;
	};
	const QString next_id = QString::number(terminals_in_x10 + 1);

	const auto readDialog = [&](bool typed, DialogState *state) {
		state->shown = interactWithTheStripDialog([&](QDialog *dialog) {
			auto *names = dialogNameCombo(dialog);
			auto *id_field = dialogIdField(dialog);
			auto *tree = dialogPreviewTree(dialog);
			if (!names || !id_field || !tree) {
				return;
			}
			useTypedIds(dialog);
			state->name = names->currentText();
			if (typed) {
				id_field->setText(next_id);
			}

			auto *root_item = tree->topLevelItem(0);
			if (!root_item) {
				return;
			}
			state->rows = root_item->childCount();
			state->selectable = 0;
			state->colon_rows = 0;
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				const auto *child = root_item->child(i);
				if (child->flags() & Qt::ItemIsSelectable) {
					++state->selectable;
				}
					//No row carries the name of the strip beside the
					//ID: the root item above says it (@sa
					//fillExistingTerminals)
				if (child->text(0).contains(QLatin1Char(':'))) {
					++state->colon_rows;
				}
			}
			if (root_item->childCount()) {
				state->first_row = root_item->child(0)->text(0);
				state->first_symbol = root_item->child(0)->text(1);
			}
		});
	};

		//Called from the strip itself: its name is in the field, and
		//the four terminals it holds are listed ahead of the ID typed.
	m_tree->clearSelection();
	m_tree->setCurrentItem(strip_item);
	DialogState from_strip;
	readDialog(true, &from_strip);
	QVERIFY2(from_strip.shown, "the strip dialog was shown");
	QCOMPARE(from_strip.name, QStringLiteral("X10"));
	QCOMPARE(from_strip.rows, terminals_in_x10 + 1);
	QCOMPARE(from_strip.selectable, 1);
	QCOMPARE(from_strip.first_row, first_id);
	QCOMPARE(from_strip.first_symbol, first_symbol_name);
	QCOMPARE(from_strip.colon_rows, 0);

		//Called from one of its terminals: the very same dialog.
	m_tree->clearSelection();
	m_tree->setCurrentItem(strip_item->child(0));
	DialogState from_terminal;
	readDialog(false, &from_terminal);
	QVERIFY(from_terminal.shown);
	QCOMPARE(from_terminal.name, QStringLiteral("X10"));
	QCOMPARE(from_terminal.rows, terminals_in_x10);
	QCOMPARE(from_terminal.selectable, 0);
	QCOMPARE(from_terminal.first_row, first_id);
	QCOMPARE(from_terminal.first_symbol, first_symbol_name);
	QCOMPARE(from_terminal.colon_rows, 0);

		//Called from a free terminal, which belongs to no strip: no
		//name, and nothing to show beside the ID typed.
	m_tree->clearSelection();
	m_tree->setCurrentItem(free_side->child(0));
	DialogState from_free;
	readDialog(true, &from_free);
	QVERIFY(from_free.shown);
	QVERIFY2(from_free.name.isEmpty(),
			 "a free terminal belongs to no strip to name");
	QCOMPARE(from_free.rows, 1);
	QCOMPARE(from_free.selectable, 1);

		//Called from the strip once more, and accepted: the terminals
		//the dialog showed are not created a second time, only the ID
		//typed goes into the strip -- and one undo takes it out again.
	m_tree->clearSelection();
	m_tree->setCurrentItem(strip_item);
	const int undo_index_before = m_project->undoStack()->index();
	const bool accepted = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *id_field = dialogIdField(dialog);
		if (!id_field) {
			return;
		}
		useTypedIds(dialog);
		id_field->setText(next_id);
		dialog->accept();
	});
	QVERIFY(accepted);
	QCOMPARE(x10->physicalTerminalCount(), terminals_in_x10 + 1);
	QCOMPARE(m_project->undoStack()->index(), undo_index_before + 1);

	m_project->undoStack()->undo();
	QCOMPARE(x10->physicalTerminalCount(), terminals_in_x10);
}

/**
	The dialog names the terminals to add in one of two ways, and the radio
	buttons say which input is the one to type into: the number of terminals
	enables the spin box, the IDs typed enable the line edit. The dialog
	opens on the number of terminals, its input being the one which is
	active (@sa AddTerminalStripDialog::onTerminalsModeChanged).
*/
void tst_terminalnavigator::radioButtonsEnableTheirField()
{
	bool number_checked = false, id_field_on = true, spin_on = false;
	bool after_id_field = false, after_id_spin = true;
	bool back_number = true, back_field = true, back_spin = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *ids_rb = dialogIdRadio(dialog);
		auto *numbers_rb = dialogNumberRadio(dialog);
		auto *id_field = dialogIdField(dialog);
		auto *spin = dialogNumberSpin(dialog);
		if (!ids_rb || !numbers_rb || !id_field || !spin) {
			return;
		}
		number_checked = numbers_rb->isChecked();
		id_field_on = id_field->isEnabled();
		spin_on = spin->isEnabled();

		ids_rb->setChecked(true);
		after_id_field = id_field->isEnabled();
		after_id_spin = spin->isEnabled();

		numbers_rb->setChecked(true);
		back_number = numbers_rb->isChecked();
		back_field = id_field->isEnabled();
		back_spin = spin->isEnabled();
	});

	QVERIFY(dialog_found);
	QVERIFY2(number_checked, "the dialog opens on the number of terminals");
	QVERIFY2(!id_field_on, "the ID field is not typed into in that mode");
	QVERIFY(spin_on);
	QVERIFY(after_id_field);
	QVERIFY2(!after_id_spin,
			 "each radio button disables the input of the other one");
	QVERIFY(back_number);
	QVERIFY(!back_field);
	QVERIFY(back_spin);
}

/**
	The number of terminals takes whole numbers from 0 to 99 and starts at
	zero: the values the .ui file sets on the spin box and the code sets
	again. A spin box clamps everything typed onto that range, and the cap
	is what the notice label tells about, reached or attempted.
*/
void tst_terminalnavigator::spinBoxTakesWholeNumbersFromZeroToNinetyNine()
{
	int minimum = -1, maximum = -1, value_first = -1;
	int value_high = -1, value_low = -1;
	QString notice_at_cap, notice_plain;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *spin = dialogNumberSpin(dialog);
		auto *notice = dialogNotice(dialog);
		if (!spin || !notice) {
			return;
		}
		minimum = spin->minimum();
		maximum = spin->maximum();
		value_first = spin->value();
		notice_plain = notice->text();

		spin->setValue(150);
		value_high = spin->value();
		notice_at_cap = notice->text();

		spin->setValue(-5);
		value_low = spin->value();
	});

	QVERIFY(dialog_found);
	QCOMPARE(minimum, 0);
	QCOMPARE(maximum, 99);
	QCOMPARE(value_first, 0);
	QCOMPARE(value_high, 99);
	QCOMPARE(value_low, 0);
	QVERIFY2(notice_plain.isEmpty(), qPrintable(notice_plain));
	QCOMPARE(notice_at_cap,
			 QStringLiteral("The number of terminals to add at once "
							 "is limited to 99"));
}

/**
	With the number of terminals being the way the terminals to add are
	named, the preview tree shows as many rows as the spin box says -- no
	more, no less -- and the OK button creates exactly them: the IDs are
	the ones a new strip starts with.
*/
void tst_terminalnavigator::addsAsManyTerminalsAsTheNumberSays()
{
	QStringList rows;
	int selectable = 0;
	const int strips_before = m_project->terminalStrip().count();
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *spin = dialogNumberSpin(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!names || !spin || !tree) {
			return;
		}
			//The dialog may well be called from a strip of the tree,
			//whose name it then prefills: this test names a strip of
			//its own.
		names->lineEdit()->clear();
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X78"));
		spin->setValue(3);

		auto *root_item = tree->topLevelItem(0);
		if (!root_item) {
			return;
		}
		for (int i = 0 ; i < root_item->childCount() ; ++i) {
			rows << root_item->child(i)->text(0);
			if (root_item->child(i)->flags() & Qt::ItemIsSelectable) {
				++selectable;
			}
		}
		dialog->accept();
	});

	QVERIFY(dialog_found);
	QCOMPARE(rows, QStringList({QStringLiteral("1"),
							   QStringLiteral("2"),
							   QStringLiteral("3")}));
	QCOMPARE(selectable, 3);

	auto *strip = stripNamed(QStringLiteral("X78"));
	QVERIFY(strip);
	QCOMPARE(strip->physicalTerminalCount(), 3);
	QCOMPARE(m_project->terminalStrip().count(), strips_before + 1);

		//Leave the project the way this test found it.
	m_project->undoStack()->undo();
	QVERIFY(!stripNamed(QStringLiteral("X78")));
}

/**
	What the strip already holds is what the new terminals continue: their
	rows are appended behind the ones the tree shows already, and their IDs
	go on behind the rows and behind the highest ID among them alike -- a
	terminal block holding the IDs 2, 3, 5 and 7 goes on with 8, never
	with 5 (@sa AddTerminalStripDialog::generatedIds).
*/
void tst_terminalnavigator::appendsTheNumberedTerminalsAfterWhatTheStripHolds()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	auto *strip_item = stripX10();
	QVERIFY(x10);
	QVERIFY(strip_item);
	const int terminals_in_x10 = x10->physicalTerminalCount();
	QVERIFY(terminals_in_x10 > 0);

		//Where the IDs continue, read the way the dialog reads it: behind
		//the rows of the strip and behind the highest ID among them.
	int highest_id = 0;
	for (int i = 0 ; i < x10->physicalTerminalCount() ; ++i)
	{
		const auto physical_terminal = x10->physicalTerminal(i);
		if (!physical_terminal) {
			continue;
		}
		for (const auto &real_terminal : physical_terminal->realTerminals()) {
			const auto label = real_terminal->label();
			bool is_number = false;
			const int id = label.mid(label.lastIndexOf(QLatin1Char(':')) + 1)
					.toInt(&is_number);
			if (is_number) {
				highest_id = qMax(highest_id, id);
			}
		}
	}
	const int first_id = qMax(terminals_in_x10, highest_id) + 1;

	m_tree->clearSelection();
	m_tree->setCurrentItem(strip_item);

	QStringList new_rows;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *spin = dialogNumberSpin(dialog);
		auto *tree = dialogPreviewTree(dialog);
		if (!spin || !tree) {
			return;
		}
		spin->setValue(3);

		auto *root_item = tree->topLevelItem(0);
		if (!root_item) {
			return;
		}
			//The rows of what the strip holds already are display only
			//and not selectable: what the spin box adds are the others.
		for (int i = 0 ; i < root_item->childCount() ; ++i) {
			if (root_item->child(i)->flags() & Qt::ItemIsSelectable) {
				new_rows << root_item->child(i)->text(0);
			}
		}
		dialog->accept();
	});

	QVERIFY(dialog_found);
	QCOMPARE(new_rows, QStringList({QString::number(first_id),
								   QString::number(first_id + 1),
								   QString::number(first_id + 2)}));
	QCOMPARE(x10->physicalTerminalCount(), terminals_in_x10 + 3);

		//Leave the project the way this test found it.
	m_project->undoStack()->undo();
	QCOMPARE(x10->physicalTerminalCount(), terminals_in_x10);
}

/**
	An ID which is taken already is stepped over by the generator, never
	raised as an error: a free terminal labelled 'X79:2' claims the ID 2
	of the strip X79, so the terminals to add go on with 1, 3 and 4 -- and
	the notice label says what was skipped without touching the OK button
	(@sa AddTerminalStripDialog::generatedIds, takenIds).
*/
void tst_terminalnavigator::skipsTheIdsAFreeTerminalHolds()
{
		//A free terminal labelled 'name:id' is what claims an ID, and it
		//is named the way the properties dialog names it: through the undo
		//command. The formula of a terminal names it, not its label, so it
		//goes away with the renaming -- a terminal of the test project
		//follows a numbering scheme or it does not, and either way the
		//label is what counts after this.
	auto free_terminals = ElementProvider(m_project).freeTerminal();
	QVERIFY2(!free_terminals.isEmpty(),
			 "the test project holds free terminals to rename");
	auto *free_element = free_terminals.first();

	auto old_info = free_element->elementInformations();
	auto new_info = old_info;
	new_info.addValue(QStringLiteral("label"), QStringLiteral("X79:2"));
	new_info.addValue(QStringLiteral("formula"), QString());
	m_project->undoStack()->push(
				new ChangeElementInformationCommand(free_element, old_info, new_info));
	QCoreApplication::processEvents();

	QStringList rows;
	QString notice;
	bool ok_enabled = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *names = dialogNameCombo(dialog);
		auto *spin = dialogNumberSpin(dialog);
		auto *tree = dialogPreviewTree(dialog);
		auto *notice_lb = dialogNotice(dialog);
		auto *buttons = dialogButtons(dialog);
		if (!names || !spin || !tree || !notice_lb || !buttons) {
			return;
		}
		names->lineEdit()->clear();
		QTest::keyClicks(names->lineEdit(), QStringLiteral("X79"));
		spin->setValue(3);
		notice = notice_lb->text();
		auto *ok = buttons->button(QDialogButtonBox::Ok);
		ok_enabled = ok && ok->isEnabled();

		auto *root_item = tree->topLevelItem(0);
		if (!root_item) {
			return;
		}
		for (int i = 0 ; i < root_item->childCount() ; ++i) {
			rows << root_item->child(i)->text(0);
		}
	});

	QVERIFY(dialog_found);
	QCOMPARE(rows, QStringList({QStringLiteral("1"),
							   QStringLiteral("3"),
							   QStringLiteral("4")}));
	QCOMPARE(notice,
			 QStringLiteral("IDs %1 are already used and were skipped")
				 .arg(QStringLiteral("2")));
	QVERIFY2(ok_enabled,
			 "a skipped ID is worth a hint, never worth stopping for");

		//Leave the project the way this test found it.
	m_project->undoStack()->undo();
	QCoreApplication::processEvents();
}

/**
	The terminals the dialog is about to create are marked the moment their
	rows appear, so that the button of the toolbar above the tree has
	something to work on -- and it has nothing to work on as soon as the
	user marks nothing anymore (@sa AddTerminalStripDialog::
	refreshPreviewTree, onPreviewSelectionChanged).
*/
void tst_terminalnavigator::marksTheTerminalsItAdds()
{
	bool button_at_start = true;
	int selected_rows = 0;
	bool button_on_rows = false;
	bool button_without_rows = true;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *spin = dialogNumberSpin(dialog);
		auto *tree = dialogPreviewTree(dialog);
		auto *action = dialogEditTerminalsAction(dialog);
		if (!spin || !tree || !action) {
			return;
		}
		button_at_start = action->isEnabled();
		spin->setValue(3);

		auto *root_item = tree->topLevelItem(0);
		if (root_item) {
			for (int i = 0 ; i < root_item->childCount() ; ++i) {
				if (root_item->child(i)->isSelected()) {
					++selected_rows;
				}
			}
		}
		button_on_rows = action->isEnabled();

		tree->clearSelection();
		button_without_rows = !action->isEnabled();
	});

	QVERIFY(dialog_found);
	QVERIFY2(!button_at_start,
			 "the button has nothing to work on while no terminal is marked");
	QCOMPARE(selected_rows, 3);
	QVERIFY2(button_on_rows, "the button works on the terminals marked");
	QVERIFY2(button_without_rows,
			 "the button stops working the moment nothing is marked");
}

/**
	The button of the toolbar opens the dialog of the properties for the
	terminals marked, and the fields the user finds there are the very ones
	of the element editor, plus the symbol a terminal is drawn with and the
	flag which keeps it out of the bill of materials
	(@sa TerminalPropertiesDialog). The button keeps quiet while nothing is
	marked: a dialog which has no terminal to work on is a dialog which
	would give its values to nothing
	(@sa AddTerminalStripDialog::openTerminalsDialog).
*/
void tst_terminalnavigator::opensItsDialogForTheMarkedTerminals()
{
	bool dialog_for_rows = false, dialog_without_rows = false;
	bool lists_the_fields = false;
	const bool dialog_found = interactWithTheStripDialog([&](QDialog *dialog) {
		auto *spin = dialogNumberSpin(dialog);
		auto *tree = dialogPreviewTree(dialog);
		auto *action = dialogEditTerminalsAction(dialog);
		if (!spin || !tree || !action) {
			return;
		}
		spin->setValue(2);

		const auto ask = [&](bool *shown) {
			*shown = interactWithThePropertiesDialog(dialog, [&](QDialog *properties) {
				lists_the_fields = properties->findChild<QGroupBox *>(
							QStringLiteral("m_terminal_gb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_terminal_type_cb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_terminal_func_cb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_terminal_section_cb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_terminal_width_cb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_terminal_color_cb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_terminal_manufacturer_cb"))
						&& properties->findChild<QComboBox *>(
							QStringLiteral("m_symbol_cb"))
						&& properties->findChild<QCheckBox *>(
							QStringLiteral("m_auto_num_locked_cb"))
						&& properties->findChild<QCheckBox *>(
							QStringLiteral("m_potential_isolating_cb"))
						&& properties->findChild<QCheckBox *>(
							QStringLiteral("m_exclude_from_bom_cb"));
			});
		};

		ask(&dialog_for_rows);

		tree->clearSelection();
		ask(&dialog_without_rows);
	});

	QVERIFY(dialog_found);
	QVERIFY2(dialog_for_rows,
			 "the button opens the dialog for the terminals marked");
	QVERIFY2(lists_the_fields,
			 "the dialog holds the very fields of the element editor, "
			 "plus the symbol and the flag out of the bill of materials");
	QVERIFY2(!dialog_without_rows,
			 "no terminal marked, no dialog to open");
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

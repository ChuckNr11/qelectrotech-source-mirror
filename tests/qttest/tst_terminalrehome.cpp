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

#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "elementprovider.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "undocommand/changeelementdatacommand.h"
#include "undocommand/changeelementinformationcommand.h"

#include <QSharedPointer>
#include <QString>
#include <QUndoStack>

/**
	The home of a terminal follows its name: renaming the symbol of a
	terminal on a folio ("X10:1" to "X2:7") takes its row to the strip
	the new name calls for, and a name no strip answers to ("X99:1")
	takes the row out of its strip -- the terminal waits under
	"Independent terminals", its symbol staying where it is drawn. One
	undo step gives the name and the home back together
	(@sa TerminalStripRehome).

	What the user moves alone is never moved here: a bridged row or a row
	of several levels stays where it is, and so does a terminal named
	"PE" -- a name of its own, calling for no strip at all.
*/
class tst_terminalrehome : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;

	TerminalStrip *stripNamed(const QString &name) const
	{
		const auto strips = m_project->terminalStrip();
		for (auto *strip : strips) {
			if (strip->name() == name) {
				return strip;
			}
		}
		return nullptr;
	}

		//A terminal named by its label and not by a formula: what a
		//formula names it is worked out again at every change and
		//would swallow the name typed here (@sa Element::actualLabel).
	QSharedPointer<RealTerminal> renameableTerminalOf(TerminalStrip *strip) const
	{
		if (!strip) {
			return {};
		}
		for (int i = 0 ; i < strip->physicalTerminalCount() ; ++i)
		{
			for (const auto &real_t : strip->physicalTerminal(i)->realTerminals())
			{
				auto *element_ = real_t ? real_t->element() : nullptr;
				if (element_ && element_->elementInformations()
						.value(QETInformation::ELMT_FORMULA).toString().isEmpty()) {
					return real_t;
				}
			}
		}
		return {};
	}

		//The rename the properties dialog does, through the very
		//command it pushes -- which is what carries the row with it.
		//The formula is emptied along the way: a name typed by hand
		//follows no numbering scheme, and what a formula names a
		//terminal is worked out again at every change, which would
		//swallow the name typed here (@sa Element::actualLabel). The
		//plant and the location go along, the way a terminal of a
		//strip carries them.
	void rename(Element *element, const QString &label,
				const QString &plant, const QString &location)
	{
		const auto old_info = element->elementInformations();
		auto new_info = old_info;
		new_info.addValue(QETInformation::ELMT_FORMULA, QString());
		new_info.addValue(QETInformation::ELMT_LABEL, label);
		new_info.addValue(QETInformation::ELMT_PLANT, plant);
		new_info.addValue(QETInformation::ELMT_LOCATION, location);
		m_project->undoStack()->push(
					new ChangeElementInformationCommand(element, old_info, new_info));
	}

		//Free terminals of the project, all of them still waiting under
		//its independent terminals -- none of them in a strip yet. Up
		//to @p count different ones.
	QVector<QSharedPointer<RealTerminal>> freeTerminals(int count) const
	{
		QVector<QSharedPointer<RealTerminal>> reals;
		for (auto *terminal_ : ElementProvider(m_project).freeTerminal())
		{
			if (!terminal_) {
				continue;
			}
			const auto real_t = terminal_->realTerminal();
			if (real_t.isNull()) {
				continue;
			}
			reals << real_t;
			if (reals.count() == count) {
				break;
			}
		}
		return reals;
	}

private slots:
	void initTestCase();
	void movesTheRowToTheStripItsNameCallsFor();
	void becomesIndependentWhenNoStripAnswersItsName();
	void keepsTheRowWhenTheNameCallsForItsOwnStrip();
	void keepsTheRowWhenTheNameHasNoStripOfItsOwn();
	void keepsRowsTheUserMovesAlone();
	void followsTheDataTheStripEditorChanges();
	void cleanupTestCase();
};

void tst_terminalrehome::initTestCase()
{
	m_project = new QETProject(QStringLiteral(QET_TEST_PROJECT_PATH), this);

		//X10 holds the four terminals this test renames, X2 is the
		//strip two of those names call for, X201 is another plant and
		//location -- a name calling for it must pick it for that.
	auto *x10 = m_project->newTerminalStrip(
				QStringLiteral("KA"), QStringLiteral("Cabinet A"), QStringLiteral("X10"));
	auto *x2 = m_project->newTerminalStrip(
				QStringLiteral("KA"), QStringLiteral("Cabinet A"), QStringLiteral("X2"));
	auto *x201 = m_project->newTerminalStrip(
				QStringLiteral("KB"), QStringLiteral("Roof"), QStringLiteral("X201"));

		//No example carries terminal strips, so the terminals below the
		//strips are taken from the ones the project has free -- drawn on
		//a folio already, which is what keeps their symbols in place
		//while their rows move.
	auto free_terminals = ElementProvider(m_project).freeTerminal();
	QVERIFY(free_terminals.count() >= 7);
	for (int i = 0 ; i < 4 ; ++i) {
		QVERIFY(x10->addTerminal(free_terminals.at(i)));
	}
	for (int i = 4 ; i < 6 ; ++i) {
		QVERIFY(x2->addTerminal(free_terminals.at(i)));
	}
	QVERIFY(x201->addTerminal(free_terminals.at(6)));
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(x2->physicalTerminalCount(), 2);
	QCOMPARE(x201->physicalTerminalCount(), 1);
}

void tst_terminalrehome::movesTheRowToTheStripItsNameCallsFor()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	auto *x2 = stripNamed(QStringLiteral("X2"));
	QVERIFY(x10);
	QVERIFY(x2);
	const auto real_t = renameableTerminalOf(x10);
	QVERIFY2(real_t, "a terminal of X10 named by its label");
	auto *element = real_t->element();
	QVERIFY(element);
	const QString before = real_t->label();

	rename(element, QStringLiteral("X2:7"), QStringLiteral("KA"), QStringLiteral("Cabinet A"));

		//The row left X10 for X2 -- the strip the name calls for --,
		//and the symbol stays drawn where it was.
	QCOMPARE(real_t->parentStrip(), x2);
	QCOMPARE(x10->physicalTerminalCount(), 3);
	QCOMPARE(x2->physicalTerminalCount(), 3);
	QVERIFY(element->diagram());
	QCOMPARE(real_t->label(), QStringLiteral("X2:7"));

		//One undo gives the name and the home back together: what a
		//rename moves, an undo of that rename gives back.
	m_project->undoStack()->undo();
	QCOMPARE(real_t->parentStrip(), x10);
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(x2->physicalTerminalCount(), 2);
	QCOMPARE(real_t->label(), before);
}

void tst_terminalrehome::becomesIndependentWhenNoStripAnswersItsName()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto real_t = renameableTerminalOf(x10);
	QVERIFY(real_t);
	auto *element = real_t->element();
	QVERIFY(element);
	const QString before = real_t->label();

		//X99 is a strip the project holds none of: the terminal waits
		//under the independent terminals, its symbol untouched.
	rename(element, QStringLiteral("X99:1"), QStringLiteral("KA"), QStringLiteral("Cabinet A"));

	QVERIFY(real_t->parentStrip() == nullptr);
	QCOMPARE(x10->physicalTerminalCount(), 3);
	QVERIFY(element->diagram());
	QCOMPARE(real_t->label(), QStringLiteral("X99:1"));

		//Undo brings the row back to the strip it was taken out of.
	m_project->undoStack()->undo();
	QCOMPARE(real_t->parentStrip(), x10);
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(real_t->label(), before);
}

void tst_terminalrehome::keepsTheRowWhenTheNameCallsForItsOwnStrip()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto real_t = renameableTerminalOf(x10);
	QVERIFY(real_t);
	auto *element = real_t->element();
	QVERIFY(element);

		//Another position of the same strip: the row is home already
		//and stays where the user put it.
	rename(element, QStringLiteral("X10:42"), QStringLiteral("KA"), QStringLiteral("Cabinet A"));

	QCOMPARE(real_t->parentStrip(), x10);
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(real_t->label(), QStringLiteral("X10:42"));

	m_project->undoStack()->undo();
}

void tst_terminalrehome::keepsTheRowWhenTheNameHasNoStripOfItsOwn()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto real_t = renameableTerminalOf(x10);
	QVERIFY(real_t);
	auto *element = real_t->element();
	QVERIFY(element);

		//"PE" names a terminal and no strip: the auto import reads
		//such a name as a strip name, but renaming one terminal is not
		//asking for a strip of that name to exist.
	rename(element, QStringLiteral("PE"), QStringLiteral("KA"), QStringLiteral("Cabinet A"));

	QCOMPARE(real_t->parentStrip(), x10);
	QCOMPARE(x10->physicalTerminalCount(), 4);
	QCOMPARE(real_t->label(), QStringLiteral("PE"));

	m_project->undoStack()->undo();
}

void tst_terminalrehome::keepsRowsTheUserMovesAlone()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);

		//A row of two levels: moving it would drag both levels along
		//or break the row apart, so it stays the way deletion refuses
		//it (@sa DeleteQGraphicsItemCommand::hasNonDeletableTerminal).
	const auto grouped = freeTerminals(2);
	QVERIFY2(grouped.count() == 2, "two free terminals to group");
	x10->addAndGroupTerminals({grouped});
	QCOMPARE(x10->physicalTerminalCount(), 5);
	const auto row = grouped.first()->physicalTerminal();
	QVERIFY(row);
	QCOMPARE(row->levelCount(), 2);

	rename(grouped.first()->element(), QStringLiteral("X2:8"),
		   QStringLiteral("KA"), QStringLiteral("Cabinet A"));

	QCOMPARE(grouped.first()->parentStrip(), x10);
	QCOMPARE(x10->physicalTerminalCount(), 5);
	m_project->undoStack()->undo();

		//A bridged row is the same thing: its bridge would have to
		//follow or go.
	const auto bridged = freeTerminals(2);
	QVERIFY2(bridged.count() == 2, "two free terminals to bridge");
	QVERIFY(x10->addTerminal(bridged.at(0)->element()));
	QVERIFY(x10->addTerminal(bridged.at(1)->element()));
	QVERIFY(x10->setBridge(bridged));
	QVERIFY(bridged.at(0)->isBridged());

	rename(bridged.at(0)->element(), QStringLiteral("X2:9"),
		   QStringLiteral("KA"), QStringLiteral("Cabinet A"));

	QCOMPARE(bridged.at(0)->parentStrip(), x10);
	m_project->undoStack()->undo();
}

void tst_terminalrehome::followsTheDataTheStripEditorChanges()
{
	auto *x10 = stripNamed(QStringLiteral("X10"));
	QVERIFY(x10);
	const auto real_t = renameableTerminalOf(x10);
	QVERIFY(real_t);
	auto *element = real_t->element();
	QVERIFY(element);
	const QString before = real_t->label();

		//The terminal strip editor changes the data of a terminal, and
		//the data carry its label too: what the name does to the row
		//goes with that undo step as well.
	auto new_data = element->elementData();
	new_data.m_informations.addValue(QETInformation::ELMT_LABEL,
									QStringLiteral("X2:11"));
	m_project->undoStack()->push(new ChangeElementDataCommand(element, new_data));

	QCOMPARE(real_t->parentStrip(), stripNamed(QStringLiteral("X2")));
	QCOMPARE(real_t->label(), QStringLiteral("X2:11"));

	m_project->undoStack()->undo();
	QCOMPARE(real_t->parentStrip(), x10);
	QCOMPARE(real_t->label(), before);
}

void tst_terminalrehome::cleanupTestCase()
{
	delete m_project;
	m_project = nullptr;
}

QET_APP_TEST_MAIN(tst_terminalrehome)
#include "tst_terminalrehome.moc"

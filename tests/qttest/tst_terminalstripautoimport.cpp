/*
	Copyright 2006-2026  The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "qetapptest.h"

#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "TerminalStrip/terminalstripautoimport.h"
#include "qetproject.h"

#include <QSqlQuery>
#include <QUndoStack>

/**
	@brief The tst_terminalstripautoimport class
	What a terminal which is not drawn in any folio yet -- a pending one
	of the terminal navigator, a spare one of the terminal strip editor --
	is worth to the rest of the project: the automatic import, which works
	out the strips from the labels of the free terminals, and the
	nomenclature, which is filled from the elements drawn on the folios
	and from the spare terminals.
*/
class tst_terminalstripautoimport : public QObject
{
	Q_OBJECT

private slots:
	void leavesTheTerminalsWhichAreNotDrawnAlone();
	void keepsTheTerminalsWhichAreNotDrawnOutOfTheNomenclature();
};

/**
	The import groups the free terminals of a project by the name in their
	label ("X1:2" goes to the strip "X1"), creates the strips missing and
	sort those strips it created itself.

	A terminal which is not drawn in any folio yet holds no element. The
	terminal navigator creates such terminals when it describes a strip
	whose symbols are not placed on a folio yet (RealTerminal::
	createPending()), and the terminal strip editor creates spare ones, so
	a strip of the project can well hold terminals which are drawn nowhere.
	The import leaves those terminals alone: it does not move them, does
	not duplicate them, does not destroy them, and it does not build a
	second strip for the name they bear -- a name only they carry is not
	a name any terminal of the project asks for.

	That is also the boundary of a pending terminal, which is part of the
	model only: it is visible in the terminal tab and in its strip, and
	invisible to the nomenclature, which is filled from the elements drawn
	on the folios and from the spare terminals (projectDataBase::
	populateSpareTerminalTable filters on RealTerminal::isSpare()). A
	pending terminal joins those two worlds when its symbol is drawn on a
	folio, and becomes a terminal of an element.
*/
void tst_terminalstripautoimport::leavesTheTerminalsWhichAreNotDrawnAlone()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));

		//A strip the terminal navigator could have made: two terminals,
		//neither of them drawn anywhere, the first one with the symbol
		//it is to be drawn with and the second one without any yet.
	auto *strip = project.newTerminalStrip(
		QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	const auto pending_with_symbol = RealTerminal::createPending(
		QStringLiteral("X999:1"), QStringLiteral("embed://terminal.elmt"));
	const auto pending_without_symbol = RealTerminal::createPending(
		QStringLiteral("X999:2"), QString());
	const auto identity = pending_with_symbol->uuid();
	QVERIFY(pending_with_symbol != pending_without_symbol);
	QVERIFY(strip->addTerminal(pending_with_symbol));
	QVERIFY(strip->addTerminal(pending_without_symbol));
	QCOMPARE(strip->realTerminals().count(), 2);

	const int strips_before = project.terminalStrip().count();
	const int undo_before = project.undoStack()->index();

	const auto result = TerminalStripAutoImport::importFreeTerminals(
				&project, false);

		//The run did its real work: the free terminals industrial.qet
		//holds were put in strips. None of it reached the terminals which
		//are drawn nowhere, the element being what the import reads and
		//what it groups by.
	QVERIFY2(result.terminals > 0,
			 "the free terminals of the project were imported");
	QCOMPARE(strip->realTerminals().count(), 2);
	QCOMPARE(strip->realTerminals().at(0), pending_with_symbol);
	QCOMPARE(strip->realTerminals().at(1), pending_without_symbol);
	QVERIFY2(!pending_with_symbol->isElement(),
			 "the terminal is still drawn in no folio");
	QVERIFY(!pending_without_symbol->isElement());
	QCOMPARE(pending_with_symbol->label(), QStringLiteral("X999:1"));
	QCOMPARE(pending_with_symbol->symbol(),
			 QStringLiteral("embed://terminal.elmt"));
	QCOMPARE(pending_with_symbol->uuid(), identity);
	QCOMPARE(pending_without_symbol->label(), QStringLiteral("X999:2"));
	QVERIFY(pending_without_symbol->symbol().isEmpty());

		//No second strip for the name they bear: the import reuses a
		//strip which exists, and creates none for a name only terminals
		//drawn nowhere carry.
	int strips_named_x999 = 0;
	for (auto strip_ : project.terminalStrip()) {
		QVERIFY(strip_);
		if (strip_->name() == QLatin1String("X999")) {
			++strips_named_x999;
			QCOMPARE(strip_, strip);
		}
	}
	QCOMPARE(strips_named_x999, 1);

		//One undo takes the whole run back out -- which is what the one
		//undoable step of the import promises -- and the strip of the
		//navigator still holds its two terminals.
	project.undoStack()->undo();
	QCOMPARE(project.undoStack()->index(), undo_before);
	QCOMPARE(project.terminalStrip().count(), strips_before);
	QCOMPARE(strip->realTerminals().count(), 2);
	QCOMPARE(strip->realTerminals().at(0), pending_with_symbol);
}

/**
	The nomenclature of a project lists the terminals of its strips, and a
	terminal reaches it in one of two ways: it is drawn on a folio, which
	makes it a terminal of an element, or it is a spare terminal the
	terminal strip editor reserved. A pending terminal of the terminal
	navigator is neither -- it holds no element, and RealTerminal::
	isSpare() is false for it -- so it stays out of the nomenclature until
	its symbol is drawn on a folio.

	projectDataBase::populateSpareTerminalTable() fills that table from
	every strip of the project, walking every real terminal and keeping
	the spare ones, which is what makes the difference executable here:
	the same strip holds one pending terminal and one spare one, the
	spare table is refilled, and only the spare terminal shows up.
*/
void tst_terminalstripautoimport::keepsTheTerminalsWhichAreNotDrawnOutOfTheNomenclature()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	auto *strip = project.newTerminalStrip(
		QStringLiteral("KC"), QStringLiteral("Cellar"), QStringLiteral("X999"));
	QVERIFY(strip);
	QVERIFY(strip->addTerminal(RealTerminal::createPending(
		QStringLiteral("X999:1"), QStringLiteral("embed://terminal.elmt"))));

	auto *database = project.dataBase();
	QVERIFY(database);
	auto spare_row_count = [database]() {
		QSqlQuery query(database->database());
		if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM spare_terminal"))
		    || !query.next()) {
			return -1;
		}
		return query.value(0).toInt();
	};

	const int rows_before = spare_row_count();
	QVERIFY2(rows_before >= 0, "the spare table of the database can be read");

		//The pending terminal changes nothing to the nomenclature, even
		//after the very refill which lists the spare terminals of every
		//strip: it is a terminal of the model only.
	database->spareTerminalsChanged();
	QCOMPARE(spare_row_count(), rows_before);

		//A spare terminal of the same strip is in the nomenclature, and
		//that is the contrast the pending terminal is measured against.
	QCOMPARE(strip->insertSpareTerminals(
				 -1, {RealTerminal::createSpare(ElementData())}).size(), 1);
	database->spareTerminalsChanged();
	QCOMPARE(spare_row_count(), rows_before + 1);
}

QET_APP_TEST_MAIN(tst_terminalstripautoimport)
#include "tst_terminalstripautoimport.moc"

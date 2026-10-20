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
#include "TerminalStrip/ui/terminalstripeditor.h"
#include "qetproject.h"

#include <QBrush>
#include <QUndoStack>

/**
	@brief The tst_terminalstripeditor class
	The table of the terminal strip editor, where the terminals of a strip
	are edited -- including the ones which are drawn in no folio yet.
*/
class tst_terminalstripeditor : public QObject
{
	Q_OBJECT

private slots:
	void keepsTheEditsOfATerminalWhichIsNotDrawnYet();
};

/**
	A terminal the terminal navigator created is already in its strip but
	is drawn nowhere: it holds no element. The table of the editor took
	what was typed into its row anyway, and apply() threw it away without
	a word -- no command pushed, no message -- reload() then showed the
	old values again, so the edit had vanished for the user.

	A terminal drawn nowhere has data of its own now, the way a spare
	terminal has (@sa RealTerminal::isPending()), and apply() stores them
	there: what the user typed into the row is on the terminal afterwards,
	in one undoable step which one undo takes back.

	The row also says what it is: without an element there is no
	cross-reference to show, so the XRef column reads "Not drawn yet" in
	gray, the way it reads "Spare" for a reserved terminal.
*/
void tst_terminalstripeditor::keepsTheEditsOfATerminalWhichIsNotDrawnYet()
{
	QETProject project(QStringLiteral(QET_TEST_PROJECT_PATH));
	auto *strip = project.newTerminalStrip(
				QStringLiteral("KC"), QStringLiteral("Cellar"),
				QStringLiteral("X999"));
	QVERIFY(strip);
	QVERIFY(strip->addTerminal(RealTerminal::createPending(
				QStringLiteral("X999:1"),
				QStringLiteral("embed://terminal.elmt"))));

	TerminalStripEditor editor(&project);
	editor.setCurrentStrip(strip);
	auto *model = editor.findChild<TerminalStripModel *>();
	QVERIFY2(model, "the editor built the table model of the strip");
	QCOMPARE(model->rowCount(QModelIndex()), 1);

		//The row is drawn nowhere and says so, in the cell which would
		//hold the reference to a folio
	const auto xref_index = model->index(0, TerminalStripModel::XRef);
	QCOMPARE(model->data(xref_index).toString(),
			 QCoreApplication::translate("TerminalStripModel",
										 "Not drawn yet"));
	QCOMPARE(qvariant_cast<QBrush>(model->data(xref_index, Qt::ForegroundRole)).color(),
			 QColor(Qt::gray));

		//The user types into the row, the way the table takes it
	const auto label_index = model->index(0, TerminalStripModel::Label);
	QCOMPARE(model->data(label_index).toString(), QStringLiteral("X999:1"));
	QVERIFY(model->setData(label_index, QStringLiteral("X999:7"),
						   Qt::EditRole));
	QVERIFY(model->setData(model->index(0, TerminalStripModel::Type),
						   QVariant::fromValue(ElementData::TTFuse),
						   Qt::EditRole));

	editor.apply();

	const auto real_t = strip->realTerminals().at(0);
	QVERIFY2(real_t->isPending(), "the terminal is still drawn in no folio");
	QVERIFY(!real_t->isSpare());
	QVERIFY2(real_t->label() == QStringLiteral("X999:7"),
			 "the label typed into the row is on the terminal");
	QCOMPARE(real_t->type(), ElementData::TTFuse);
	QCOMPARE(real_t->terminalData().terminalType(), ElementData::TTFuse);
	QVERIFY(project.undoStack()->canUndo());

		//One undo takes the edits back: apply() put them in one
		//undoable step, which is what a step the user can not see the
		//end of would not be
	project.undoStack()->undo();
	QCOMPARE(real_t->label(), QStringLiteral("X999:1"));
	QCOMPARE(real_t->type(), ElementData::TTGeneric);
}

QET_APP_TEST_MAIN(tst_terminalstripeditor)
#include "tst_terminalstripeditor.moc"

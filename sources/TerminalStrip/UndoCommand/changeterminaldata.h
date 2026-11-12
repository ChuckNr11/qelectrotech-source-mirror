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
#ifndef CHANGETERMINALDATA_H
#define CHANGETERMINALDATA_H

#include "../realterminal.h"

#include <QVector>
#include <QUndoCommand>

/**
	@brief The ChangeTerminalData class
	One undo step which gives one or more terminals the properties the
	user chose for them: what the dialog of the properties shows for the
	terminals marked in the terminal navigator
	(@sa TerminalNavigatorWidget::editSelection), the same values for all
	of them, since that is what one click on OK means there.

	The data are written where a terminal keeps its own: the element it
	is drawn with -- which is the one funnel every change of a symbol
	goes through, label and notification alike
	(@sa Element::setElementData()) -- or the copy it carries itself
	while it is drawn in no folio yet (@sa RealTerminal::setSpareData()).
	What those properties are worth is worked out by the caller: this
	command knows a terminal and the data it should carry from here on,
	never how they were gathered. What the terminal carries before it is
	pushed is what undo() puts back, terminal by terminal.

	One command and not one per terminal: one click on OK is one step
	back out of the undo stack. The label is left alone along the way --
	what the properties are worth is not what a terminal is called
	(@sa TerminalProperties::applyTo).
*/
class ChangeTerminalData : public QUndoCommand
{
	public:
		/// One terminal, and the data it should carry from here on.
		struct Change {
			QSharedPointer<RealTerminal> terminal;
			ElementData after;
		};

		ChangeTerminalData(const QVector<Change> &changes,
		                   QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		/// Writes @p data into the terminal of @p index, where it keeps
		/// its own -- and does nothing when that terminal left the
		/// project in the meantime.
		void apply(int index, const ElementData &data) const;

		QVector<Change> m_changes;
		QVector<ElementData> m_before;
};

#endif // CHANGETERMINALDATA_H

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
#ifndef REMOVEPHYSICALTERMINALSCOMMAND_H
#define REMOVEPHYSICALTERMINALSCOMMAND_H

#include <QPair>
#include <QPointer>
#include <QSharedPointer>
#include <QUndoCommand>
#include <QUuid>
#include <QVector>

class PhysicalTerminal;
class TerminalStrip;

/**
	@brief The RemovePhysicalTerminalsCommand class
	Takes whole physical terminals -- one row of a strip, every level
	of it -- out of their strip, without touching the strip itself:
	this is one step of deleting a terminal row of the terminal
	navigator, or of a terminal the user drew and chose to delete from
	the whole project rather than from the drawing alone.

	redo() drops the rows, undo() puts them back at the very place they
	were in, keeping the physical terminals themselves: their identity
	(the uuid written with the project) is what the drawings and the
	strips remember, so the same objects come back rather than new ones
	-- which is what addAndGroupTerminals() would make, and what the
	whole-strip deletion (RemoveTerminalStripCommand) can afford
	because the whole strip goes away with them.

	Both ways to reach this command -- the navigator and the drawing
	editor -- refuse bridged terminals and multi-level ones the way the
	drawing editor has always refused to delete them
	(DeleteQGraphicsItemCommand::hasNonDeletableTerminal), so a row
	taken here holds neither: there is no bridge to restore, and every
	row is a single terminal.
*/
class RemovePhysicalTerminalsCommand : public QUndoCommand
{
	public:
		RemovePhysicalTerminalsCommand(TerminalStrip *strip,
									   const QVector<QUuid> &physical_terminals,
									   QUndoCommand *parent = nullptr);
		~RemovePhysicalTerminalsCommand() override;

		void undo() override;
		void redo() override;

	private:
		QPointer<TerminalStrip> m_strip;
			/// One row per entry, with the place it had in the strip:
			/// ordered by that place, undo() gives every row its own
			/// slot back, and the rows which stayed in the strip in
			/// between do not move.
		QVector<QPair<int, QSharedPointer<PhysicalTerminal>>> m_rows;
};

#endif //REMOVEPHYSICALTERMINALSCOMMAND_H

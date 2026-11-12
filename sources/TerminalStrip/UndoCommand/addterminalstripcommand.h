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
#ifndef ADDTERMINALSTRIPCOMMAND_H
#define ADDTERMINALSTRIPCOMMAND_H

#include <QUndoCommand>
#include <QPair>
#include <QPointer>
#include <QSharedPointer>
#include <QVector>

class TerminalStrip;
class TerminalStripBridge;
class QETProject;
class RealTerminal;

class AddTerminalStripCommand : public QUndoCommand
{
	public:
		AddTerminalStripCommand(TerminalStrip *strip, QETProject *project, QUndoCommand *parent = nullptr);
		~AddTerminalStripCommand() override;

		void undo() override;
		void redo() override;

	private:
		QPointer<TerminalStrip> m_strip;
		QPointer<QETProject> m_project;
};

class RemoveTerminalStripCommand : public QUndoCommand
{
	public:
		RemoveTerminalStripCommand(TerminalStrip *strip, QETProject *project, QUndoCommand *parent = nullptr);
		~RemoveTerminalStripCommand() override;

		void undo() override;
		void redo() override;

	private:
		QPointer<TerminalStrip> m_strip;
		QPointer<QETProject> m_project;
		/// Terminals of the strip, grouped the way its physical terminals
		/// hold them, so that undo() gives the strip back as it was.
		/// Shared references and not raw pointers: a terminal which is not
		/// drawn in any folio yet has no element to hold it, and a raw
		/// pointer would be a dangling one as soon as this command is the
		/// only thing left remembering it.
		QVector<QVector<QSharedPointer<RealTerminal>>> m_terminals;
		/// The bridges those terminals were part of, one bridge with
		/// the terminals of it which belong to this strip: redo() takes
		/// every terminal out of its bridge, and undo() has to put them
		/// back in, or a strip taken back would come with its bridges
		/// silently gone.
		QVector<QPair<QSharedPointer<TerminalStripBridge>,
					  QVector<QSharedPointer<RealTerminal>>>> m_bridges;
};

#endif // ADDTERMINALSTRIPCOMMAND_H

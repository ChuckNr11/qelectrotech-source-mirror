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
#ifndef REMOVEDIAGRAMCOMMAND_H
#define REMOVEDIAGRAMCOMMAND_H

#include <QUndoCommand>
#include <QPointer>

#include "../TerminalStrip/UndoCommand/terminalstripdeletion.h"

class QETProject;
class Diagram;

/**
	@brief The RemoveDiagramCommand class
	Undo/redo support for removing a folio (diagram) from a project.
	redo() detaches the diagram from the project (and, reactively, its
	ProjectView tab) without destroying it; undo() reinserts it at its
	original position. The diagram is only actually destroyed when this
	command itself is destroyed while the diagram is detached (i.e. while
	the removal is still in effect).

	What the folio draws of the terminals of a strip goes with it: redo()
	takes those terminals off their symbols, so they wait in their rows
	and can be drawn again elsewhere (@sa
	TerminalStripDeletion::terminalsDrawnOn), and undo() binds them back
	to the symbols this command keeps alive with the folio. Leaving them
	bound would cost the project their rows at the next save.
*/
class RemoveDiagramCommand : public QUndoCommand
{
	public:
		RemoveDiagramCommand(QETProject *project, Diagram *diagram, QUndoCommand *parent = nullptr);
		~RemoveDiagramCommand() override;

		void undo() override;
		void redo() override;

	private:
		QPointer<QETProject> m_project;
		Diagram *m_diagram;
		int m_position;
		/// true while the diagram is detached from the project (i.e. the removal is in effect)
		bool m_owns_diagram = false;
		/// the strip terminals the folio drew, taken off their symbols
		/// while the removal is in effect (@sa TerminalStripDeletion::terminalsDrawnOn)
		TerminalStripDeletion::BoundTerminals m_unbound;
};

#endif // REMOVEDIAGRAMCOMMAND_H

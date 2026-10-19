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
#ifndef CREATETERMINALSTRIPCOMMAND_H
#define CREATETERMINALSTRIPCOMMAND_H

#include <QPointer>
#include <QUndoCommand>

class QETProject;
class TerminalNavigatorWidget;
class TerminalStrip;

/**
 * @brief The CreateTerminalStripCommand class
 * Puts a terminal strip into the project, or takes it out again -- the
 * strip the terminal navigator creates through the creator dialog.
 *
 * One command per file, the way the terminal strip editor keeps its
 * commands (sources/TerminalStrip/UndoCommand/): the model half is the
 * same those commands do, add the strip to the project or drop it, but
 * this one also keeps the tree it was created from in step. A terminal
 * strip is not a folio item, so nothing the navigator already listens
 * to -- an element being placed, a strip's own orderChanged() -- says
 * anything about a strip appearing or disappearing, and without the
 * refresh here an undone creation would sit in the tree forever, and a
 * redo would not bring it back.
 *
 * The navigator is followed with a QPointer and is only refreshed while
 * it shows the project this command belongs to: it is a tab of the
 * elements panel and simply shows whichever project the user works on,
 * which need not be this one.
 */
class CreateTerminalStripCommand : public QUndoCommand
{
	public:
		CreateTerminalStripCommand(TerminalStrip *strip,
								   QETProject *project,
								   TerminalNavigatorWidget *navigator,
								   QUndoCommand *parent = nullptr);
		~CreateTerminalStripCommand() override;

		void undo() override;
		void redo() override;

	private:
		void refreshTree(bool select_strip);

	private:
		QPointer<TerminalStrip> m_strip;
		QPointer<QETProject> m_project;
		QPointer<TerminalNavigatorWidget> m_navigator;
};

#endif // CREATETERMINALSTRIPCOMMAND_H

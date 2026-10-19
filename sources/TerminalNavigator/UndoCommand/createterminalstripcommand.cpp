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
#include "createterminalstripcommand.h"

#include "../../TerminalStrip/terminalstrip.h"
#include "../../qetproject.h"
#include "../ui/terminalnavigatorwidget.h"

#include <QObject>

/**
 * @brief CreateTerminalStripCommand::CreateTerminalStripCommand
 * @param strip : the terminal strip to put into the project. It is not
 * created here -- the creator dialog has already put it there, the way
 * the terminal strip editor does -- so that redo() has something to add
 * again after an undo() took it out.
 * @param project : project the strip belongs to
 * @param navigator : the tree to keep in step, or nullptr for none
 * @param parent : parent command, for a macro
 */
CreateTerminalStripCommand::CreateTerminalStripCommand(TerminalStrip *strip,
													   QETProject *project,
													   TerminalNavigatorWidget *navigator,
													   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_strip(strip),
	m_project(project),
	m_navigator(navigator)
{
		//Same wording as AddTerminalStripCommand of the terminal strip
		//editor: the outcome is the same, and the translators already
		//have this one.
	setText(QObject::tr("Add a group of terminals"));
}

CreateTerminalStripCommand::~CreateTerminalStripCommand()
{}

/**
 * @brief CreateTerminalStripCommand::undo
 * Take the strip out of the project again, and let the tree show that:
 * the strip leaves its branch, and a branch which held nothing but this
 * strip goes away with it.
 */
void CreateTerminalStripCommand::undo()
{
	if (m_project && m_strip) {
		m_project->removeTerminalStrip(m_strip);
	}
	refreshTree(false);
}

/**
 * @brief CreateTerminalStripCommand::redo
 * Put the strip back into the project and select it in the tree -- which
 * is what the first redo() of a fresh command already does, since
 * QUndoStack::push() runs redo() itself.
 */
void CreateTerminalStripCommand::redo()
{
	if (m_project && m_strip) {
		m_project->addTerminalStrip(m_strip);
	}
	refreshTree(true);
}

/**
 * @brief CreateTerminalStripCommand::refreshTree
 * Rebuild the tree of @c m_navigator, and select the strip in it if
 * @p select_strip. Nothing happens when there is no navigator left, or
 * when it has meanwhile moved on to another project -- rebuilding it
 * for a strip which is none of its business would drop the selection of
 * that project for no reason.
 * @param select_strip
 */
void CreateTerminalStripCommand::refreshTree(bool select_strip)
{
	if (!m_navigator || !m_project) {
		return;
	}
	if (m_navigator->project() != m_project) {
		return;
	}

	m_navigator->reload();
	if (select_strip) {
		m_navigator->setSelectedStrip(m_strip);
	}
}

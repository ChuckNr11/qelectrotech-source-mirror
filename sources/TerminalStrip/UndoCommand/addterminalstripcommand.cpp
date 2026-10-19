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
#include "addterminalstripcommand.h"
#include "../../qetproject.h"
#include "../physicalterminal.h"
#include "../terminalstrip.h"
#include "../realterminal.h"

#include <QObject>

/**
 * @brief AddTerminalStripCommand::AddTerminalStripCommand
 * @param strip
 * @param parent
 */
AddTerminalStripCommand::AddTerminalStripCommand(TerminalStrip *strip,
												 QETProject    *project,
												 QUndoCommand  *parent) :
	QUndoCommand(parent),
	m_strip(strip),
	m_project(project)
{
	setText(QObject::tr("Add a group of terminals"));
}

AddTerminalStripCommand::~AddTerminalStripCommand()
{}

void AddTerminalStripCommand::undo() {
		if (m_project && m_strip) {
				m_project->removeTerminalStrip(m_strip);
		}
}

void AddTerminalStripCommand::redo() {
	if (m_project && m_strip) {
			m_project->addTerminalStrip(m_strip);
	}
}

/**
 * @brief RemoveTerminalStripCommand::RemoveTerminalStripCommand
 * @param strip
 * @param parent
 */
RemoveTerminalStripCommand::RemoveTerminalStripCommand(TerminalStrip *strip,
													   QETProject    *project,
													   QUndoCommand  *parent) :
	QUndoCommand(parent),
	m_strip(strip),
	m_project(project)
{
		//The terminals are kept here grouped the way the physical terminals
		//hold them: redo() takes the strip out of the project, and then
		//these shared references are the only thing left keeping a terminal
		//which is not drawn in any folio alive.
	for (const auto &phy_t : strip->physicalTerminal()) {
		m_terminals.append(phy_t->realTerminals());
	}
	setText(QObject::tr("Delete a group of terminals"));
}

RemoveTerminalStripCommand::~RemoveTerminalStripCommand()
{}

void RemoveTerminalStripCommand::undo()
{
	if (m_project && m_strip) {
		m_strip->addAndGroupTerminals(m_terminals);
		m_project->addTerminalStrip(m_strip);
	}
}

void RemoveTerminalStripCommand::redo()
{
	if (m_project && m_strip) {
		QVector<QSharedPointer<RealTerminal>> real_t;
		for (const auto &terminal_vector : std::as_const(m_terminals)) {
			real_t.append(terminal_vector);
		}
		m_strip->removeTerminals(real_t);
		m_project->removeTerminalStrip(m_strip);
	}
}

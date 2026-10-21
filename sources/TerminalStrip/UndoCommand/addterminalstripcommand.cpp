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
#include "../terminalstripbridge.h"
#include "../realterminal.h"

#include <QObject>

#include <utility>

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

		//The bridges those terminals are part of go with them, or undo()
		//would give the strip back with its bridges silently gone:
		//redo() takes every terminal out of its bridge, and
		//addAndGroupTerminals() only rebuilds the physical terminals.
	for (const auto &real_t : strip->realTerminals())
	{
		const auto bridge_ = real_t->bridge();
		if (!bridge_) {
			continue;
		}
		bool known_ = false;
		for (auto &pair_ : m_bridges) {
			if (pair_.first == bridge_) {
				pair_.second.append(real_t);
				known_ = true;
				break;
			}
		}
		if (!known_) {
			m_bridges.append(qMakePair(bridge_,
									   QVector<QSharedPointer<RealTerminal>>{real_t}));
		}
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

			//The bridges the terminals were part of are back on too:
			//addAndGroupTerminals() rebuilt the physical terminals, but
			//the terminals it rebuilt are not in any bridge until here.
		for (const auto &pair_ : std::as_const(m_bridges)) {
			m_strip->setBridge(pair_.first, pair_.second);
		}
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

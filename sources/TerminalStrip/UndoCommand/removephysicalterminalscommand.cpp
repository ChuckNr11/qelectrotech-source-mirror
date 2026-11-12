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
#include "removephysicalterminalscommand.h"

#include "../physicalterminal.h"
#include "../realterminal.h"
#include "../terminalstrip.h"

#include <QObject>

#include <algorithm>

/**
 * @brief RemovePhysicalTerminalsCommand::RemovePhysicalTerminalsCommand
 * Take the physical terminals named by @a physical_terminals out of
 * @a strip when the command runs. The uuids which name no row of the
 * strip are left out: the tree this is built from may have been
 * rebuilt while the command was waiting to run, and names rows the
 * project does not have anymore.
 * @param strip
 * @param physical_terminals : the uuids of the rows to take
 * @param parent
 */
RemovePhysicalTerminalsCommand::RemovePhysicalTerminalsCommand(
		TerminalStrip *strip,
		const QVector<QUuid> &physical_terminals,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_strip(strip)
{
	if (strip) {
		for (const auto &uuid_ : physical_terminals) {
			const auto phy_t = strip->physicalTerminal(uuid_);
			if (phy_t) {
				m_rows.append(qMakePair(strip->pos(phy_t), phy_t));
			}
		}
	}

		//Ordered by the place the rows had, so that undo() can walk the
		//list up and give each row its own slot back.
	std::sort(m_rows.begin(), m_rows.end(),
			  [](const QPair<int, QSharedPointer<PhysicalTerminal>> &a,
				 const QPair<int, QSharedPointer<PhysicalTerminal>> &b) {
		return a.first < b.first;
	});

	setText(QObject::tr("Delete %n terminal(s)", "", m_rows.count()));
}

RemovePhysicalTerminalsCommand::~RemovePhysicalTerminalsCommand()
{}

/**
 * @brief RemovePhysicalTerminalsCommand::redo
 * Take every row of this command out of its strip: the strip keeps
 * the rows between them, and only says it changed once
 * (orderChanged()).
 */
void RemovePhysicalTerminalsCommand::redo()
{
	if (!m_strip) {
		return;
	}

	QVector<QSharedPointer<PhysicalTerminal>> phys;
	phys.reserve(m_rows.count());
	for (const auto &row : std::as_const(m_rows)) {
		phys.append(row.second);
	}
	m_strip->removeTerminals(phys);
}

/**
 * @brief RemovePhysicalTerminalsCommand::undo
 * Put every row back where it was, one row at a time from the first
 * place to the last: each row takes the very slot it had, and the
 * rows which stayed in the strip do not move.
 */
void RemovePhysicalTerminalsCommand::undo()
{
	if (!m_strip) {
		return;
	}

	for (const auto &row : std::as_const(m_rows)) {
		m_strip->insertTerminals(row.first, {row.second});
	}
}

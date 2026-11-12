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
#include "changeterminaldata.h"
#include "../../qetgraphicsitem/element.h"

/**
	@brief ChangeTerminalData::ChangeTerminalData
	@param changes : the terminals to write, each with the data it
	should carry from here on
	@param parent
 */
ChangeTerminalData::ChangeTerminalData(const QVector<Change> &changes,
									   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_changes(changes)
{
		//What a terminal is worth right now is what undo() puts back:
		//the copy of the element it is drawn with, or the one it carries
		//itself while it is drawn in no folio yet -- both are copies, so
		//nothing follows the change behind this command's back
		//(@sa RealTerminal::terminalData).
	for (const auto &change : m_changes) {
		m_before.append(change.terminal
						? change.terminal->terminalData()
						: ElementData());
	}
}

/**
	@brief ChangeTerminalData::undo
 */
void ChangeTerminalData::undo()
{
	for (int i = 0 ; i < m_changes.count() ; ++i) {
		apply(i, m_before.at(i));
	}
}

/**
	@brief ChangeTerminalData::redo
 */
void ChangeTerminalData::redo()
{
	for (int i = 0 ; i < m_changes.count() ; ++i) {
		apply(i, m_changes.at(i).after);
	}
}

/**
	@brief ChangeTerminalData::apply
	Write @p data into the terminal of @p index, the way a terminal keeps
	what is its own: through its element when it is drawn in a folio, and
	through its own copy when it is not. A terminal which left the
	project in the meantime is worth nothing here.
 */
void ChangeTerminalData::apply(int index, const ElementData &data) const
{
	const auto terminal = m_changes.at(index).terminal;
	if (!terminal) {
		return;
	}

	if (terminal->isElement() && terminal->element()) {
		terminal->element()->setElementData(data);
	} else {
		terminal->setSpareData(data);
	}
}

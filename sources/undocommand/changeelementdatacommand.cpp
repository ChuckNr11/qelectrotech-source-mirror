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
#include "changeelementdatacommand.h"
#include <qetgraphicsitem/element.h>

ChangeElementDataCommand::ChangeElementDataCommand(Element *element, ElementData new_data, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_element(element),
	m_old_data(element->elementData()),
	m_new_data(new_data)
{
	setText(QObject::tr("Modify the properties of an element"));
}

void ChangeElementDataCommand::undo()
{
	if (!m_element) {
		return;
	}
		//The row goes back to its strip before the data do: one undo
		//step for both (@sa TerminalStripRehome).
	TerminalStripRehome::revert(m_plan);
	m_plan = TerminalStripRehome::Plan();
	m_element.data()->setElementData(m_old_data);
}

void ChangeElementDataCommand::redo()
{
	if (!m_element) {
		return;
	}
		//The label before the change, so that only a name the user
		//really changed moves the row (@sa TerminalStripRehome).
	const auto label_before = TerminalStripRehome::labelOf(m_element.data());
	m_element.data()->setElementData(m_new_data);
	m_plan = TerminalStripRehome::planFor(m_element.data(), label_before);
	TerminalStripRehome::apply(m_plan);
}

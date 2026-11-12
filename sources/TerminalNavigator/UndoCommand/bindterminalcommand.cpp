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
#include "bindterminalcommand.h"

#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetgraphicsitem/terminalelement.h"

/**
 * @brief BindTerminalCommand::BindTerminalCommand
 * @param pending : the terminal of the strip which is not drawn yet
 * @param drawn : the terminal element just placed on a folio
 * @param parent : parent undo command
 */
BindTerminalCommand::BindTerminalCommand(const QSharedPointer<RealTerminal> &pending,
										 TerminalElement *drawn,
										 QUndoCommand *parent) :
	QUndoCommand{parent},
	m_pending{pending},
	m_strip{pending ? pending->parentStrip() : nullptr},
	m_drawn{drawn},
	m_drawn_data_before{drawn ? drawn->elementData() : ElementData()}
{
	setText(QObject::tr("Draw the terminal %1").arg(pending ? pending->label()
															: QString()));
}

/**
 * @brief BindTerminalCommand::redo
 * The terminal takes the place of the pending one in the strip, and the
 * element takes over what was edited for that terminal.
 */
void BindTerminalCommand::redo()
{
	if (!m_strip || m_drawn.isNull() || m_pending.isNull()) {
		return;
	}

		//First the terminal takes the place of the pending one...
	if (!m_strip->replaceRealTerminal(m_pending, m_drawn->realTerminal())) {
		return;
	}

		//... then the element takes over what the terminal strip editor
		//had edited for it. Both halves or none: an element carrying
		//the data of a terminal which sits in no strip would be half
		//bound, and the strip would show the old values for it.
	m_drawn->setElementData(takenOverData());
}

/**
 * @brief BindTerminalCommand::undo
 * The other way round: the element gives the data back, then the
 * pending terminal takes its place in the strip again -- at the same
 * level, so nothing else of the strip moves.
 */
void BindTerminalCommand::undo()
{
	if (!m_strip || m_drawn.isNull() || m_pending.isNull()) {
		return;
	}

	m_drawn->setElementData(m_drawn_data_before);
	m_strip->replaceRealTerminal(m_drawn->realTerminal(), m_pending);
}

/**
 * @brief BindTerminalCommand::takenOverData
 * @return the data of the element with what the pending terminal holds
 * written into them: the values the user set win, the ones never
 * touched keep the values the symbol is drawn with, and the label of
 * the strip is the label of the element.
 */
ElementData BindTerminalCommand::takenOverData() const
{
	ElementData data = m_drawn_data_before;
	const ElementData own = m_pending->terminalData();

		//The terminal type, function and LED of the row: only those the
		//user actually set are taken over. A value the user never chose
		//is the generic one, and writing it over the element would take
		//the terminal definition of the symbol away from it.
	if (own.terminalType() != ElementData::TTGeneric) {
		data.setTerminalType(own.terminalType());
	}
	if (own.terminalFunction() != ElementData::TFGeneric) {
		data.setTerminalFunction(own.terminalFunction());
	}
	if (own.terminalLed()) {
		data.setTerminalLED(true);
	}

		//Every information the terminal strip editor stored with the
		//terminal: the cable and its wire, the color, the section, the
		//width, the catalog entry, the separator plate... Only the keys
		//the pending terminal carries are written, so the informations
		//of the symbol itself (description, manufacturer...) survive.
	for (const auto &key : own.m_informations.keys()) {
		data.m_informations.addValue(key, own.m_informations.value(key));
	}

		//The label last, because it is the label of the strip and has
		//to stay the one it numbered the terminal with: label() reads
		//it from the informations, and a formula of the symbol would
		//keep computing another one (@sa Element::actualLabel())
	const auto label_ = m_pending->label();
	if (!label_.isEmpty()) {
		data.m_informations.addValue(QStringLiteral("label"), label_);
		data.m_informations.remove(QStringLiteral("formula"));
	}

	return data;
}

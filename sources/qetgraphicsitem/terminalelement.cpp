/*
	Copyright 2006-2025 The QElectroTech Team
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
#include "terminalelement.h"
#include"../TerminalStrip/realterminal.h"

/**
	@brief TerminalElement::TerminalElement
	Default constructor
	@param location location of xml definition
	@param qgi parent QGraphicItem
	@param state int used to know if the creation of element have error
*/
TerminalElement::TerminalElement(const ElementsLocation &location,
				 QGraphicsItem *qgi, int *state) :
	Element(location, qgi, state, Element::Terminale)
{
	auto rt = new RealTerminal(this);
	m_real_terminal = rt->sharedRef();
}

TerminalElement::~TerminalElement()
{}

/**
	@brief TerminalElement::initLink
	@param project
*/
void TerminalElement::initLink(QETProject *project) {
	Element::initLink(project);
}

/**
 * @brief TerminalElement::realTerminal
 * @return the real terminal of this terminal element.
 */
QSharedPointer<RealTerminal> TerminalElement::realTerminal() const {
	return m_real_terminal;
}

/**
 * @brief TerminalElement::parentTerminalStrip
 * @return The parent terminal strip of this
 * terminal element or nullptr if not.
 */
TerminalStrip *TerminalElement::parentTerminalStrip() const {
	if (m_real_terminal) {
		return m_real_terminal->parentStrip();
	}
	return nullptr;
}


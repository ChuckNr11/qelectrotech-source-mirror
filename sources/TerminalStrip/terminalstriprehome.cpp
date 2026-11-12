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
#include "terminalstriprehome.h"

#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "physicalterminal.h"
#include "realterminal.h"
#include "terminalstrip.h"
#include "terminalstripautoimport.h"

/**
 * @brief TerminalStripRehome::labelOf
 * @param element
 * @return the label naming @p element, or an empty string when it draws
 * no terminal. A drawn terminal is named by its symbol (@sa
 * RealTerminal::label), which is what makes a symbol edited on a folio
 * rename the row of the terminal navigator.
 */
QString TerminalStripRehome::labelOf(Element *element)
{
	auto *terminal_element = dynamic_cast<TerminalElement *>(element);
	if (!terminal_element) {
		return QString();
	}
	const auto real_t = terminal_element->realTerminal();
	return real_t ? real_t->label() : QString();
}

/**
 * @brief TerminalStripRehome::planFor
 * @param element
 * @param label_before
 * @return what to do with the row of the terminal @p element draws now
 * that its information changed, an empty plan when nothing moves. The
 * rules are told at the namespace (@sa TerminalStripRehome).
 */
TerminalStripRehome::Plan TerminalStripRehome::planFor(Element *element,
													   const QString &label_before)
{
	Plan plan;
	auto *terminal_element = dynamic_cast<TerminalElement *>(element);
	if (!terminal_element) {
		return plan;
	}
	const auto real_t = terminal_element->realTerminal();
	if (real_t.isNull()) {
		return plan;
	}

		//Only a name the user changed moves anything: editing the
		//comment of a terminal which sits in a strip its name does not
		//call for leaves the row where the user put it. What a formula
		//names a terminal is read back the same way as what is typed
		//(@sa RealTerminal::label).
	const QString label_after = real_t->label();
	if (label_after == label_before) {
		return plan;
	}

	auto *from = real_t->parentStrip();
	if (!from) {
		return plan;	//Independent already: nothing to take it out of
	}

		//A bridged row or a row of several levels is the user's to
		//move, the way deletion refuses both (@sa
		//DeleteQGraphicsItemCommand::hasNonDeletableTerminal).
	if (real_t->isBridged()) {
		return plan;
	}
	const auto row = real_t->physicalTerminal();
	if (row.isNull() || row->levelCount() != 1) {
		return plan;
	}

		//A name written "strip:position" names a strip; a name of its
		//own ("PE", "Masse") names a terminal and no strip at all, and
		//keeps the row where it is (@sa the namespace above).
	if (label_after.indexOf(QLatin1Char(':')) <= 0) {
		return plan;
	}
	const auto name_ = TerminalStripAutoImport::stripNameForLabel(label_after);
	if (name_.isEmpty()) {
		return plan;
	}

	auto *diagram_ = element->diagram();
	if (!diagram_ || !diagram_->project()) {
		return plan;
	}
	const auto informations_ = element->elementData().m_informations;
	auto *to = TerminalStripAutoImport::stripOf(
				diagram_->project(), name_,
				TerminalStripAutoImport::normalizedPlant(
					informations_.value(QETInformation::ELMT_PLANT).toString()),
				TerminalStripAutoImport::normalizedLocation(
					informations_.value(QETInformation::ELMT_LOCATION).toString()));
	if (to == from) {
		return plan;	//Home enough: nothing moves
	}

	plan.row = row;
	plan.from = from;
	plan.to = to;	//Null : the name calls for a strip there is none of
	return plan;
}

/**
 * @brief TerminalStripRehome::apply
 * @param plan
 * Do what @p plan says (@sa the declaration): the row leaves its strip
 * for the one its name calls for, or for none at all.
 */
void TerminalStripRehome::apply(const Plan &plan)
{
	if (plan.isEmpty()) {
		return;
	}
	plan.from->removeTerminals(QVector<QSharedPointer<PhysicalTerminal>>{plan.row});
	if (plan.to) {
		plan.to->addTerminals(QVector<QSharedPointer<PhysicalTerminal>>{plan.row});
	}
}

/**
 * @brief TerminalStripRehome::revert
 * @param plan
 * The undo of apply(): the row goes back to the strip @p plan took it
 * from, at the place that strip gives it (@sa MoveTerminalCommand, which
 * moves a row the same way).
 */
void TerminalStripRehome::revert(const Plan &plan)
{
	if (plan.isEmpty()) {
		return;
	}
	if (plan.to) {
		plan.to->removeTerminals(QVector<QSharedPointer<PhysicalTerminal>>{plan.row});
	}
	plan.from->addTerminals(QVector<QSharedPointer<PhysicalTerminal>>{plan.row});
}

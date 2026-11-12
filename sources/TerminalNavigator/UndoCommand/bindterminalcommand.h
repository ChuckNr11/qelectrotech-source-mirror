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
#ifndef BINDTERMINALCOMMAND_H
#define BINDTERMINALCOMMAND_H

#include "../../properties/elementdata.h"

#include <QPointer>
#include <QSharedPointer>
#include <QUndoCommand>

class RealTerminal;
class TerminalElement;
class TerminalStrip;

/**
 * @brief The BindTerminalCommand class
 * Hang a terminal of a strip on a folio: @a drawn -- a terminal element
 * just placed on a folio -- takes the place, the level and the bridge of
 * @a pending, the terminal of the strip which waited for its symbol
 * (@sa RealTerminal::isPending()), and takes over what the terminal
 * strip editor had edited for it.
 *
 * Both halves belong together, redo() and undo() are therefore a pair
 * around one single swap: where the terminal sits in the strip does not
 * change, so no other terminal of the strip moves, and the pending
 * terminal is only out of the strip while the symbol is on the folio.
 *
 * What the element takes over is what makes the strip keep showing the
 * terminal it numbered: its label first, because the strip writes its
 * terminals by it, then the type, function, LED and informations the
 * user typed into the row of the terminal. Only what the user actually
 * set is taken: a symbol which is drawn as a fuse terminal keeps saying
 * so when the pending terminal never said anything itself.
 *
 * The identity follows too, without anything to do here: RealTerminal::
 * uuid() answers with the uuid of the element for a drawn terminal, so
 * every bridge, every address and every command addressing the pending
 * terminal by its own uuid addresses the bound one as soon as the swap
 * is done -- and takes the pending one back on undo().
 */
class BindTerminalCommand : public QUndoCommand
{
	public:
		BindTerminalCommand(const QSharedPointer<RealTerminal> &pending,
							TerminalElement *drawn,
							QUndoCommand *parent = nullptr);
		void undo() override;
		void redo() override;

	private:
		ElementData takenOverData() const;

	private:
		QSharedPointer<RealTerminal> m_pending;
		QPointer<TerminalStrip> m_strip;
		QPointer<TerminalElement> m_drawn;
			/// The data of the element before it took anything over:
			/// what undo() gives back, and what redo() builds the taken
			/// over data from, so a redo after an undo ends up with the
			/// very same result as the first one.
		ElementData m_drawn_data_before;
};

#endif // BINDTERMINALCOMMAND_H

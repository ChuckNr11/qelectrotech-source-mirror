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
#ifndef TERMINALSTRIPREHOME_H
#define TERMINALSTRIPREHOME_H

#include <QSharedPointer>
#include <QString>

class Element;
class PhysicalTerminal;
class TerminalStrip;

/**
	@brief The TerminalStripRehome namespace
	The home of a terminal follows its name: a name which calls for
	another strip ("X1:2" names the strip "X1") takes the row there when
	the user renames the symbol on a folio, and a name no strip answers
	to takes the row out of its strip -- the terminal waits under
	"Independent terminals" of the terminal navigator, its symbol staying
	where it is drawn.

	The naming rule is the one the auto import of a project works by
	(@sa TerminalStripAutoImport::stripNameForLabel, stripOf): a name
	written "strip:position" names a strip, and the plant and the
	location typed on the terminal pick between the strips of that name
	when they are set. A name of its own -- "PE", "Masse" -- names a
	terminal and no strip, and keeps the row where it is: the auto import
	reads such a name as a strip name too, but renaming one terminal is
	not asking for a strip to exist.

	Only what the user can move alone moves here: a bridged row or a row
	of several levels stays where it is, the way deletion refuses both
	(@sa DeleteQGraphicsItemCommand::hasNonDeletableTerminal) -- moving
	one row would drag its levels and its bridges along or break them
	apart. So does a terminal which sits in no strip already: there is
	nothing to take it out of.

	Renaming a terminal is one gesture, so the move is part of the undo
	command which renames it (ChangeElementInformationCommand,
	ChangeElementDataCommand): planFor() is asked right after the change,
	apply() does the move and revert() gives it back -- one undo step for
	the name and the home of a terminal together, the way the user sees
	one thing changing.
*/
namespace TerminalStripRehome
{
	/**
		@brief The Plan struct
		The row of a terminal and the strip it leaves: @a to is the strip
		its name calls for, or null when no strip does, which makes the
		terminal independent (@sa isEmpty).
	*/
	struct Plan
	{
		QSharedPointer<PhysicalTerminal> row;
		TerminalStrip *from = nullptr;
		TerminalStrip *to = nullptr;
		bool isEmpty() const {return row.isNull() || !from || from == to;}
	};

	/**
		@brief labelOf
		@return the label naming @p element -- the label of the terminal
		it draws, or an empty string when @p element is no terminal or
		draws none. Read it before and after a change to know whether the
		name changed at all (@sa planFor).
	*/
	QString labelOf(Element *element);

	/**
		@brief planFor
		@param element : the element whose information just changed
		@param label_before : what labelOf() answered for it before that
		@return what to do with the row of the terminal @p element draws,
		an empty plan when nothing moves: the name did not change, calls
		for the strip the terminal is in already, or belongs to a row
		this never moves alone (@sa the namespace above).
	*/
	Plan planFor(Element *element, const QString &label_before);

	/**
		@brief apply
		Do what @p plan says: the row leaves @a from and joins @a to,
		or joins no strip at all when @a to is null. The row itself is
		not touched -- its levels, its bridges and its position in the
		strip it joins are its own --, the way MoveTerminalCommand moves
		a row.
	*/
	void apply(const Plan &plan);

	/**
		@brief revert
		The undo of apply(): the row goes back to the strip @p plan took
		it from.
	*/
	void revert(const Plan &plan);
}

#endif // TERMINALSTRIPREHOME_H

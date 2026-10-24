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
#ifndef TERMINALSTRIPDELETION_H
#define TERMINALSTRIPDELETION_H

#include <QHash>
#include <QList>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QUuid>
#include <QVector>

class BridgePart;
class CablePart;
class Diagram;
class DiagramContent;
class Element;
class QETProject;
class QUndoCommand;
class RealTerminal;
class TerminalStrip;

/**
	@brief The TerminalStripDeletion class
	Builds the commands which take terminals and strips off a project,
	drawings included. The terminal navigator, the drawing editor (the
	"from the project" answer of its delete dialog) and the terminal
	block manager all delete through these, so that a strip or a
	terminal row leaves the drawings, the navigator and the manager at
	once -- there is no way here to leave a symbol or a plan of a
	strip behind on a folio -- and one undo step brings the whole of
	it back.

	Every command runs its drawings first and the strip model last:
	an undo gives the strips and their terminals back before the
	drawings which hang on them, so a symbol never comes back to a
	terminal which is not there yet (the bindings themselves are held
	by the elements and the terminals, which stay alive as long as the
	commands do).

	What is taken from a strip must not be bridged or part of a
	multi-level terminal: both callers refuse those the way the
	drawing editor has always refused to delete them
	(hasNonDeletableTerminal), before this class is asked anything.
*/
class TerminalStripDeletion
{
	public:
		/**
			@brief stripsDeletionMacro
			The macro which takes every strip of @a strips out of
			@a project with everything drawn for them: their symbols
			on every folio, and their plans -- every folio is built
			for this, since a plan waits in the XML of a folio which
			is not built yet.
			@return the macro to push, or nullptr when there is nothing
			to delete. With @a parent, the macro is one of its children
			instead, and also returned.
		*/
		static QUndoCommand *stripsDeletionMacro(
				QETProject *project,
				const QVector<TerminalStrip *> &strips,
				QUndoCommand *parent = nullptr);

		/**
			@brief terminalsDeletionMacro
			The macro which takes the rows of @a rows (one strip with
			the uuids of its physical terminals, per entry) out of
			@a project, with the symbols those rows hold on any folio.
			The strips themselves stay, with the rows between the ones
			which go.
			@return the macro to push, or nullptr when there is nothing
			to delete. With @a parent, the macro is one of its children
			instead, and also returned.
		*/
		static QUndoCommand *terminalsDeletionMacro(
				QETProject *project,
				const QVector<QPair<TerminalStrip *, QVector<QUuid>>> &rows,
				QUndoCommand *parent = nullptr);

		/**
			@brief drawingDeletionCommand
			The command the drawing editor pushes for a deletion on
			@a diagram: what @a selection holds, the @a cable_parts and
			the @a bridge_parts of it -- lines of their own, which no
			content holds -- plus, when @a rows names rows of strips to
			take off the project with them, the removal of those rows
			and of whatever they drew on other folios. Everything is
			one undo step, lines included.
			With no @a rows this is the plain deletion the editor has
			always pushed. @a text names the whole gesture when there
			are @a rows.
			@return the command to push, never nullptr.
		*/
		static QUndoCommand *drawingDeletionCommand(
				Diagram *diagram,
				const DiagramContent &selection,
				const QList<CablePart *> &cable_parts,
				const QList<BridgePart *> &bridge_parts,
				const QVector<QPair<TerminalStrip *, QVector<QUuid>>> &rows,
				const QString &text);

		/**
			@brief drawingOnlyDeletionCommand
			The command the drawing editor pushes when the user deletes
			what is selected but answers "from the drawing only" to the
			question about a terminal of a strip: the symbols of @a
			selection leave their folio -- cable lines and bridge lines
			and all, exactly like a plain deletion --, but the terminals
			they draw stay in their strips, unbound from those symbols
			(@sa RealTerminal::unbindFromElement). Such a terminal waits
			in its row like one which is not drawn yet, and the terminal
			navigator can draw it on a folio again.
			@return the command to push, never nullptr.
		*/
		static QUndoCommand *drawingOnlyDeletionCommand(
				Diagram *diagram,
				const DiagramContent &selection,
				const QList<CablePart *> &cable_parts,
				const QList<BridgePart *> &bridge_parts);

		/**
			@brief hasDrawnTerminal
			@return true when @a content holds a terminal element which
			is bound to a strip: such a symbol is the drawing of a
			terminal row, and deleting it asks the user what to
			delete -- the drawing alone, or the row with it.
		*/
		static bool hasDrawnTerminal(const DiagramContent &content);

		/**
			@brief rowsOf
			@return the rows of the strips the terminals of @a content
			belong to: one strip with the uuids of its physical
			terminals, one entry per strip -- one row per physical
			terminal, however many levels it has.
		*/
		static QVector<QPair<TerminalStrip *, QVector<QUuid>>> rowsOf(
				const DiagramContent &content);

		/// A terminal of a strip with the element which draws it.
		using BoundTerminal = QPair<QSharedPointer<RealTerminal>, Element *>;
		using BoundTerminals = QVector<BoundTerminal>;

		/**
			@brief unbindTerminals
			Take each terminal of @a terminals off the element it is
			drawn with (@sa RealTerminal::unbindFromElement): the
			terminal keeps the label, the symbol and the data of that
			element as its own and waits in its row, while the element
			itself is left alone.
		*/
		static void unbindTerminals(const BoundTerminals &terminals);

		/**
			@brief rebindTerminals
			The undo of unbindTerminals(): each terminal of @a
			terminals goes back to being the terminal of the element it
			was collected with (@sa RealTerminal::rebindToElement), and
			says nothing of its own anymore.
		*/
		static void rebindTerminals(const BoundTerminals &terminals);
};

#endif // TERMINALSTRIPDELETION_H

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
#include "terminalstripdeletion.h"

#include "../../autoconnect.h"
#include "../../bridge/bridgeundocommands.h"
#include "../../cable/editcablecommand.h"
#include "../../diagram.h"
#include "../../diagramcontent.h"
#include "../../qetgraphicsitem/conductor.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetgraphicsitem/terminal.h"
#include "../../qetgraphicsitem/terminalelement.h"
#include "../../qetproject.h"
#include "../../undocommand/deleteqgraphicsitemcommand.h"
#include "../GraphicsItem/terminalstripitem.h"
#include "../physicalterminal.h"
#include "../realterminal.h"
#include "../terminalstrip.h"
#include "addterminalstripcommand.h"
#include "removephysicalterminalscommand.h"

#include <QCoreApplication>
#include <QObject>
#include <QSet>

/**
 * @brief appendElementsTo
 * Add @a elements to @a content the way DiagramContent's own
 * constructor fills itself from a selection: the elements themselves,
 * and every wire one of them holds an end of -- a wire whose other
 * end goes too moves away with it, one whose other end stays is
 * updated, and both are deleted by the command built from the
 * content, as deleting a symbol through the selection does.
 * The elements already in @a content are left alone.
 */
static void appendElementsTo(DiagramContent &content,
							 const QList<Element *> &elements)
{
	for (auto *element : elements)
	{
		if (!element || content.m_elements.contains(element)) {
			continue;
		}
		content.m_elements << element;
		content.m_selected_items << element;
		if (element->elementData().m_type == ElementData::Terminal) {
			content.m_terminal_elements << static_cast<TerminalElement *>(element);
		}

		for (auto *terminal : element->terminals())
		{
			for (auto *conductor : terminal->conductors())
			{
				auto *other = conductor->terminal1 == terminal
						? conductor->terminal2 : conductor->terminal1;
				if (content.m_elements.contains(other->parentElement())) {
					if (!content.m_conductors_to_move.contains(conductor)) {
						content.m_conductors_to_move << conductor;
					}
				} else if (!content.m_conductors_to_update.contains(conductor)) {
					content.m_conductors_to_update << conductor;
				}
			}
		}
	}
}

/**
 * @brief collectElements
 * Add what @a reals drew to @a contents: one content per folio,
 * holding the symbols bound to those terminals. The folio is read off
 * the element itself, so a terminal drawn anywhere is found wherever
 * its symbol stands -- a terminal bound to a symbol is on a folio
 * which is built by construction, there is nothing lazy to ask for.
 * A symbol already in @a exclude (the selection being deleted right
 * now) or in a content already collected is not collected twice: two
 * delete commands taking the same item would take it out of the folio
 * twice.
 */
static void collectElements(QHash<Diagram *, DiagramContent> &contents,
							const QVector<QSharedPointer<RealTerminal>> &reals,
							const DiagramContent *exclude)
{
	for (const auto &real_t : reals)
	{
		auto *element = real_t->element();
		if (!element || !element->diagram()) {
			continue;		//Not drawn in any folio yet
		}
		if (exclude && exclude->m_elements.contains(element)) {
			continue;
		}
		appendElementsTo(contents[element->diagram()], {element});
	}
}

/**
 * @brief collectPlans
 * Add the plans the strips of @a strips are drawn with to @a contents:
 * the terminal strip items standing for one of those strips, on
 * whatever folio they are. Every folio of the project is built for
 * this -- diagrams() is the way to that --, because a plan waits in
 * the XML of a folio which is not built yet, and a strip deleted with
 * its plan left behind would leave a drawing of nothing there.
 * A plan which names a strip by a uuid the project never resolved
 * (terminalStrip() is null) is none of ours and stays.
 */
static void collectPlans(QHash<Diagram *, DiagramContent> &contents,
						 QETProject *project,
						 const QVector<TerminalStrip *> &strips)
{
	for (auto *diagram : project->diagrams())
	{
		for (auto *item : diagram->items())
		{
			if (item->type() != TerminalStripItem::Type) {
				continue;
			}
			auto *plan = qgraphicsitem_cast<TerminalStripItem *>(item);
			if (!plan || !strips.contains(plan->terminalStrip().data())) {
				continue;
			}
			auto &content = contents[diagram];
			if (!content.m_terminal_strip.contains(plan)) {
				content.m_terminal_strip << plan;
				content.m_selected_items << plan;
			}
		}
	}
}

/**
 * @brief appendDrawings (steps flavour)
 * One DeleteQGraphicsItemCommand per folio @a contents holds, so that
 * every symbol and every plan is taken off its own folio, and one
 * undo puts it back there. The steps are raw: this flavour feeds a
 * BatchCommand, which owns what it holds.
 */
static void appendDrawings(QList<QUndoCommand *> &steps,
						   const QHash<Diagram *, DiagramContent> &contents)
{
	for (auto it = contents.cbegin() ; it != contents.cend() ; ++it) {
		if (!it.value().items(DiagramContent::All).isEmpty()) {
			steps << new DeleteQGraphicsItemCommand(it.key(), it.value());
		}
	}
}

/**
 * @brief appendDrawings (parent flavour)
 * The same, one child of @a parent per folio: this flavour feeds a
 * plain QUndoCommand macro, which owns its children.
 */
static void appendDrawings(QUndoCommand *parent,
						   const QHash<Diagram *, DiagramContent> &contents)
{
	for (auto it = contents.cbegin() ; it != contents.cend() ; ++it) {
		if (!it.value().items(DiagramContent::All).isEmpty()) {
			new DeleteQGraphicsItemCommand(it.key(), it.value(), parent);
		}
	}
}

/**
 * @brief realsOf
 * @return every real terminal the rows of @a rows hold, skipping the
 * rows which name no physical terminal of their strip anymore.
 */
static QVector<QSharedPointer<RealTerminal>> realsOf(
		const QVector<QPair<TerminalStrip *, QVector<QUuid>>> &rows)
{
	QVector<QSharedPointer<RealTerminal>> reals;
	for (const auto &row : rows)
	{
		if (!row.first) {
			continue;
		}
		for (const auto &uuid_ : row.second) {
			const auto phy_t = row.first->physicalTerminal(uuid_);
			if (phy_t) {
				reals << phy_t->realTerminals();
			}
		}
	}
	return reals;
}

/**
 * @brief TerminalStripDeletion::hasDrawnTerminal
 * @param content
 * @return true when @a content holds a terminal element which is
 * bound to a strip
 */
bool TerminalStripDeletion::hasDrawnTerminal(const DiagramContent &content)
{
	for (const auto &element : content.m_terminal_elements) {
		if (!element.isNull() && element->parentTerminalStrip()) {
			return true;
		}
	}
	return false;
}

/**
 * @brief TerminalStripDeletion::rowsOf
 * @param content
 * @return the rows of the strips the terminals of @a content belong
 * to: one entry per strip, holding the uuids of its physical
 * terminals -- one per row, however many levels it has, and once per
 * row however many of its levels are in @a content.
 */
QVector<QPair<TerminalStrip *, QVector<QUuid>>> TerminalStripDeletion::rowsOf(
		const DiagramContent &content)
{
	QHash<TerminalStrip *, QVector<QUuid>> per_strip;
	for (const auto &element : content.m_terminal_elements)
	{
		if (element.isNull() || !element->parentTerminalStrip()) {
			continue;
		}
		auto *strip = element->parentTerminalStrip();
		const auto real_t = element->realTerminal();
		const auto phy_t = real_t ? real_t->physicalTerminal()
								  : QSharedPointer<PhysicalTerminal>();
		if (phy_t.isNull()) {
			continue;
		}
		auto &uuids = per_strip[strip];
		if (!uuids.contains(phy_t->uuid())) {
			uuids << phy_t->uuid();
		}
	}

	QVector<QPair<TerminalStrip *, QVector<QUuid>>> rows;
	for (auto it = per_strip.cbegin() ; it != per_strip.cend() ; ++it) {
		rows.append(qMakePair(it.key(), it.value()));
	}
	return rows;
}

/**
 * @brief TerminalStripDeletion::stripsDeletionMacro
 * @param project
 * @param strips
 * @param parent
 * @return the macro which takes every strip of @a strips out of
 * @a project, drawings included, or nullptr when there is nothing to
 * delete. With @a parent the macro is one of its children instead,
 * and also returned.
 */
QUndoCommand *TerminalStripDeletion::stripsDeletionMacro(
		QETProject *project,
		const QVector<TerminalStrip *> &strips,
		QUndoCommand *parent)
{
	if (!project || strips.isEmpty()) {
		return nullptr;
	}

	QVector<QSharedPointer<RealTerminal>> reals;
	for (auto *strip : strips) {
		reals << strip->realTerminals();
	}

	QHash<Diagram *, DiagramContent> contents;
	collectElements(contents, reals, nullptr);
	collectPlans(contents, project, strips);

	auto *macro = new QUndoCommand(parent);
		//Drawings first, strips last: undo() gives the strips and
		//their terminals back before the drawings which hang on them.
	appendDrawings(macro, contents);
	for (auto *strip : strips) {
		new RemoveTerminalStripCommand(strip, project, macro);
	}

	macro->setText(QObject::tr("Delete %n terminal strip(s)", "", strips.count()));
	return macro;
}

/**
 * @brief TerminalStripDeletion::terminalsDeletionMacro
 * @param project
 * @param rows
 * @param parent
 * @return the macro which takes the rows of @a rows out of @a
 * project, with the symbols they hold on any folio, or nullptr when
 * there is nothing to delete. With @a parent the macro is one of its
 * children instead, and also returned.
 */
QUndoCommand *TerminalStripDeletion::terminalsDeletionMacro(
		QETProject *project,
		const QVector<QPair<TerminalStrip *, QVector<QUuid>>> &rows,
		QUndoCommand *parent)
{
	if (!project || rows.isEmpty()) {
		return nullptr;
	}

	QHash<Diagram *, DiagramContent> contents;
	const auto reals = realsOf(rows);
	collectElements(contents, reals, nullptr);

	auto *macro = new QUndoCommand(parent);
		//Symbols first, rows last, for the same reason as above
	appendDrawings(macro, contents);
	for (const auto &row : rows) {
		if (row.first && !row.second.isEmpty()) {
			new RemovePhysicalTerminalsCommand(row.first, row.second, macro);
		}
	}

	macro->setText(QObject::tr("Delete %n terminal(s)", "", reals.count()));
	return macro;
}

/**
 * @brief TerminalStripDeletion::drawingDeletionCommand
 * The command the drawing editor pushes for a deletion on
 * @a diagram: what @a selection holds, the @a cable_parts and the
 * @a bridge_parts of it -- lines of their own, which no content
 * holds -- plus, when @a rows names rows of strips to take off the
 * project with them, the removal of those rows and of whatever they
 * drew on other folios. Everything is one undo step, lines included.
 * With no @a rows this is the plain deletion the editor has always
 * pushed. @a text names the whole gesture when there are @a rows.
 * @return the command to push, never nullptr.
 */
QUndoCommand *TerminalStripDeletion::drawingDeletionCommand(
		Diagram *diagram,
		const DiagramContent &selection,
		const QList<CablePart *> &cable_parts,
		const QList<BridgePart *> &bridge_parts,
		const QVector<QPair<TerminalStrip *, QVector<QUuid>>> &rows,
		const QString &text)
{
		//A line of a cable or of a bridge is not held by
		//DiagramContent: each is removed by its own command, and the
		//cable or the bridge goes with it when no line of it is left
		//anywhere.
	const bool no_lines = cable_parts.isEmpty() && bridge_parts.isEmpty();

		//What the rows drew, wherever they drew it -- nothing of it
		//when the drawing alone goes (@sa collectElements).
	QHash<Diagram *, DiagramContent> contents;
	collectElements(contents, realsOf(rows), &selection);

	if (no_lines && AutoConnect::isOn(diagram))
	{
			//The terminals the removed symbols stood between, or were
			//wired to, look for a new partner in the same step: one
			//macro, so the wires it makes and the deletion undo
			//together.
		auto *undo = new QUndoCommand(text);
		auto *del = new DeleteQGraphicsItemCommand(diagram, selection, undo);
		if (text.isEmpty()) {
			undo->setText(del->text());
		}
		appendDrawings(undo, contents);

			//What leaves a folio is told to that folio's automatic
			//wires: a row of a strip takes its symbols off folios the
			//selection was not on, and the terminals at the far ends
			//there look for a new partner as well.
		QHash<Diagram *, QSet<Element *>> leaving;
		for (auto *element : selection.m_elements) {
			leaving[diagram] << element;
		}
		for (auto it = contents.cbegin() ; it != contents.cend() ; ++it) {
			for (auto *element : it.value().m_elements) {
				leaving[it.key()] << element;
			}
		}
		for (auto it = leaving.cbegin() ; it != leaving.cend() ; ++it) {
			AutoConnect::update(it.key(), {}, {}, undo, it.value());
		}

		for (const auto &row : rows) {
			if (row.first && !row.second.isEmpty()) {
				new RemovePhysicalTerminalsCommand(row.first, row.second, undo);
			}
		}
		return undo;
	}

	QList<QUndoCommand *> steps;
	if (selection.hasDeletableItems() || no_lines) {
		steps << new DeleteQGraphicsItemCommand(diagram, selection);
	}
	if (!cable_parts.isEmpty()) {
		steps << new RemoveCableCommand(diagram, cable_parts);
	}
	if (!bridge_parts.isEmpty()) {
		steps << new RemoveBridgeCommand(bridge_parts);
	}
	appendDrawings(steps, contents);
	for (const auto &row : rows) {
		if (row.first && !row.second.isEmpty()) {
			steps << new RemovePhysicalTerminalsCommand(row.first, row.second);
		}
	}

		//One single step, so a selection holding both a line and
		//ordinary drawings comes back whole.
	if (rows.isEmpty() && steps.count() == 1) {
		return steps.first();
	}
	return new BatchCommand(steps, text.isEmpty()
			? QCoreApplication::translate("BatchCommand", "Delete")
			: text);
}

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
#include "terminalnavigatorwidgettree.h"
#include "../diagramevent/diagrameventaddterminal.h"

#include <QDrag>
#include <QMimeData>

TerminalNavigatorTree::TerminalNavigatorTree(QWidget *parent) :
	QTreeWidget(parent)
{}

/**
	@brief TerminalNavigatorTree::selectedItemsInOrder
	@return every selected item, in the order the tree shows them:
	top to bottom, the children of a branch below it -- whatever order
	the clicks marked them in.

	The order is the one a run of terminals is drawn in, so it has to
	be the order of the tree itself: selectedItems() answers in an
	order Qt leaves free, which would make the run follow the way the
	user clicked rather than what they see in the tree.
*/
QList<QTreeWidgetItem *> TerminalNavigatorTree::selectedItemsInOrder() const
{
	QList<QTreeWidgetItem *> ordered_;
	for (int i = 0 ; i < topLevelItemCount() ; ++i) {
		collectSelected(topLevelItem(i), ordered_);
	}
	return ordered_;
}

/**
	@brief TerminalNavigatorTree::collectSelected
	Depth-first walk filling @a ordered with the selected items of the
	branch below @a item, parents before children.
	@param item
	@param ordered receives the selected items, in tree order
*/
void TerminalNavigatorTree::collectSelected(
		QTreeWidgetItem *item, QList<QTreeWidgetItem *> &ordered) const
{
	if (!item) {
		return;
	}
	if (item->isSelected()) {
		ordered.append(item);
	}
	for (int i = 0 ; i < item->childCount() ; ++i) {
		collectSelected(item->child(i), ordered);
	}
}

/**
	@brief TerminalNavigatorTree::startDrag
	Ask what the selected items carry (itemsDragged()), and drag them
	only when the answer holds the format of a terminal waiting for its
	symbol: every other item of the tree -- an installation, a strip, a
	free terminal, one already drawn -- gets its payload refused and
	dragged nowhere, which is what keeps this class free of any
	knowledge of strips.

	Several items at a time, in tree order: the terminals of a selection
	are drawn one after another, top to bottom, and only the navigator
	(the slot behind itemsDragged()) knows which of the selected items
	are such terminals.
	@param supportedActions
*/
void TerminalNavigatorTree::startDrag(Qt::DropActions supportedActions)
{
	Q_UNUSED(supportedActions)

	const auto items = selectedItemsInOrder();
	if (items.isEmpty()) {
		return;
	}

	auto *mime = new QMimeData();
	emit itemsDragged(items, mime);

	if (!mime->formats().contains(DiagramEventAddTerminal::pendingTerminalMime())) {
		delete mime;
		return;			//Nothing worth dragging hangs below these items
	}

	auto *drag = new QDrag(this);
	drag->setMimeData(mime);
	if (const auto icon_ = items.constFirst()->icon(0); !icon_.isNull()) {
		drag->setPixmap(icon_.pixmap(32, 32));
	}
	drag->exec(Qt::CopyAction);
}

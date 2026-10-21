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
#ifndef TERMINALNAVIGATORTREE_H
#define TERMINALNAVIGATORTREE_H

#include <QList>
#include <QTreeWidget>

class QMimeData;

/**
	@brief The TerminalNavigatorTree class
	The tree of the terminal navigator tab, with the drag of its own the
	tab needs: terminals of the strip which are drawn in no folio yet can
	be dragged onto a folio, which starts the placement of their symbols
	there, one after another (@sa DiagramEventAddTerminal).

	This class is promoted in terminalnavigatorwidget.ui and knows
	nothing of strips, terminals or folios -- it is a plain QTreeWidget
	which asks what a drag carries: itemsDragged() hands the selected
	items over to whoever fills it in, and nothing is dragged when the
	answer holds no payload. The navigator refuses every item which is
	not a terminal waiting for its symbol, so an installation, a strip,
	a free terminal or a terminal already drawn simply do not drag, and
	the reason stays where it belongs, next to the tree it builds.
*/
class TerminalNavigatorTree : public QTreeWidget
{
		Q_OBJECT

	public:
		explicit TerminalNavigatorTree(QWidget *parent = nullptr);
		QList<QTreeWidgetItem *> selectedItemsInOrder() const;

	signals:
		/**
			@brief itemsDragged
			The tree is about to drag @a items -- every selected item,
			in the order they are shown in the tree: fill @a mime with
			the payload of the drag, in the format of the tool which
			will read it. An empty answer means none of them drags at
			all, and the tree goes back to a plain press.
		*/
		void itemsDragged(const QList<QTreeWidgetItem *> &items,
						  QMimeData *mime);


	protected:
		void startDrag(Qt::DropActions supportedActions) override;

	private:
		void collectSelected(QTreeWidgetItem *item,
							 QList<QTreeWidgetItem *> &ordered) const;
};

#endif // TERMINALNAVIGATORTREE_H

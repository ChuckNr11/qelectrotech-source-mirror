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
#ifndef DIAGRAMEVENTADDTERMINAL_H
#define DIAGRAMEVENTADDTERMINAL_H

#include "../../ElementsCollection/elementslocation.h"
#include "../../diagramevent/diagrameventinterface.h"

#include <QByteArray>
#include <QPair>
#include <QPointer>
#include <QSharedPointer>
#include <QUuid>
#include <QVector>

class QMimeData;
class QStatusBar;
class Element;
class RealTerminal;

/**
	@brief The DiagramEventAddTerminal class
	Draws one terminal -- or a whole run of them -- of terminal strips
	on a folio: the interactive placement mode a drag out of the
	terminal navigator starts (@sa DiagramView::startTerminalPlacement()).

	It behaves the way DiagramEventAddElement does -- the symbol follows
	the cursor on the grid, a left click drops it, Space rotates it, a
	right click gives the whole thing up -- with the differences the
	terminals of a strip make:

	* the symbol is the one the pending terminal carries
	(@a RealTerminal::symbol()), not one the user picks in the elements
	collection panel: the terminal says which element draws it, and the
	symbol is imported into the project's own collection the same way
	a drop from the panel does it, so the element saved with the
	project is the one the strip refers to.

	* a drag may carry several terminals of a multi-selection, and they
	are drawn one per click, in the order they came in: the symbol of
	the next one takes the cursor as soon as the previous one hangs on
	the folio, and finish() is emitted when the last one is placed.

	* giving the run up -- right click, double click or Escape -- leaves
	the terminals already drawn right where they are (every click has
	already pushed its own undo macro) and the others waiting for their
	symbol in the navigator: nothing of those is touched at all.

	The click which drops a symbol binds it to its pending terminal in
	a single undo macro: BindTerminalCommand takes the place, the level
	and the bridge of the pending terminal over and hands it what the
	terminal strip editor had edited for it -- the label first, because
	the strip numbers its terminals by it. One undo takes that symbol
	off the folio again and gives that terminal its place back, data
	included -- one undo per terminal of the run, in the order the
	the run's clicks placed them.
*/
class DiagramEventAddTerminal : public DiagramEventInterface
{
		Q_OBJECT

	public:
			/**
				@param pendings : the terminals which are not drawn in any
				folio yet, in the order they have to be drawn in -- the
				order of the selection in the terminal navigator, top to
				bottom. Every one of them must still wait for its symbol
				in a strip of this project, or nothing is built at all.
				@param diagram : diagram owner of this event
				@param pos : first pos of the symbol (optional)
			*/
		DiagramEventAddTerminal(const QVector<QSharedPointer<RealTerminal>> &pendings,
								Diagram *diagram,
								QPointF pos = QPointF(0,0));
		~DiagramEventAddTerminal() override;

		void mouseMoveEvent        (QGraphicsSceneMouseEvent *event) override;
		void mousePressEvent       (QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent     (QGraphicsSceneMouseEvent *event) override;
		void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event) override;
		void keyPressEvent         (QKeyEvent *event) override;
		void init() override;

			//The drag out of the terminal navigator carries this format:
			//one line per terminal, holding the strip the terminal belongs
			//to and the terminal itself, both by uuid. The format is
			//defined here, with the event it starts, so that the tree
			//producing it (the navigator) and the view reading it
			//(DiagramView) go by one single definition. A drag of a single
			//terminal is exactly one line.
		static QString pendingTerminalMime();
		static QByteArray pendingTerminalPayload(const QUuid &strip_uuid,
												 const QUuid &terminal_uuid);
		static QByteArray pendingTerminalsPayload(
				const QVector<QPair<QUuid, QUuid>> &terminals);
		static bool readPendingTerminalMime(const QMimeData *mime,
											QUuid *strip_uuid,
											QUuid *terminal_uuid);
		static bool readPendingTerminalsMime(
				const QMimeData *mime,
				QVector<QPair<QUuid, QUuid>> *terminals);

	private:
		bool checkPending(const QSharedPointer<RealTerminal> &pending) const;
		bool buildElement();
		void addElement();
		void advance();
		void showMessage(const QString &message);
		void endPlacement();

	private:
			/// Every terminal of the run, in the order they are drawn in
		QVector<QSharedPointer<RealTerminal>> m_queue;
			/// Index in m_queue of the terminal drawn right now
		int m_current_index{0};
			/// The terminal of the run waiting for its symbol right now
			/// (m_queue.at(m_current_index)) -- null before the run starts
		QSharedPointer<RealTerminal> m_pending;
		ElementsLocation m_location;
		Element *m_element;
		QString m_integrate_path;
		QPointer<QStatusBar> m_status_bar;
			/// true once init() took the context menus of the views away:
			/// only then the destructor gives them back, so a construction
			/// which failed before init() leaves the views untouched
		bool m_menus_taken{false};
};

#endif // DIAGRAMEVENTADDTERMINAL_H

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
#ifndef TERMINALNAVIGATORWIDGET_H
#define TERMINALNAVIGATORWIDGET_H

#include <QHash>
#include <QPointer>
#include <QSharedPointer>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <QWidget>

class Element;
class QETProject;
class QMimeData;
class QTreeWidgetItem;
class TerminalStrip;
class RealTerminal;

namespace Ui {
	class TerminalNavigatorWidget;
}

/**
	@brief The TerminalNavigatorWidget class
	Shows the terminal strips of a project as a tree, as a third tab
	("Terminal") of the elements collection panel.

	The tree is the one TerminalStripTreeDockWidget draws inside the
	terminal strip editor, in the same order and with the same items:
	the project as the root, then installation, location, strip and
	terminal, and a second root holding the free terminals. The strips
	are read from the project itself (QETProject::terminalStrip()), the
	free terminals from the folios (ElementProvider::freeTerminal()).

	Unlike the dock this is a plain tab, so free terminals are moved in
	the terminal strip editor -- but terminals of a strip which are not
	drawn in any folio yet can be dragged onto a folio, which places
	their own symbols there and binds them to the terminals, one after
	another: the tree asks what a drag carries
	(pendingTerminalMimeForItems()), and only such terminals get an
	answer, so they are the only ones which drag
	(@sa TerminalNavigatorTree, DiagramEventAddTerminal).

	The tree follows what the folios hold: a terminal placed on a folio
	shows up below the free terminals, and one taken off leaves again --
	QETProject::elementAdded()/elementRemoved() say so for every way an
	element reaches a folio (DiagramEventAddElement, a paste, an undo, a
	drop from the panel). Those signals arrive one per element, so they
	are collected by a timer into a single rebuild.

	The tab also creates strips: a toolbar button and a context menu entry
	both open the creator dialog of the terminal strip editor, prefilled
	with the plant and the location the user is looking at, and the strip
	lands in the project through its own undo command --
	CreateTerminalStripCommand, in sources/TerminalNavigator/UndoCommand/ --
	so it can be taken back with a single undo, tree included.

*/
class TerminalNavigatorWidget : public QWidget
{
	Q_OBJECT

	private:
		enum TreeWidgetType{
			Root,
			Terminal,
			FreeTerminal,
			Installation,
			Location,
			Strip
		};
			//Role used for data in QTreeWidgetItem
		static constexpr int UUID_USER_ROLE{Qt::UserRole + 1};

	public:
		explicit TerminalNavigatorWidget(QWidget *parent = nullptr);
		~TerminalNavigatorWidget();

		void setProject(QETProject *project = nullptr);
		QETProject *project() const;
		void reload();

		bool currentIsStrip() const;
		TerminalStrip* currentStrip() const;
		QString currentInstallation() const;
		QString currentLocation() const;
		void setSelectedStrip(TerminalStrip *strip);
		QSharedPointer<RealTerminal> currentRealTerminal() const;

		/**
			@brief pendingTerminalMimeForItem
			Fill @a mime with the payload which drags @a item onto a
			folio: the strip it hangs below and the terminal of it
			which waits for its symbol, both by uuid. Every other item
			of the tree -- an installation, a strip, a free terminal,
			one already drawn -- leaves @a mime empty and drags
			nothing at all.
			@return true when @a item holds a terminal to be drawn
		*/
		bool pendingTerminalMimeForItem(QTreeWidgetItem *item, QMimeData *mime);

		/**
			@brief pendingTerminalsMimeForItems
			Fill @a mime with the payload which drags every terminal to
			be drawn of @a items onto a folio: one line per item which
			holds one, in the order the items come in -- which is the
			order of the tree, top to bottom, and the order the run
			draws them in. The items which hold no such terminal (an
			installation, a strip, a free terminal, one already drawn)
			simply leave no line.
			@return true when at least one item holds a terminal to be
			drawn
		*/
		bool pendingTerminalsMimeForItems(const QList<QTreeWidgetItem *> &items,
										   QMimeData *mime);


	signals:
		void currentStripChanged(TerminalStrip *strip);

	private slots:
		void on_m_tree_view_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
		void on_m_tree_view_customContextMenuRequested(const QPoint &pos);
		void on_m_add_strip_triggered();
		void onElementChanged();

	private:
		void buildTree();
		QTreeWidgetItem* addTerminalStrip(TerminalStrip *terminal_strip);
		void addFreeTerminal();
		bool pendingTerminalOfItem(QTreeWidgetItem *item,
								   QUuid *strip_uuid,
								   QUuid *terminal_uuid) const;
		void setCurrentStrip(TerminalStrip *strip);
		void connectElementSignals();
		void disconnectElementSignals();
		void createTerminalStrip();

	private:
		Ui::TerminalNavigatorWidget *ui;
		QPointer<QETProject> m_project;
		QPointer<TerminalStrip> m_current_strip;
		QMetaObject::Connection m_project_destroy_connection;

		QHash<QTreeWidgetItem *, QPointer<TerminalStrip>> m_item_strip_H;
		QHash<QUuid, QSharedPointer<RealTerminal>> m_uuid_terminal_H;
		QHash<QUuid, QPointer<TerminalStrip>> m_uuid_strip_H;
		QVector<QMetaObject::Connection> m_strip_changed_connection;
		QVector<QMetaObject::Connection> m_element_changed_connection;
			/// Collects the elementAdded()/elementRemoved() signals into
			/// one rebuild: placing, pasting and loading emit one signal
			/// per element, and rebuilding the whole tree per signal
			/// would be a visible stall on a big project.
		QTimer m_reload_timer;
		bool m_current_is_free_terminal{false};
};

#endif // TERMINALNAVIGATORWIDGET_H

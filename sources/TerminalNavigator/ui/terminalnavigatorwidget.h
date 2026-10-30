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
class QMenu;
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

	The marks stay on one side of the tree at a time. The project and
	the independent terminals are the two roots, and a mark on one of
	them takes every mark of the other side away -- an independent
	terminal belongs to no strip, and a strip or a branch marked
	beside it would have the answers of the tab say two things at
	once. Inside the project several items are marked together only
	within one terminal strip (the strip itself with its rows, or
	rows of it), so a mark in a second strip takes the marks of the
	first one away, and everything else -- the project, an
	installation, a location -- is worth a mark of its own only. The
	marks the user sets come in one click or key at a time, and it is
	always the one which just came in that says which side is meant;
	what the tab answers the drag, the delete and the buttons of the
	toolbar -- is what was kept of the selection
	(@sa onSelectionChanged).

	The tab also creates strips: a toolbar button and a context menu entry
	both open the creator dialog of the terminal strip editor, prefilled
	with the plant and the location the user is looking at, and the strip
	lands in the project through its own undo command --
	CreateTerminalStripCommand, in sources/TerminalNavigator/UndoCommand/ --
	so it can be taken back with a single undo, tree included.

	The tab also deletes, with the context menu, the Delete key and a
	delete button of the toolbar: a terminal row takes its symbols off
	the drawings and itself out of its strip in one undo step, without
	asking -- which is what the drawing editor does with a terminal
	too, once its own delete dialog answered "from the project".
	Anything above a row (the project, an installation, a location, a
	strip) says what it takes with it and asks first, since a whole
	strip goes with its symbols and its plans on every folio
	(@sa TerminalStripDeletion, in
	sources/TerminalStrip/UndoCommand/). The free terminals go the
	same way as a row, but the other direction: they belong to no
	strip, so nothing leaves a strip -- their symbols leave the folio
	they are drawn on, wires included, and that is all
	(@sa TerminalStripDeletion::freeTerminalsDeletionMacro). A single
	or several marked ones go without asking, the way a row does;
	the root of them says what it takes and asks, like the project
	above it does. The toolbar button waits for a terminal row to be
	marked, of a strip or of the free terminals -- deleting a strip
	or a whole branch asks its question and stays in the context menu
	and on the Delete key, and both buttons follow the selection
	(@sa updateActionsState).

	Drawing the terminals which wait for their symbol is the drag
	onto a folio, and the context menu says the same in words
	("Draw"): the selection is taken the way the drag takes it, in
	the order of the tree, and the symbols are placed on the drawing
	the editor shows, one click each
	(@sa drawSelection, DiagramEventAddTerminal).
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

	public slots:
		/**
			@brief deleteSelection
			Take what the tree has selected off the project: a
			terminal row goes without asking -- its symbols leave the
			drawings with it, one undo step --, anything above a row
			(the project, an installation, a location, a strip) says
			what it takes with it and asks first, because a whole
			strip goes with its symbols and its plans on every folio.
			The free terminals of the second root go without asking
			too when they are marked on their own: they belong to no
			strip, so only their symbols leave the folios they are
			drawn on, wires included. Marked as the whole root they
			ask like a branch does, since every one of them goes at
			once then.
		*/
		void deleteSelection();

		/**
			@brief drawSelection
			Draw the terminals of the selection which wait for their
			symbol, the way a drag of the same selection does: one run
			of placements on the drawing the editor shows, one click
			per terminal, in the order of the tree. The entry of the
			context menu for it is greyed out as long as no row of the
			selection waits for its symbol.
		*/
		void drawSelection();

		/**
			@brief editSelection
			Open the dialog of the properties for the terminal rows
			marked in the tree, and give every one of them what the OK
			button chose there -- the values of the first terminal in
			the order of the tree are what that dialog opens on
			(@sa TerminalPropertiesDialog). That dialog asks for no
			symbol at all: a terminal which exists already is drawn with
			the symbol of its element.

			Only the rows of a terminal strip are worth this -- an
			independent terminal belongs to no strip and is what its
			element is --, so the button of the toolbar and the entry of
			the context menu stay greyed out on that side of the tree
			(@sa updateActionsState). One undo step takes the change
			back, all of the marked terminals at once
			(@sa ChangeTerminalData).
		*/
		void editSelection();

	protected:
		/**
			@brief confirmGroupDeletion
			Ask before @p strip_count strips and @p terminal_count
			terminal rows leave the project the way deleteSelection()
			asked for it: more than a single row goes -- the symbols
			and the plans of the strips on the folios too --, so the
			answer is wanted before anything moves. @p terminal_count
			is everything the drawings lose a symbol by: the rows
			marked on their own plus the rows of the strips which go
			as a whole plus the free terminals the root of them takes
			with it. A single terminal row, which the drawing
			editor deletes without asking either, does not come
			through here -- neither does a free terminal marked on
			its own. With @p strip_count at 0 nothing but free
			terminals goes, and the question says so: no plan and no
			strip is involved then, only the symbols on the folios.
			@return true to go on with the deletion
		*/
		virtual bool confirmGroupDeletion(int strip_count, int terminal_count);

	signals:
		void currentStripChanged(TerminalStrip *strip);

	protected:
		/**
			@brief buildContextMenu
			Build the context menu the tree shows for @a item: the
			creation of a strip, the drawing of what waits for its
			symbol, and the deletion of what is marked. The "New
			terminal strip" entry is left out when @a item is one of
			the independent terminals or their root -- those belong to
			no strip, and adding a strip below them makes no sense
			(@sa isInFreeTerminalBranch). Kept out of the slot which
			shows the menu, so that the entries can be read without a
			real menu in the way.
			@return the menu, owned by the caller.
		*/
		QMenu *buildContextMenu(QTreeWidgetItem *item);

	private slots:
		void on_m_tree_view_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
		void on_m_tree_view_customContextMenuRequested(const QPoint &pos);
		void on_m_add_strip_triggered();
		void on_m_edit_selection_triggered();
		void onElementChanged();
		/**
			@brief onSelectionChanged
			Every mark the user (or a test, or the panel of another
			window) sets comes through here, since the tree says so on
			any change: the marks are first brought down to what the
			tree allows, then the buttons are told what is left of the
			selection (@sa updateActionsState).

			What the tree allows is one branch of it at a time: the
			project on one side, the independent terminals on the
			other -- a mark on one side takes every mark of the other
			side away, and the way back is the same. Inside the
			project several items are marked together only within one
			terminal strip: the strip itself with its rows, or rows of
			it -- a mark in a second strip takes the marks of the
			first one away. Everything else (the project, an
			installation, a location) is a single mark, and a second
			one beside it takes the first away.

			The item the marks are measured against is the one which
			just came in (@p added below), which is the item the user
			clicked or the keyboard landed on -- marks which only
			leave cannot make the selection illegal, so nothing is
			checked for those.
		*/
		void onSelectionChanged();
		/**
			@brief updateActionsState
			Say what the selection is worth to the buttons which
			follow it (@sa the constructor): the delete button of the
			toolbar waits for a terminal row to be marked -- a row of
			a strip, which goes out of its strip, or a free terminal,
			which goes off its folio --,
			the "Draw" entry of the context menu for one which waits
			for its symbol, and the "New terminal strip" button for a
			selection which is not one of the independent terminals:
			those belong to no strip, and a strip added below them
			would stand in their branch for nothing. Called after
			every mark is settled and after every rebuild: a rebuild
			marks nothing, and a button must not stay enabled for a
			selection which is gone.
		*/
		void updateActionsState();

	private:
		void buildTree();
		QTreeWidgetItem* addTerminalStrip(TerminalStrip *terminal_strip);
		void addFreeTerminal();
		bool pendingTerminalOfItem(QTreeWidgetItem *item,
								   QUuid *strip_uuid,
								   QUuid *terminal_uuid,
								   QSharedPointer<RealTerminal> *real = nullptr) const;
		/**
			@brief isInFreeTerminalBranch
			@return true when @a item is the root of the independent
			terminals or hangs below it.
		*/
		bool isInFreeTerminalBranch(QTreeWidgetItem *item) const;
		/**
			@brief selectionScopeKey
			What the marks of a selection must agree on
			(@sa onSelectionChanged): the root of the independent
			terminals for anything below it, the item of the strip for
			the strip and for its rows -- the one item which makes
			their selection a single one, marks of another strip are
			not of it --, and the item itself for everything else,
			which is worth a mark of its own only.
			@return the item every mark of a legal selection is equal
			to, or nullptr when there is nothing to say about @a item.
		*/
		QTreeWidgetItem *selectionScopeKey(QTreeWidgetItem *item) const;
		/**
			@brief nodePathOf
			What @a item stands for in the tree: its text and the text
			of every item above it, the last one being its own. A
			rebuild takes every item away with it, and this is how the
			node the user was on is read back afterwards -- the very
			node the dialog of a new strip is called from
			(@sa reload, itemForNodePath).
			@return the texts from the root of the tree down to
			@a item, empty when there is no @a item.
		*/
		QStringList nodePathOf(QTreeWidgetItem *item) const;
		/**
			@brief itemForNodePath
			@param path : what nodePathOf answered
			@return the item which stands for that very node now, or
			nullptr when the tree holds it no more.
		*/
		QTreeWidgetItem *itemForNodePath(const QStringList &path) const;
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
			/// The free terminals of the second root, read by uuid the
			/// way the strips are: their item holds the uuid of their
			/// element, and this answers the element itself for the
			/// deletion of them (@sa deleteSelection). QPointer since
			/// a folio taken off the project takes its symbols with it
			/// while the tree may still show them, until the signal of
			/// the project rebuilds it.
		QHash<QUuid, QPointer<Element>> m_uuid_free_terminal_H;
			/// The marks the tree accepted the last time, the ones the
			/// marks which just came in are weighed against (@sa
			/// onSelectionChanged): minus this, the new ones say which
			/// side of the tree was meant. The items are rebuilt with
			/// every reload, so this goes with them.
		QList<QTreeWidgetItem *> m_marked_snapshot;
			/// Set while onSelectionChanged() takes marks away again:
			/// that changes the selection a second time, and the
			/// second round must not weigh and take away once more.
		bool m_filtering_selection{false};
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

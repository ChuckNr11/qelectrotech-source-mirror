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
#include "terminalnavigatorwidget.h"
#include "ui_terminalnavigatorwidget.h"

#include "../../diagram.h"
#include "../../diagramview.h"
#include "../../elementprovider.h"
#include "../../qetapp.h"
#include "../../qetdiagrameditor.h"
#include "../../qetgraphicsitem/terminalelement.h"
#include "../../qeticons.h"
#include "../../qetinformation.h"
#include "../../qetmessagebox.h"
#include "../../qetproject.h"
#include "../../TerminalStrip/physicalterminal.h"
#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../../TerminalStrip/UndoCommand/terminalstripdeletion.h"
#include "../../utils/qetutils.h"
#include "../UndoCommand/createterminalstripcommand.h"
#include "../diagramevent/diagrameventaddterminal.h"
#include "addterminalstripdialog.h"
#include "terminalnavigatorwidgettree.h"

#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QScopedPointer>
#include <QUndoStack>

#include <algorithm>
#include <utility>

TerminalNavigatorWidget::TerminalNavigatorWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::TerminalNavigatorWidget)
{
	ui->setupUi(this);

	ui->m_tree_view->expandRecursively(ui->m_tree_view->rootIndex());

		//Several terminals are marked at once to be drawn in one run:
		//Ctrl and Shift extend the selection, and the drag carries every
		//selected terminal, in the order the tree shows them
		//(@sa TerminalNavigatorTree::selectedItemsInOrder). What such a
		//selection may hold is one branch at a time, and one strip of
		//the project at most: the marks which do not belong are taken
		//away again as soon as they come in, before any of the answers
		//of the tab sees them (@sa onSelectionChanged).
	ui->m_tree_view->setSelectionMode(QAbstractItemView::ExtendedSelection);

		//The tab drags its own payload: terminals of a strip which are
		//not drawn in any folio yet, onto a folio of the project. The
		//tree asks what the selected items carry, and only such a
		//terminal gets an answer -- every other item drags nothing at
		//all, which is all DragOnly needs to let go of the drops:
		//nothing falls into this tree, free terminals are moved in the
		//strip editor.
	ui->m_tree_view->setDragEnabled(true);
	ui->m_tree_view->setDragDropMode(QAbstractItemView::DragOnly);
	connect(ui->m_tree_view, &TerminalNavigatorTree::itemsDragged,
			this, &TerminalNavigatorWidget::pendingTerminalsMimeForItems);

		//The Delete key deletes what is selected, the way the context
		//menu and the toolbar button do (@sa deleteSelection), and the
		//marks also say what the toolbar buttons are worth: the delete
		//button waits for a terminal row to be marked -- of a strip or
		//of the free terminals --, the "New terminal strip" button for
		//a selection which stands on the project side, and the "Draw"
		//entry of the context menu for a row which waits for its
		//symbol. The two of them start disabled, in the .ui file; the
		//strip button starts disabled here too, since there is no
		//project to add a strip to yet (@sa setProject).
		//
		//The marks are settled first (@sa onSelectionChanged), the
		//buttons are told after: one connection, in that order, since
		//the settling takes marks away and would say so itself.
	connect(ui->m_tree_view, &TerminalNavigatorTree::deleteRequested,
			this, &TerminalNavigatorWidget::deleteSelection);
	connect(ui->m_tree_view, &QTreeWidget::itemSelectionChanged,
			this, &TerminalNavigatorWidget::onSelectionChanged);
	connect(ui->m_delete_selection, &QAction::triggered,
			this, &TerminalNavigatorWidget::deleteSelection);
	connect(ui->m_draw_selection, &QAction::triggered,
			this, &TerminalNavigatorWidget::drawSelection);

		//No project yet: adding a strip has nowhere to go until the
		//panel hands one over. setProject() switches it on again.
	ui->m_add_strip->setEnabled(false);

		//Elements are reported one signal at a time -- a paste of thirty
		//terminals emits thirty of them, a project load one per element
		//of every folio. Rebuilding the tree per signal would be a
		//visible stall, so the signals only restart this timer, and the
		//tree is rebuilt once, when the burst is over. A single shot
		//with no interval fires as soon as the event loop turns.
	m_reload_timer.setSingleShot(true);
	m_reload_timer.setInterval(0);
	connect(&m_reload_timer, &QTimer::timeout,
			this, &TerminalNavigatorWidget::reload);
}

TerminalNavigatorWidget::~TerminalNavigatorWidget()
{
	delete ui;
}

/**
 * @brief TerminalNavigatorWidget::setProject
 * Set @p project as the project handled by this navigator.
 * If a previous project was set, everything is cleared.
 * The destruction of the project is tracked: with the project pointer
 * deleted there is no need to call this function with a nullptr,
 * everything is made inside this class.
 * @param project
 */
void TerminalNavigatorWidget::setProject(QETProject *project)
{
	if (m_project && m_project_destroy_connection) {
		disconnect(m_project_destroy_connection);
	}
	disconnectElementSignals();
	m_project = project;
	if (m_project) {
			//`this` as context: this widget can be deleted before the
			//project (with the editor window that owns it), and the
			//connection must go with it
		m_project_destroy_connection = connect(m_project, &QObject::destroyed, this, [this]() {
				//The project is going away: drop both pointers before
				//reloading, so buildTree() can never read a project that
				//is already half destroyed.
			this->m_project.clear();
			this->m_current_strip.clear();
			this->disconnectElementSignals();
			this->ui->m_add_strip->setEnabled(false);
			this->reload();
		});
		connectElementSignals();
	}
		//Nothing can be added to no project at all.
	ui->m_add_strip->setEnabled(bool(m_project));
	m_current_strip.clear();
	reload();
}

/**
 * @brief TerminalNavigatorWidget::connectElementSignals
 * Follow the elements of the project's folios, so a terminal placed on
 * a folio shows up below the free terminals and one taken off leaves
 * again. Only terminals matter here: any other element -- a resistor, a
 * drawing item -- leaves the tree exactly as it was, and rebuilding it
 * for those would be work for nothing.
 */
void TerminalNavigatorWidget::connectElementSignals()
{
	if (!m_project) {
		return;
	}

	const auto on_element = [this](Element *element) {
		if (element && element->elementData().m_type == ElementData::Terminal) {
			this->onElementChanged();
		}
	};

	m_element_changed_connection.append(
				connect(m_project, &QETProject::elementAdded, this, on_element));
	m_element_changed_connection.append(
				connect(m_project, &QETProject::elementRemoved, this, on_element));

		//A symbol edited on a folio renames the row it draws -- the
		//label of a terminal is read from its element --, and the data
		//of a strip renamed are the same thing for a whole branch.
		//Both are told through the project (@sa QETProject::
		//elementDataChanged, terminalStripDataChanged), and both
		//rebuild through the timer like the element signals above: a
		//search and replace sends one signal per symbol. The rows
		//moving say nothing here on purpose -- the tree is rebuilt
		//under whoever works with its marked rows only when something
		//they show changes; the strip's own orderChanged(), which
		//reload() takes from every strip of the tree, covers those.
	m_element_changed_connection.append(
				connect(m_project, &QETProject::elementDataChanged, this, on_element));
	m_element_changed_connection.append(
				connect(m_project, &QETProject::terminalStripDataChanged,
						this, [this](TerminalStrip *) {
							this->onElementChanged();
						}));

		//A folio which leaves the project takes the drawings of the
		//terminals of the strips it holds with it: those terminals go
		//back to waiting in their rows (@sa
		//TerminalStripDeletion::terminalsDrawnOn) and nothing says so
		//through the elements -- their symbols stay alive with the
		//folio, for the undo which may bring both back. Batched like
		//the element signals above, since a gesture over several
		//folios sends one of these per folio.
	m_element_changed_connection.append(
				connect(m_project, &QETProject::diagramRemoved,
						this, [this](QETProject *, Diagram *) {
							this->onElementChanged();
						}));

		//A strip which joins or leaves the project -- created here,
		//deleted here, deleted in the terminal block manager or taken
		//out by the script API -- rebuilds the tree right away, and
		//they are the only way the tree learns of a strip which went
		//elsewhere: before them each view refreshed only itself.
		//
		//Directly, unlike the element signals above: those come in
		//flurries (one per element, e.g. when a folio is deleted) and
		//are worth batching into the timer, while a strip signal comes
		//exactly once per gesture. A pending timer rebuild would also
		//fire inside a dialog opened later on, and reload() only
		//restores the selection of a strip -- a location picked in the
		//tree would be lost under the feet of whoever prefills such a
		//dialog from it.
	m_element_changed_connection.append(
				connect(m_project, &QETProject::terminalStripAdded,
						this, &TerminalNavigatorWidget::reload));
	m_element_changed_connection.append(
				connect(m_project, &QETProject::terminalStripRemoved,
						this, &TerminalNavigatorWidget::reload));
}

/**
 * @brief TerminalNavigatorWidget::disconnectElementSignals
 */
void TerminalNavigatorWidget::disconnectElementSignals()
{
	for (const auto &connection_ : std::as_const(m_element_changed_connection)) {
		disconnect(connection_);
	}
	m_element_changed_connection.clear();
	m_reload_timer.stop();
}

/**
 * @brief TerminalNavigatorWidget::onElementChanged
 * A terminal was placed on a folio or taken off one. The tree is not
 * rebuilt here but when the event loop turns: an insertion is reported
 * one signal at a time, and a paste or a load sends a whole burst of
 * them, which this timer turns into a single rebuild.
 */
void TerminalNavigatorWidget::onElementChanged()
{
	if (!m_project) {
		return;
	}

	m_reload_timer.start();
}

/**
 * @brief TerminalNavigatorWidget::project
 * @return the project handled by this navigator, or nullptr.
 */
QETProject *TerminalNavigatorWidget::project() const
{
	return m_project;
}

/**
 * @brief TerminalNavigatorWidget::reload
 * Rebuild the tree from the current project, keeping the strip which
 * was selected if it is still there.
 */
void TerminalNavigatorWidget::reload()
{
	auto current_ = m_current_strip;

	ui->m_tree_view->clear();
	m_item_strip_H.clear();
	m_uuid_terminal_H.clear();
	m_uuid_strip_H.clear();
	m_uuid_free_terminal_H.clear();
		//The marks of the tree went with the items above: the ones
		//the next mark is weighed against must go too, or they would
		//be weighed against items which are deleted by now
		//(@sa onSelectionChanged).
	m_marked_snapshot.clear();

	for (const auto &connection_ : std::as_const(m_strip_changed_connection)) {
		disconnect(connection_);
	}
	m_strip_changed_connection.clear();

	buildTree();

	ui->m_tree_view->expandRecursively(ui->m_tree_view->rootIndex());

		//clear() above marked nothing anymore, so the toolbar buttons
		//say so too: none of them stays enabled for a selection which
		//is gone.
	updateActionsState();

		//Reselect the tree widget item of the strip which was current
	auto item = m_item_strip_H.key(current_);
	if (item) {
		ui->m_tree_view->setCurrentItem(item);
	}
	else if (current_ && m_current_strip) {
			//The strip which was selected is not part of the tree
			//anymore: undone, or deleted in the terminal strip editor.
			//Holding on to the pointer would make currentStrip() answer
			//for a strip the project does not have, and
			//currentInstallation()/currentLocation() would offer a
			//branch which is gone -- both feed the creator dialog.
			//clear() above has normally said this already through
			//currentItemChanged(); the check on m_current_strip keeps
			//this a safety net instead of a second signal.
		setCurrentStrip(nullptr);
	}
}

/**
 * @brief TerminalNavigatorWidget::currentIsStrip
 * @return true if the current selected item is a terminal strip.
 */
bool TerminalNavigatorWidget::currentIsStrip() const {
	return m_item_strip_H.contains(ui->m_tree_view->currentItem());
}

/**
 * @brief TerminalNavigatorWidget::currentStrip
 * @return The current selected strip or nullptr if there is
 * no strip selected;
 */
TerminalStrip *TerminalNavigatorWidget::currentStrip() const {
	return m_current_strip;
}

/**
 * @brief TerminalNavigatorWidget::currentInstallation
 * @return the installation according to the current selection
 */
QString TerminalNavigatorWidget::currentInstallation() const
{
	if (m_current_strip) {
		return m_current_strip->installation();
	}

	if (auto item = ui->m_tree_view->currentItem())
	{
		if (item->type() == Location) {
			item = item->parent();
		}
		if (item->type() == Installation) {
			return item->data(0, Qt::DisplayRole).toString();
		}
	}

	return QString();
}

/**
 * @brief TerminalNavigatorWidget::currentLocation
 * @return the location according to the current selection
 */
QString TerminalNavigatorWidget::currentLocation() const
{
	if (m_current_strip) {
		return m_current_strip->location();
	}

	if (auto item = ui->m_tree_view->currentItem()) {
		if (item->type() == Location) {
			return item->data(0, Qt::DisplayRole).toString();
		}
	}

	return QString();
}

/**
 * @brief TerminalNavigatorWidget::setSelectedStrip
 * @param strip
 */
void TerminalNavigatorWidget::setSelectedStrip(TerminalStrip *strip) {
	ui->m_tree_view->setCurrentItem(m_item_strip_H.key(strip));
}

/**
 * @brief TerminalNavigatorWidget::currentRealTerminal
 * @return the current real terminal or a null QSharedPointer.
 */
QSharedPointer<RealTerminal> TerminalNavigatorWidget::currentRealTerminal() const
{
	if (auto item = ui->m_tree_view->currentItem()) {
		if (item->type() == Terminal) {
			return m_uuid_terminal_H.value(item->data(0, UUID_USER_ROLE).toUuid());
		}
	}
	return QSharedPointer<RealTerminal>();
}

/**
 * @brief TerminalNavigatorWidget::on_m_tree_view_currentItemChanged
 * Tell which strip the current item stands for, and the buttons of the
 * toolbar what that is worth: the context menu moves the current item
 * to the item under the cursor without marking anything, and "New
 * terminal strip" is not offered for the side of the independent
 * terminals (@sa updateActionsState).
 * @param current
 * @param previous
 */
void TerminalNavigatorWidget::on_m_tree_view_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous)

	if (!current) {
		m_current_is_free_terminal = false;
		setCurrentStrip(nullptr);
		updateActionsState();
		return;
	}

	TerminalStrip *strip_ = nullptr;
	bool current_is_free{false};
	const auto current_type{current->type()};
	if (current_type == Strip) {
		strip_ = m_item_strip_H.value(current);
	}
	else if (current_type == Terminal && current->parent())
	{
		const auto parent_type{current->parent()->type()};
		if (parent_type == Strip) {
			strip_ = m_item_strip_H.value(current->parent());
		} else if (parent_type == FreeTerminal) {
			current_is_free = true;
		}
	}

		//The flag must follow every selection change, or a reload of the
		//tree leaves it stale and the next click on a free terminal
		//shows nothing (#1306)
	if (strip_ != m_current_strip || current_is_free != m_current_is_free_terminal) {
		m_current_is_free_terminal = current_is_free;
		setCurrentStrip(strip_);
	}

	updateActionsState();
}

/**
 * @brief TerminalNavigatorWidget::on_m_add_strip_triggered
 * The toolbar button: create a terminal strip the way the terminal strip
 * editor does, in the branch the user is looking at.
 */
void TerminalNavigatorWidget::on_m_add_strip_triggered()
{
	createTerminalStrip();
}

/**
 * @brief TerminalNavigatorWidget::on_m_tree_view_customContextMenuRequested
 * Right click offers what the item under the cursor is worth: the
 * clicked item becomes the current one first, so that the entries
 * which follow it and the dialog behind "New terminal strip" are about
 * that item and its branch, and the menu itself is the one
 * buildContextMenu() puts together for it.
 * @param pos
 */
void TerminalNavigatorWidget::on_m_tree_view_customContextMenuRequested(const QPoint &pos)
{
	if (!m_project) {
		return;
	}

	auto *item = ui->m_tree_view->itemAt(pos);
	if (item) {
		ui->m_tree_view->setCurrentItem(item);
	}

	QScopedPointer<QMenu> menu(buildContextMenu(item));
	menu->exec(ui->m_tree_view->viewport()->mapToGlobal(pos));
}

/**
 * @brief TerminalNavigatorWidget::buildContextMenu
 * @param item the item under the cursor, or nullptr when the click
 * found no item at all
 * @return the context menu of @a item, owned by the caller
 */
QMenu *TerminalNavigatorWidget::buildContextMenu(QTreeWidgetItem *item)
{
	auto *menu = new QMenu(this);

		//The creation of a strip is for the project side of the tree:
		//an independent terminal belongs to no strip, and a strip made
		//below that root would stand in its branch for nothing
		//(@sa isInFreeTerminalBranch). With no item under the cursor
		//the current one says which side is meant, the way the button
		//of the toolbar does (@sa updateActionsState).
	auto *scope_item = item ? item : ui->m_tree_view->currentItem();
	if (!isInFreeTerminalBranch(scope_item)) {
		menu->addAction(ui->m_add_strip);
	}

		//Drawing a terminal which waits for its symbol is the drag
		//onto a folio; this entry is the same run without the drag,
		//and follows the selection the same way (@sa drawSelection).
	menu->addAction(ui->m_draw_selection);

		//Deleting is for what can leave the project from here: a strip,
		//a terminal row of one, or a free terminal with its symbol.
		//deleteSelection() alone knows what each item of the
		//selection is worth -- the entry is only greyed out when
		//there is nothing selected at all.
	auto *delete_ = menu->addAction(QET::Icons::EditDelete, tr("Delete"));
	delete_->setEnabled(!ui->m_tree_view->selectedItems().isEmpty());
	connect(delete_, &QAction::triggered,
			this, &TerminalNavigatorWidget::deleteSelection);

	return menu;
}

/**
 * @brief TerminalNavigatorWidget::createTerminalStrip
 * Open the navigator's own strip creation dialog, prefilled with the plant
 * and the location of the current selection and, when that selection is a
 * terminal strip or one of its terminals, with the name of that strip as
 * well -- currentStrip() tells the difference, an installation, a location
 * or a free terminal belonging to no strip. The dialog shows the terminals
 * such a strip already holds, so that the user reads where the IDs he types
 * continue them (@sa AddTerminalStripDialog::fillExistingTerminals).
 * What the dialog describes is then put into the project: a terminal
 * strip, and the terminals of its ID field -- the terminals the strip
 * already holds are display in that dialog and are not described twice.
 *
 * The terminals are created as terminals which are not drawn in any folio
 * yet: they hold a label ('strip name:ID'), the symbol the user gave each
 * of them and the properties he chose for them in the dialog of the
 * marked terminals, but no element, which is why isElement() is the
 * marker the tree uses to tell a drawn terminal from a pending one.
 * Nothing needs an active folio here, the terminals get their folio when
 * their symbol is dropped on one.
 *
 * A name which already belongs to a strip of the node means that the
 * terminals are added to that strip rather than to a new one: same node,
 * same strip, the IDs are simply extended.
 *
 * Everything goes into a single macro, so that one undo takes the
 * terminals out again and leaves the tree the way it was. QUndoStack::
 * push() runs redo() straight away, so there is nothing left to do here.
 */
void TerminalNavigatorWidget::createTerminalStrip()
{
	if (!m_project) {
		return;
	}

	QScopedPointer<AddTerminalStripDialog> dialog(
				new AddTerminalStripDialog(m_project, this));
	dialog->setLocation(currentLocation());
	dialog->setInstallation(currentInstallation());
		//The name of the strip the dialog was called from, which is
		//what currentStrip() tells: the item the user was on is the
		//strip itself or one of its terminals, and the two setters
		//above had to come first -- they reload the names of the node
		//and empty the field with them (@sa fillNamesOfTheCurrentNode).
		//The dialog then shows what that strip already holds, ahead of
		//the IDs typed below (@sa fillExistingTerminals). Nothing to
		//pre-fill for an installation, a location or a free terminal:
		//those belong to no strip, and currentStrip() answers null.
	if (auto *strip = currentStrip()) {
		dialog->setName(strip->name());
	}

	if (dialog->exec() != QDialog::Accepted) {
		return;
	}

	auto *existing_strip = dialog->terminalStripOfTheDialog();
	auto *strip_ = existing_strip ? existing_strip : dialog->generatedTerminalStrip();
	if (!strip_) {
		return;
	}

	const auto described = dialog->describedTerminals();
	auto *macro = new QUndoCommand(described.isEmpty()
		? tr("Add a group of terminals")
		: tr("Add %n terminal(s) to the terminal strip %1", "", described.count())
			.arg(strip_->name()));

	if (!existing_strip) {
		new CreateTerminalStripCommand(strip_, m_project, this, macro);
	}

	if (!described.isEmpty())
	{
		QVector<QSharedPointer<RealTerminal>> terminals;
		const auto label_prefix = strip_->name() + QStringLiteral(":");
		for (const auto &terminal : described) {
				//The terminal is created with the label and the symbol
				//the dialog described it with, and the data it keeps for
				//itself are the ones the dialog gathered for it: type,
				//function, section, width, color and flags. Those are
				//what the terminal strip editor shows for such a
				//terminal, and what a drawn terminal carries over into
				//the element it is drawn with
				//(@sa TerminalProperties::applyTo,
				//BindTerminalCommand::takenOverData).
			auto real_terminal = RealTerminal::createPending(
						label_prefix + terminal.id, terminal.properties.symbol);
			auto data = real_terminal->spareData();
			terminal.properties.applyTo(data);
			real_terminal->setSpareData(data);
			terminals.append(real_terminal);
		}
		new AddTerminalToStripCommand(terminals, strip_, macro);
	}

	if (macro->childCount() == 0) {	//Neither a strip nor a terminal was described
		delete macro;
		return;
	}

	m_project->undoStack()->push(macro);

	if (existing_strip) {
		//The command of a new strip already selects it; an existing one
		//is only reloaded through orderChanged() and would keep whatever
		//item the tree happened to select.
		setSelectedStrip(strip_);
	}
}

/**
 * @brief TerminalNavigatorWidget::buildTree
 * Fill the tree with the terminal strips of the project, grouped by
 * installation and location, plus the free terminals.
 */
void TerminalNavigatorWidget::buildTree()
{
	if (!m_project) {
		return;
	}

	auto title_ = m_project->title();
	if (title_.isEmpty()) {
		title_ = tr("Untitled project");
	}

	QStringList strl{title_};
	new QTreeWidgetItem(ui->m_tree_view, strl, Root);

	QStringList ftstrl(tr("Independent terminals"));
	new QTreeWidgetItem(ui->m_tree_view, ftstrl, FreeTerminal);

	auto ts_vector = m_project->terminalStrip();
	std::sort(ts_vector.begin(), ts_vector.end(), [](TerminalStrip *a, TerminalStrip *b) {
		return QETUtils::naturalLessThan(a->name(), b->name());
	});

	for (const auto &ts : std::as_const(ts_vector)) {
		addTerminalStrip(ts);
	}
	addFreeTerminal();
}

QTreeWidgetItem* TerminalNavigatorWidget::addTerminalStrip(TerminalStrip *terminal_strip)
{
	if (auto item = m_item_strip_H.key(terminal_strip)) {
		return item;
	}

	auto root_item = ui->m_tree_view->topLevelItem(0);

		//Check if installation already exist
		//if not create a new one
	auto installation_str = terminal_strip->installation();
	QTreeWidgetItem *inst_qtwi = nullptr;
	for (int i = 0 ; i<root_item->childCount() ; ++i) {
		auto child_inst = root_item->child(i);
		if (child_inst->data(0, Qt::DisplayRole).toString() == installation_str) {
			inst_qtwi = child_inst;
			break;
		}
	}
	if (!inst_qtwi) {
		QStringList inst_strl{installation_str};
		inst_qtwi = new QTreeWidgetItem(root_item, inst_strl, Installation);
	}

		//Check if location already exist
		//if not create a new one
	auto location_str = terminal_strip->location();
	QTreeWidgetItem *loc_qtwi = nullptr;
	for (int i = 0 ; i<inst_qtwi->childCount() ; ++i) {
		auto child_loc = inst_qtwi->child(i);
		if (child_loc->data(0, Qt::DisplayRole).toString() == location_str) {
			loc_qtwi = child_loc;
			break;
		}
	}
	if (!loc_qtwi) {
		QStringList loc_strl{location_str};
		loc_qtwi = new QTreeWidgetItem(inst_qtwi, loc_strl, Location);
	}

		//Add the terminal strip
	QStringList name{terminal_strip->name()};
	auto strip_item = new QTreeWidgetItem(loc_qtwi, name, Strip);
	strip_item->setData(0, UUID_USER_ROLE, terminal_strip->uuid());
	strip_item->setIcon(0, QET::Icons::TerminalStrip);

		//Add child terminal of the strip
	for (auto i=0 ; i<terminal_strip->physicalTerminalCount() ; ++i)
	{
		auto phy_t = terminal_strip->physicalTerminal(i);
		if (phy_t->realTerminalCount())
		{
			QString text_;
			bool drawn_ = false;
			bool pending_ = false;
			for (const auto &real_t : phy_t->realTerminals())
			{
				if (text_.isEmpty())
					text_ = real_t->label();
				else
					text_.append(QStringLiteral(", ")).append(real_t->label());
					//Drawn means drawn on a folio, and the folio is
					//what says so: a symbol kept alive by the command
					//which took it off its folio -- deleting a
					//terminal "from the drawing only", or the folio
					//itself gone with its drawings -- is off every
					//folio, and its terminal is unbound from it
					//(@sa RealTerminal::unbindFromElement), so
					//isElement() alone would keep such a row drawn.
					//What waits after that drags again; a terminal an
					//older version left with no element and no
					//identity of its own says "not drawn" and drags
					//nothing (@sa isPending).
				drawn_ = drawn_ || (real_t->isElement() && real_t->element()->diagram());
				pending_ = pending_ || real_t->isPending();
			}
			auto terminal_item = new QTreeWidgetItem(strip_item, QStringList(text_), Terminal);
			terminal_item->setData(0, UUID_USER_ROLE, phy_t->uuid());
			if (drawn_) {
				terminal_item->setIcon(0, QET::Icons::ElementTerminal);
			} else {
					//The missing icon already tells the row is drawn
					//nowhere; the tooltip says it in words, the way the
					//terminal strip editor's table does ("Not drawn yet")
				terminal_item->setToolTip(0, tr("Not drawn in any folio yet"));
			}
				//Only a terminal which still waits for its symbol can be
				//dragged onto a folio -- one already drawn is where it
				//belongs, and the rest of the tree answers nothing to
				//pendingTerminalMimeForItem() either way: the flag only
				//keeps Qt from offering a drag which would lead nowhere.
			terminal_item->setFlags(terminal_item->flags() & ~Qt::ItemIsDragEnabled);
			if (pending_) {
				terminal_item->setFlags(terminal_item->flags() | Qt::ItemIsDragEnabled);
			}
		}
	}

	m_item_strip_H.insert(strip_item, terminal_strip);
	m_uuid_strip_H.insert(terminal_strip->uuid(), terminal_strip);

	m_strip_changed_connection.append(connect(terminal_strip, &TerminalStrip::orderChanged, this, &TerminalNavigatorWidget::reload));
	return strip_item;
}

/**
 * @brief TerminalNavigatorWidget::addFreeTerminal
 */
void TerminalNavigatorWidget::addFreeTerminal()
{
	ElementProvider ep(m_project);
	auto vector_ = ep.freeTerminal();

	if (vector_.isEmpty()) {
		return;
	}

		//Sort the terminal element by label
	std::sort(vector_.begin(), vector_.end(), [](TerminalElement *a, TerminalElement *b)
	{
		return QETUtils::naturalLessThan(
					a->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString(),
					b->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString());
	});

	auto free_terminal_item = ui->m_tree_view->topLevelItem(1);

	for (const auto terminal : std::as_const(vector_))
	{
		QUuid uuid_ = terminal->uuid();
		QStringList strl{terminal->actualLabel()};
		auto item = new QTreeWidgetItem(free_terminal_item, strl, Terminal);
		item->setData(0, UUID_USER_ROLE, uuid_);
		item->setIcon(0, QET::Icons::ElementTerminal);

		m_uuid_terminal_H.insert(uuid_, terminal->realTerminal());
			//The item holds the uuid of its element, and this answers
			//that element for the deletion of it: a free terminal has
			//no RealTerminal to ask (@sa deleteSelection)
		m_uuid_free_terminal_H.insert(uuid_, terminal);
	}
}

/**
 * @brief TerminalNavigatorWidget::pendingTerminalOfItem
 * Read the terminal a drag of @a item would carry: the strip the item
 * hangs below and the first terminal of it which waits for its symbol,
 * both by uuid -- the same question both drag payloads and the "Draw"
 * of the context menu ask, one item at a time
 * (@sa pendingTerminalMimeForItem, pendingTerminalsMimeForItems,
 * drawSelection).
 *
 * The item holds every level of one physical terminal: the first of
 * them which waits for its symbol is the one carried, so the next
 * drags of the same item take the next level.
 * @param item
 * @param strip_uuid : receives the uuid of the strip
 * @param terminal_uuid : receives the uuid of the terminal
 * @param real : receives the terminal itself, for the run the context
 * menu's "Draw" starts without a drag reading it back through uuids,
 * may be null
 * @return true when @a item holds a terminal to be drawn
 */
bool TerminalNavigatorWidget::pendingTerminalOfItem(QTreeWidgetItem *item,
													QUuid *strip_uuid,
													QUuid *terminal_uuid,
													QSharedPointer<RealTerminal> *real) const
{
	if (!item || !m_project) {
		return false;
	}
	if (item->type() != Terminal) {
		return false;
	}

		//The strip the item hangs below: walked up from the item, since
		//the tree holds the strip per strip item, not per terminal. A
		//free terminal hangs below its own root and has no strip, and
		//is drawn on a folio already anyway.
	QTreeWidgetItem *strip_item = item;
	while (strip_item && strip_item->type() != Strip) {
		strip_item = strip_item->parent();
	}
	auto *strip = strip_item ? m_item_strip_H.value(strip_item).data() : nullptr;
	if (!strip) {
		return false;
	}

	const auto phy_t = strip->physicalTerminal(item->data(0, UUID_USER_ROLE).toUuid());
	if (!phy_t) {
		return false;
	}
	for (const auto &real_t : phy_t->realTerminals()) {
		if (real_t->isPending()) {
			if (strip_uuid) {
				*strip_uuid = strip->uuid();
			}
			if (terminal_uuid) {
				*terminal_uuid = real_t->uuid();
			}
			if (real) {
				*real = real_t;
			}
			return true;
		}
	}

	return false;
}

/**
 * @brief TerminalNavigatorWidget::pendingTerminalMimeForItem
 * Fill @a mime with what a drag of @a item carries: the strip the item
 * hangs below and the terminal of it which waits for its symbol, both
 * by uuid. The drag reads them back through the project, which is where
 * both are known -- a terminal can not travel by pointer, the tree is
 * rebuilt by every reload and the strip editor undoes while the drag is
 * on its way.
 * @param item
 * @param mime
 * @return true when @a item holds a terminal to be drawn
 */
bool TerminalNavigatorWidget::pendingTerminalMimeForItem(QTreeWidgetItem *item, QMimeData *mime)
{
	if (!mime) {
		return false;
	}

	QUuid strip_, terminal_;
	if (!pendingTerminalOfItem(item, &strip_, &terminal_)) {
		return false;
	}

	mime->setData(DiagramEventAddTerminal::pendingTerminalMime(),
				  DiagramEventAddTerminal::pendingTerminalPayload(strip_, terminal_));
	return true;
}

/**
 * @brief TerminalNavigatorWidget::pendingTerminalsMimeForItems
 * Fill @a mime with what a drag of @a items carries: one line of
 * payload per item which holds a terminal waiting for its symbol, in
 * the order the items come in -- the order of the tree, which is the
 * order the run draws the terminals in.
 *
 * The items which hold no terminal to be drawn (an installation, a
 * strip, a free terminal, one already drawn) leave no line: a
 * selection of everything may still drag the terminals below it.
 * @param items
 * @param mime
 * @return true when at least one of @a items holds a terminal to be
 * drawn
 */
bool TerminalNavigatorWidget::pendingTerminalsMimeForItems(
		const QList<QTreeWidgetItem *> &items, QMimeData *mime)
{
	if (!mime || !m_project) {
		return false;
	}

	QVector<QPair<QUuid, QUuid>> carried;
	for (auto *item : items) {
		QUuid strip_, terminal_;
		if (pendingTerminalOfItem(item, &strip_, &terminal_)) {
			carried.append(qMakePair(strip_, terminal_));
		}
	}
	if (carried.isEmpty()) {
		return false;
	}

	mime->setData(DiagramEventAddTerminal::pendingTerminalMime(),
				  DiagramEventAddTerminal::pendingTerminalsPayload(carried));
	return true;
}

void TerminalNavigatorWidget::setCurrentStrip(TerminalStrip *strip)
{
	m_current_strip = strip;
	emit currentStripChanged(strip);
}

/**
 * @brief TerminalNavigatorWidget::deleteSelection
 * Take what the tree has selected off the project. One undo step
 * brings everything back: the symbols leave the folios they are drawn
 * on, the rows leave their strips, the strips leave the project -- the
 * navigator, the terminal block manager and the drawings all follow,
 * through the signals of the project and of the strips
 * (@sa TerminalStripDeletion, which builds the macros).
 *
 * What belongs to a strip goes as a strip knows it: a terminal row
 * with its symbols, or a whole branch (the project, an installation,
 * a location, a strip) with the strips it holds, their symbols and
 * their plans. What belongs to no strip -- the free terminals of the
 * second root, the independent ones -- goes the other way round:
 * there is no row to take out of anywhere, only the symbol leaves the
 * folio it is drawn on, wires included, which is what deleting the
 * same element in the drawing editor does
 * (@sa TerminalStripDeletion::freeTerminalsDeletionMacro).
 */
void TerminalNavigatorWidget::deleteSelection()
{
	if (!m_project) {
		return;
	}

	const auto items = ui->m_tree_view->selectedItemsInOrder();
	if (items.isEmpty()) {
		return;
	}

		//Collected first, asked after: a strip taken as a whole eats
		//the rows below it, and a row whose strip goes with it is one
		//deletion less to describe. The tree walks parents before
		//children, so a strip is known before its rows are looked at.
	QVector<TerminalStrip *> strips;
	QHash<TerminalStrip *, QVector<QUuid>> rows;
	QVector<Element *> free_elements;
	bool group_selected{false};

		//Collects every strip below @a item: the branch of the project,
		//an installation or a location is the strips it holds, all of
		//them -- and the rows below a strip go with the strip.
	const auto collect_strips = [&](auto &&self, QTreeWidgetItem *item) -> void {
		if (!item) {
			return;
		}
		if (item->type() == Strip) {
			if (auto *strip = m_item_strip_H.value(item).data();
				strip && !strips.contains(strip)) {
				strips << strip;
			}
			return;
		}
		for (int i = 0 ; i < item->childCount() ; ++i) {
			self(self, item->child(i));
		}
	};

		//The same for the free terminals below their root: every one
		//of them goes, its symbol off its folio. The element is read
		//back through the uuid the item holds, and a symbol which is
		//already off the folios (deleted elsewhere while the tree
		//still showed it) answers null and is skipped.
	const auto collect_free_terminals = [&](auto &&self, QTreeWidgetItem *item) -> void {
		if (!item) {
			return;
		}
		if (item->type() == Terminal) {
			if (auto *element = m_uuid_free_terminal_H.value(
						item->data(0, UUID_USER_ROLE).toUuid()).data();
				element && !free_elements.contains(element)) {
				free_elements << element;
			}
			return;
		}
		for (int i = 0 ; i < item->childCount() ; ++i) {
			self(self, item->child(i));
		}
	};

	for (auto *item : items)
	{
		switch (item->type())
		{
			case Root:
			case Installation:
			case Location:
				group_selected = true;
				collect_strips(collect_strips, item);
				break;
			case FreeTerminal:
					//The root of the free terminals: every one of them
					//goes at once, so it says so and asks like the
					//branch of the project above it does.
				group_selected = true;
				collect_free_terminals(collect_free_terminals, item);
				break;
			case Strip:
			{
				group_selected = true;
				if (auto *strip = m_item_strip_H.value(item).data();
					strip && !strips.contains(strip)) {
					strips << strip;
				}
				break;
			}
			case Terminal:
			{
				auto *parent_item = item->parent();
				if (!parent_item) {
					break;
				}
				if (parent_item->type() == FreeTerminal)
				{
						//A free terminal marked on its own: it goes
						//without asking, the way a row of a strip does.
					if (auto *element = m_uuid_free_terminal_H.value(
								item->data(0, UUID_USER_ROLE).toUuid()).data();
						element && !free_elements.contains(element)) {
						free_elements << element;
					}
					break;
				}
				if (parent_item->type() != Strip) {
					break;
				}
					//A row of a strip: the row goes, the strip stays.
				auto *strip = m_item_strip_H.value(parent_item).data();
				if (!strip || strips.contains(strip)) {
					break;		//Its strip goes as a whole: covered
				}
				const auto uuid_ = item->data(0, UUID_USER_ROLE).toUuid();
				auto &uuids = rows[strip];
				if (!uuids.contains(uuid_)) {
					uuids << uuid_;
				}
				break;
			}
			default:
				break;			//Nothing of an installation or location
		}
	}

	if (strips.isEmpty() && rows.isEmpty() && free_elements.isEmpty()) {
		return;					//Nothing here leaves the project
	}

		//The reason a deletion is refused, the way the drawing editor
		//has always refused the same terminals
		//(DeleteQGraphicsItemCommand::hasNonDeletableTerminal): a row
		//taken here would take the bridge it is part of with it, and a
		//multi-level row is deleted level by level in the strip editor.
		//Unbridge it and take its levels off first.
	const auto refuse = [this]() {
		QET::QetMessageBox::information(this,
										tr("Unable to delete the terminal"),
										tr("The deletion cannot be performed because the selection contains one or more "
										   "terminals that are bridged and/or belong to a multi-level terminal.\n"
										   "Unbridge and/or remove the levels from the affected terminals so that they "
										   "can be deleted"));
	};
	for (auto it = rows.cbegin() ; it != rows.cend() ; ++it)
	{
		for (const auto &uuid_ : it.value())
		{
			const auto phy_t = it.key()->physicalTerminal(uuid_);
			if (!phy_t) {
				continue;
			}
			if (phy_t->levelCount() != 1) {
				refuse();
				return;
			}
			for (const auto &real_t : phy_t->realTerminals()) {
				if (real_t->isBridged()) {
					refuse();
					return;
				}
			}
		}
	}

		//What the question says it takes: the rows marked on their
		//own plus the rows of the strips which go as a whole plus the
		//free terminals which go with them -- a strip eats the rows
		//below it before this point, and the question said
		//"0 terminal(s)" for a strip of four, which is what every one
		//of its symbols costs the drawings
		//(@sa confirmGroupDeletion). A free terminal costs its folio
		//a symbol, which is what its own line of the question says.
	int row_count{0};
	for (auto it = rows.cbegin() ; it != rows.cend() ; ++it) {
		row_count += it.value().count();
	}
	for (auto *strip : std::as_const(strips)) {
		row_count += strip->physicalTerminalCount();
	}
	row_count += free_elements.count();
	if (group_selected && !confirmGroupDeletion(strips.count(), row_count)) {
		return;
	}

	QVector<QPair<TerminalStrip *, QVector<QUuid>>> row_list;
	for (auto it = rows.cbegin() ; it != rows.cend() ; ++it) {
		row_list.append(qMakePair(it.key(), it.value()));
	}

		//Whatever the selection held goes in a single undo step:
		//the strips first, the rows of the strips which stay after
		//them, the free terminals last -- and one undo brings the
		//whole of it back. One kind alone does not need a macro of
		//its own: the command of it is the step.
	const int kinds = (strips.isEmpty() ? 0 : 1)
			+ (row_list.isEmpty() ? 0 : 1)
			+ (free_elements.isEmpty() ? 0 : 1);
	QUndoCommand *macro = nullptr;
	if (kinds > 1) {
		macro = new QUndoCommand(tr("Delete terminals"));
		if (!strips.isEmpty()) {
			TerminalStripDeletion::stripsDeletionMacro(m_project, strips, macro);
		}
		if (!row_list.isEmpty()) {
			TerminalStripDeletion::terminalsDeletionMacro(m_project, row_list, macro);
		}
		if (!free_elements.isEmpty()) {
			TerminalStripDeletion::freeTerminalsDeletionMacro(m_project, free_elements, macro);
		}
		if (macro->childCount() == 0) {
				//Nothing could be collected after all (symbols which
				//are off the folios already): no empty step on the
				//undo stack for that.
			delete macro;
			return;
		}
	} else if (!strips.isEmpty()) {
		macro = TerminalStripDeletion::stripsDeletionMacro(m_project, strips);
	} else if (!row_list.isEmpty()) {
		macro = TerminalStripDeletion::terminalsDeletionMacro(m_project, row_list);
	} else {
		macro = TerminalStripDeletion::freeTerminalsDeletionMacro(m_project, free_elements);
	}

	if (macro) {
			//The tree follows through the signals of the project and
			//of the strips: no reload to run here.
		m_project->undoStack()->push(macro);
	}
}

/**
 * @brief TerminalNavigatorWidget::confirmGroupDeletion
 * @param strip_count
 * @param terminal_count
 * @return the answer to the question, true being "delete it all"
 */
bool TerminalNavigatorWidget::confirmGroupDeletion(int strip_count, int terminal_count)
{
		//No strip at all goes when @p strip_count is 0: only the free
		//terminals of the second root are marked as a whole then, and
		//they take no plan and no row with them -- only their symbols
		//leave the folios, which is what this wording says instead of
		//the plans of the strips.
	QString question_;
	if (strip_count == 0) {
		question_ = tr("Delete %1 independent terminal(s) from the project?\n"
					   "Their symbols in the drawings are deleted too. "
					   "This can be undone.")
						.arg(terminal_count);
	} else {
		question_ = tr("Delete %1 terminal strip(s) and %2 terminal(s) from the project?\n"
					   "Their symbols and their plans in the drawings are deleted too. "
					   "This can be undone.")
						.arg(strip_count)
						.arg(terminal_count);
	}

	const auto answer = QMessageBox::question(this,
											  tr("Delete"),
											  question_,
											  QMessageBox::Yes | QMessageBox::No,
											  QMessageBox::No);
	return answer == QMessageBox::Yes;
}

/**
 * @brief TerminalNavigatorWidget::isInFreeTerminalBranch
 * @param item
 * @return true when @a item is the root of the independent terminals
 * or hangs below it
 */
bool TerminalNavigatorWidget::isInFreeTerminalBranch(QTreeWidgetItem *item) const
{
	for (auto *it = item ; it ; it = it->parent()) {
		if (it->type() == FreeTerminal) {
			return true;
		}
	}
	return false;
}

/**
 * @brief TerminalNavigatorWidget::selectionScopeKey
 * @param item
 * @return the item every mark of a legal selection has to be equal to
 * (@sa the class documentation), or nullptr for nothing at all
 *
 * The scope of a mark is what it may be marked beside: the root of the
 * independent terminals for anything of their side -- one key for the
 * whole of it, since a mark on one side takes the marks of the other
 * side away (@sa onSelectionChanged) --, the item of the strip for a
 * strip and for its rows, which are one scope together and the only
 * thing several items are marked for at a time, and the item itself
 * for everything else, which is worth a mark of its own only.
 */
QTreeWidgetItem *TerminalNavigatorWidget::selectionScopeKey(QTreeWidgetItem *item) const
{
	if (!item) {
		return nullptr;
	}

		//The whole side of the independent terminals, root included,
		//answers with the same item: their root.
	for (auto *it = item ; it ; it = it->parent()) {
		if (it->type() == FreeTerminal) {
			return it;
		}
	}

		//A strip and the rows below it: the strip is the one item
		//they all hang on, so a strip with its rows is a legal
		//selection and the rows of a second strip are not of it.
	if (item->type() == Strip) {
		return item;
	}
	if (item->type() == Terminal
		&& item->parent() && item->parent()->type() == Strip) {
		return item->parent();
	}

		//The project, an installation, a location: each of them is a
		//mark of its own, and the next mark beside it is not of it.
	return item;
}

/**
 * @brief TerminalNavigatorWidget::onSelectionChanged
 * Bring the marks of the tree down to what it allows, then tell the
 * buttons what is left of them.
 *
 * The why of the rules is told at the declaration (@sa
 * onSelectionChanged): one side of the tree at a time, one strip of
 * the project at most, everything else a mark of its own. What the
 * rules weigh is the marks which just came in -- minus the ones which
 * were accepted the last time (@sa m_marked_snapshot) --, since those
 * say which side, which strip, which item the user meant; marks which
 * only left cannot make a selection illegal, so they are taken for
 * what they are: marks which went away.
 *
 * The mark which just came in is normally also the current item --
 * Qt makes the clicked or the keyboard's item current --, and it is
 * weighed against itself only to be kept. Everything else in the
 * selection has to be of the same scope, or it goes.
 */
void TerminalNavigatorWidget::onSelectionChanged()
{
		//Taking marks away is a selection change of its own: the
		//second round must not weigh once more -- what it would find
		//is what this one decided.
	if (m_filtering_selection) {
		return;
	}

	auto marked = ui->m_tree_view->selectedItems();

	QList<QTreeWidgetItem *> added;
	for (auto *item : std::as_const(marked)) {
		if (!m_marked_snapshot.contains(item)) {
			added.append(item);
		}
	}

	if (!added.isEmpty())
	{
			//The item the new marks are weighed against: the one the
			//user clicked or the keyboard landed on, or the first of
			//the new ones when a whole range came in at once and the
			//current item is not part of it.
		auto *anchor = added.contains(ui->m_tree_view->currentItem())
				? ui->m_tree_view->currentItem() : added.constFirst();
		auto *key = selectionScopeKey(anchor);

		QList<QTreeWidgetItem *> kept;
		m_filtering_selection = true;
		for (auto *item : std::as_const(marked))
		{
			if (selectionScopeKey(item) == key) {
				kept.append(item);
			} else {
				item->setSelected(false);
			}
		}
		m_filtering_selection = false;

			//The weighing may have taken the current item away with
			//it: the current one goes to the first mark which stayed,
			//so that the tab never speaks for a mark which is gone
			//(@sa on_m_tree_view_currentItemChanged). NoUpdate keeps
			//this out of the marks themselves -- they are settled.
		if (key && !kept.contains(ui->m_tree_view->currentItem())) {
			ui->m_tree_view->setCurrentItem(kept.constFirst(), 0,
											QItemSelectionModel::NoUpdate);
		}
	}

	m_marked_snapshot = ui->m_tree_view->selectedItems();
	updateActionsState();
}

/**
 * @brief TerminalNavigatorWidget::updateActionsState
 * Say what the selection is worth to the three buttons which follow
 * it: the delete button of the toolbar is for a terminal row of a
 * strip, which goes out of its strip without asking, and for a free
 * terminal, which goes off its folio the same way -- and the "Draw"
 * entry of the context menu for a row which waits for its symbol --
 * strips and branches are deleted from the context menu and the
 * Delete key, which ask their question first, and a free terminal is
 * drawn nowhere, being drawn already.
 *
 * The "New terminal strip" button follows the side of the tree the
 * selection (or the current item) stands on: an independent terminal
 * belongs to no strip, and a strip created below that root would
 * stand in its branch for nothing -- the context menu says the same
 * by leaving the entry out (@sa buildContextMenu). The current item
 * counts because the context menu moves it without marking anything.
 */
void TerminalNavigatorWidget::updateActionsState()
{
	bool deletable{false};
	bool drawable{false};
	const auto items = ui->m_tree_view->selectedItems();

		//The side of the tree the button which creates a strip is for
		//(@sa updateActionsState): the current item counts on its
		//own, since the context menu leaves it behind on the item
		//under the cursor without marking anything.
	bool free_side{isInFreeTerminalBranch(ui->m_tree_view->currentItem())};
	for (auto *item : items) {
		if (isInFreeTerminalBranch(item)) {
			free_side = true;
			break;
		}
	}

	for (auto *item : items) {
		if (!drawable && pendingTerminalOfItem(item, nullptr, nullptr)) {
			drawable = true;
		}

			//A row of a strip, or a free terminal: the parent of the
			//item says which one it is -- the strip the row belongs
			//to, or the root the free terminals hang below. The free
			//terminal only counts while its element is still there:
			//it can be gone from the folios already, with the tree
			//not rebuilt yet (@sa m_uuid_free_terminal_H).
		if (item->type() == Terminal && item->parent())
		{
			const auto parent_type{item->parent()->type()};
			if (parent_type == Strip) {
				deletable = true;
			} else if (parent_type == FreeTerminal
					   && m_uuid_free_terminal_H.contains(
						   item->data(0, UUID_USER_ROLE).toUuid())) {
				deletable = true;
			}
		}
		if (deletable && drawable) {
			break;
		}
	}

	ui->m_add_strip->setEnabled(bool(m_project) && !free_side);
	ui->m_delete_selection->setEnabled(bool(m_project) && deletable);
	ui->m_draw_selection->setEnabled(bool(m_project) && drawable);
}

/**
 * @brief TerminalNavigatorWidget::drawSelection
 * Draw the terminals of the selection which wait for their symbol, the
 * way a drag of the same selection does: one run of placements on the
 * drawing the editor shows, one click per terminal, in the order of
 * the tree, which is the order the drag carries them in
 * (@sa DiagramEventAddTerminal, DiagramView::startTerminalPlacement).
 *
 * The tab lives inside the elements collection panel of a drawing
 * editor window, which is where the drawing is -- reached the same way
 * the strip editor reaches its editor window, by being a friend of
 * QETDiagramEditor for currentDiagramView().
 */
void TerminalNavigatorWidget::drawSelection()
{
	if (!m_project) {
		return;
	}

		//The terminals of the selection which wait for their symbol,
		//in the order the tree shows them -- an item which holds none
		//(a strip, an installation, a row already drawn) leaves
		//nothing behind, the way the drag does.
	QVector<QSharedPointer<RealTerminal>> pendings;
	for (auto *item : ui->m_tree_view->selectedItemsInOrder()) {
		QSharedPointer<RealTerminal> pending;
		if (pendingTerminalOfItem(item, nullptr, nullptr, &pending)) {
			pendings.append(pending);
		}
	}
	if (pendings.isEmpty()) {
		return;
	}

	auto *editor = QETApp::diagramEditorAncestorOf(this);
	DiagramView *view = editor ? editor->currentDiagramView() : nullptr;
	if (!view || view->diagram()->project() != m_project) {
		QET::QetMessageBox::information(this,
										tr("Nothing to draw on"),
										tr("The symbols of the selected terminals are placed on a "
										   "drawing of this project: open one first."));
		return;
	}

		//No drop point here, unlike the drag: the run starts in the
		//middle of the drawing, and the symbol follows the cursor
		//from there.
	view->startTerminalPlacement(pendings,
								 view->mapToScene(view->viewport()->rect().center()));
}

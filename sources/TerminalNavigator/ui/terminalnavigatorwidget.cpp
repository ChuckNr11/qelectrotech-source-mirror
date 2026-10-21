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

#include "../../elementprovider.h"
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
		//(@sa TerminalNavigatorTree::selectedItemsInOrder).
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
		//menu does (@sa deleteSelection).
	connect(ui->m_tree_view, &TerminalNavigatorTree::deleteRequested,
			this, &TerminalNavigatorWidget::deleteSelection);

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

	for (const auto &connection_ : std::as_const(m_strip_changed_connection)) {
		disconnect(connection_);
	}
	m_strip_changed_connection.clear();

	buildTree();

	ui->m_tree_view->expandRecursively(ui->m_tree_view->rootIndex());

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
 * @param current
 * @param previous
 */
void TerminalNavigatorWidget::on_m_tree_view_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous)

	if (!current) {
		m_current_is_free_terminal = false;
		setCurrentStrip(nullptr);
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
 * Right click offers the same creation as the toolbar button, but for the
 * branch under the cursor: the clicked item becomes the current one
 * first, so the dialog is prefilled with its plant and location.
 * @param pos
 */
void TerminalNavigatorWidget::on_m_tree_view_customContextMenuRequested(const QPoint &pos)
{
	if (!m_project) {
		return;
	}

	if (auto *item = ui->m_tree_view->itemAt(pos)) {
		ui->m_tree_view->setCurrentItem(item);
	}

	QMenu menu(this);
	menu.addAction(ui->m_add_strip);

		//Deleting is for what can leave the project from here: a strip
		//or a terminal row of one. The free terminals belong to no
		//strip, and deleteSelection() alone knows what each item of
		//the selection is worth -- the entry is only greyed out when
		//there is nothing selected at all.
	auto *delete_ = menu.addAction(QET::Icons::EditDelete, tr("Delete"));
	delete_->setEnabled(!ui->m_tree_view->selectedItems().isEmpty());
	connect(delete_, &QAction::triggered,
			this, &TerminalNavigatorWidget::deleteSelection);

	menu.exec(ui->m_tree_view->viewport()->mapToGlobal(pos));
}

/**
 * @brief TerminalNavigatorWidget::createTerminalStrip
 * Open the navigator's own strip creation dialog, prefilled with the plant
 * and the location of the current selection, and put what the dialog
 * describes into the project: a terminal strip, and the terminals of its
 * ID field.
 *
 * The terminals are created as terminals which are not drawn in any folio
 * yet: they hold a label ('strip name:ID') and the symbol the user gave
 * each of them in the dialog, but no element, which is why isElement() is
 * the marker the tree uses to tell a drawn terminal from a pending one.
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
			terminals.append(RealTerminal::createPending(
						label_prefix + terminal.id, terminal.symbol));
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
					//A terminal which is not drawn in any folio yet holds no
					//symbol, and the missing icon is what tells the two apart
				drawn_ = drawn_ || real_t->isElement();
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
	}
}

/**
 * @brief TerminalNavigatorWidget::pendingTerminalOfItem
 * Read the terminal a drag of @a item would carry: the strip the item
 * hangs below and the first terminal of it which waits for its symbol,
 * both by uuid -- the same question both drag payloads ask, one item
 * at a time (@sa pendingTerminalMimeForItem,
 * pendingTerminalsMimeForItems).
 *
 * The item holds every level of one physical terminal: the first of
 * them which waits for its symbol is the one carried, so the next
 * drags of the same item take the next level.
 * @param item
 * @param strip_uuid : receives the uuid of the strip
 * @param terminal_uuid : receives the uuid of the terminal
 * @return true when @a item holds a terminal to be drawn
 */
bool TerminalNavigatorWidget::pendingTerminalOfItem(QTreeWidgetItem *item,
													QUuid *strip_uuid,
													QUuid *terminal_uuid) const
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
 * Only what belongs to a strip can go from here: a terminal row with
 * its symbols, or a whole branch (the project, an installation, a
 * location, a strip) with the strips it holds, their symbols and their
 * plans. The free terminals are left alone -- they belong to no strip,
 * and a symbol taken off a folio is the drawing editor's business.
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
					//A row of a strip: the row goes, the strip stays.
					//A free terminal hangs below the second root and
					//belongs to no strip -- not deletable from here.
				auto *parent_item = item->parent();
				if (!parent_item || parent_item->type() != Strip) {
					break;
				}
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
				break;			//Nothing of the free terminals' root
		}
	}

	if (strips.isEmpty() && rows.isEmpty()) {
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

	int row_count{0};
	for (auto it = rows.cbegin() ; it != rows.cend() ; ++it) {
		row_count += it.value().count();
	}
	if (group_selected && !confirmGroupDeletion(strips.count(), row_count)) {
		return;
	}

	QVector<QPair<TerminalStrip *, QVector<QUuid>>> row_list;
	for (auto it = rows.cbegin() ; it != rows.cend() ; ++it) {
		row_list.append(qMakePair(it.key(), it.value()));
	}

	QUndoCommand *macro = nullptr;
	if (!strips.isEmpty() && !row_list.isEmpty()) {
			//Both kinds in one step: the strips first, the rows of
			//other strips after them
		macro = new QUndoCommand(tr("Delete terminals"));
		TerminalStripDeletion::stripsDeletionMacro(m_project, strips, macro);
		TerminalStripDeletion::terminalsDeletionMacro(m_project, row_list, macro);
	} else if (!strips.isEmpty()) {
		macro = TerminalStripDeletion::stripsDeletionMacro(m_project, strips);
	} else {
		macro = TerminalStripDeletion::terminalsDeletionMacro(m_project, row_list);
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
	const auto answer = QMessageBox::question(this,
											  tr("Delete"),
											  tr("Delete %1 terminal strip(s) and %2 terminal(s) from the project?\n"
												 "Their symbols and their plans in the drawings are deleted too. "
												 "This can be undone.")
											  .arg(strip_count)
											  .arg(terminal_count),
											  QMessageBox::Yes | QMessageBox::No,
											  QMessageBox::No);
	return answer == QMessageBox::Yes;
}

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
#include "../../qetproject.h"
#include "../../TerminalStrip/physicalterminal.h"
#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../../utils/qetutils.h"
#include "../UndoCommand/createterminalstripcommand.h"
#include "addterminalstripdialog.h"

#include <QMenu>
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
 * yet: they hold a label ('strip name:ID') and the symbol the user picked,
 * but no element, which is why isElement() is the marker the tree uses to
 * tell a drawn terminal from a pending one. Nothing needs an active folio
 * here, the terminals get their folio when their symbol is dropped on one.
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

	const auto ids = dialog->terminalIds();
	auto *macro = new QUndoCommand(ids.isEmpty()
		? tr("Add a group of terminals")
		: tr("Add %n terminal(s) to the terminal strip %1", "", ids.count())
			.arg(strip_->name()));

	if (!existing_strip) {
		new CreateTerminalStripCommand(strip_, m_project, this, macro);
	}

	if (!ids.isEmpty())
	{
		QVector<QSharedPointer<RealTerminal>> terminals;
		const auto label_prefix = strip_->name() + QStringLiteral(":");
		for (const auto &id : ids) {
			terminals.append(
						RealTerminal::createPending(label_prefix + id, dialog->symbolLocation()));
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
			for (const auto &real_t : phy_t->realTerminals())
			{
				if (text_.isEmpty())
					text_ = real_t->label();
				else
					text_.append(QStringLiteral(", ")).append(real_t->label());
					//A terminal which is not drawn in any folio yet holds no
					//symbol, and the missing icon is what tells the two apart
				drawn_ = drawn_ || real_t->isElement();
			}
			auto terminal_item = new QTreeWidgetItem(strip_item, QStringList(text_), Terminal);
			terminal_item->setData(0, UUID_USER_ROLE, phy_t->uuid());
			if (drawn_) {
				terminal_item->setIcon(0, QET::Icons::ElementTerminal);
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

void TerminalNavigatorWidget::setCurrentStrip(TerminalStrip *strip)
{
	m_current_strip = strip;
	emit currentStripChanged(strip);
}

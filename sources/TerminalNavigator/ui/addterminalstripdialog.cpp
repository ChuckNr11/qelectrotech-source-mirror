/*
	Copyright 2006-2026  The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file addterminalstripdialog.cpp
	@brief Implementation of the AddTerminalStripDialog class
*/
#include "addterminalstripdialog.h"
#include "ui_addterminalstripdialog.h"

#include "terminalsymbollist.h"

#include "../../ElementsCollection/elementslocation.h"
#include "../../TerminalStrip/physicalterminal.h"
#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../elementprovider.h"
#include "../../qet.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qeticons.h"
#include "../../qetproject.h"
#include "../../utils/qetutils.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextDocument>
#include <QTreeWidgetItem>

#include <algorithm>

/*
	Structures designations of a terminal strip, as used by this dialog:
	RootItem                  = Project
	   Plant                  = '=Inst' (installation)
	      Location            = '+Loc'  (location)
	         Klemmleistenname = Name     (one strip is made of many terminals)
	            :             = separator between strip name and ID
	            ID            = number of the terminal in the strip
*/
const QString AddTerminalStripDialog::s_installation_default = QStringLiteral("=INST");
const QString AddTerminalStripDialog::s_location_default     = QStringLiteral("+LOC");

namespace {
	/// Number of IDs a single list may expand to. A silly range such as
	/// 1-999999999 would otherwise freeze the dialog while it is expanded,
	/// for no use case anybody ever had.
	const int max_id_count = 1000;

	/// An ID is either a number (the position of the terminal in the strip)
	/// or a single letter, the letters being allowed because the labels of the
	/// terminals are not necessarily numbers.
	bool isNumberId(const QString &id)
	{
		if (id.isEmpty() || id.size() > 9) {
			return false; // int overflow guard
		}
		for (const auto c : id) {
			if (!c.isDigit()) {
				return false;
			}
		}
		return true;
	}

	bool isLetterId(const QString &id)
	{
		return id.size() == 1 && id.at(0).isLetter();
	}

	/// The ID of a terminal, as its label carries it: the label of a
	/// terminal is 'strip name:ID', so everything up to and including the
	/// last colon goes -- a label without a colon at all ("XPE") is an ID
	/// of its own already.
	QString idOfLabel(const QString &label)
	{
		return label.mid(label.lastIndexOf(QLatin1Char(':')) + 1);
	}
}

/**
	@brief AddTerminalStripDialog::AddTerminalStripDialog
	@param project : the project the terminal strip is created in, may be null
	(the dialog is then empty, which keeps it usable in tests and previews).
	@param parent
*/
AddTerminalStripDialog::AddTerminalStripDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AddTerminalStripDialog),
	m_project(project)
{
	ui->setupUi(this);
	ui->m_preview_tree->setHeaderLabels(
				{tr("ID"), tr("Symbol")});

		//The button of the preview toolbar works on marked terminals
		//only: just opened, the dialog marks none of them yet -- what
		//rows appear from here on mark themselves (@sa
		//onPreviewSelectionChanged).
	ui->m_edit_terminals->setEnabled(false);

		//The number of terminals to add takes whole numbers from 0 to 99,
		//zero being the value the dialog opens on: the very values the
		//.ui file sets on the spin box, set again here so that the dialog
		//and its code never disagree (@sa generatedIds).
	ui->m_number_sb->setRange(min_terminal_count, max_terminal_count);
	ui->m_number_sb->setValue(min_terminal_count);

		//Three lines of description, whatever the font and the screen: the
		//field is capped, so that it stops taking the room the preview
		//tree below it needs.
	const int line_spacing = ui->m_description_te->fontMetrics().lineSpacing();
	ui->m_description_te->setFixedHeight(
		line_spacing * 3
		+ ui->m_description_te->frameWidth() * 2
		+ qRound(ui->m_description_te->document()->documentMargin()) * 2);

		//The root of the preview tree is the terminal strip itself, its
		//children are the terminals of the ID field, @sa validate().
	m_root_item = new QTreeWidgetItem(ui->m_preview_tree, QStringList(QString()));

		//Both dialogs of this dialog share one list of the symbols of the
		//project and of the standard collection: this one names the
		//symbols its rows show with it, while the properties dialog shows
		//the list itself and starts reading the standard collection, which
		//holds more than a thousand elements and is read in the background
		//(@sa TerminalSymbolList). The symbols of the project are read by
		//the list itself, since its embedded collection lives in memory.
	m_symbols = new TerminalSymbolList(m_project, this);
	connect(m_symbols, &TerminalSymbolList::symbolsRead,
		this, &AddTerminalStripDialog::onSymbolListRead);

	fillInstallationAndLocation();

	connect(ui->m_installation_cb, &QComboBox::currentTextChanged,
		this, &AddTerminalStripDialog::fillNamesOfTheCurrentNode);
	connect(ui->m_location_cb, &QComboBox::currentTextChanged,
		this, &AddTerminalStripDialog::fillNamesOfTheCurrentNode);
	connect(ui->m_name_cb, &QComboBox::currentTextChanged,
		this, &AddTerminalStripDialog::validate);
	connect(ui->m_id_le, &QLineEdit::textChanged,
		this, &AddTerminalStripDialog::validate);
	connect(ui->m_number_sb, &QSpinBox::valueChanged,
		this, &AddTerminalStripDialog::validate);
		//One radio button says which of the two inputs names the
		//terminals to add: the number of them, or the IDs typed
		//(@sa onTerminalsModeChanged).
	connect(ui->m_number_rb, &QRadioButton::toggled,
		this, &AddTerminalStripDialog::onTerminalsModeChanged);
	connect(ui->m_id_rb, &QRadioButton::toggled,
		this, &AddTerminalStripDialog::onTerminalsModeChanged);
	connect(ui->m_preview_tree, &QTreeWidget::itemSelectionChanged,
		this, &AddTerminalStripDialog::onPreviewSelectionChanged);
	connect(ui->m_edit_terminals, &QAction::triggered,
		this, &AddTerminalStripDialog::onEditTerminalsTriggered);

		//The dialog opens on the number of terminals, its input being the
		//one which is active -- the radio button says so in the .ui file
		//already, the enabled state is the one the code owes it.
	ui->m_number_rb->setChecked(true);
	onTerminalsModeChanged(true);

	fillNamesOfTheCurrentNode();
	QET::trackDialogGeometry(this);
}

/**
	@brief AddTerminalStripDialog::~AddTerminalStripDialog
*/
AddTerminalStripDialog::~AddTerminalStripDialog()
{
	delete ui;
}

/**
	@brief AddTerminalStripDialog::setInstallation
	Pre-selects the plant (structure designation '=Inst') of the terminal strip
	to create, and reloads the strip names of that plant.
*/
void AddTerminalStripDialog::setInstallation(const QString &installation)
{
	ui->m_installation_cb->setCurrentText(installation);
	fillNamesOfTheCurrentNode();
}

/**
	@brief AddTerminalStripDialog::setLocation
	Pre-selects the location (structure designation '+Loc') of the terminal
	strip to create, and reloads the strip names of that location.
*/
void AddTerminalStripDialog::setLocation(const QString &location)
{
	ui->m_location_cb->setCurrentText(location);
	fillNamesOfTheCurrentNode();
}

/**
 * @brief AddTerminalStripDialog::setName
 * @param name : the strip name (structure designation 'Name') to pre-select
 * The name is set after setInstallation() and setLocation(), which is the
 * order the caller of this dialog has to keep: those two reload the names
 * of the node they select, and empty the name field with them
 * (@sa fillNamesOfTheCurrentNode).
 * The preview tree follows through validate(): a name which designates a
 * strip of the node already holding terminals shows them, ahead of the IDs
 * typed below (@sa fillExistingTerminals).
 */
void AddTerminalStripDialog::setName(const QString &name)
{
	if (name.isEmpty()) {
		return;
	}

		//Picked from the list when the name is one of it: setting the
		//text of an editable combo box only fills its line edit, which
		//reads exactly like a name typed by hand. The list of this
		//field holds the strips of the node -- which was just filled by
		//setInstallation()/setLocation() --, so the strip this dialog
		//is about is in it.
	const int index = ui->m_name_cb->findText(name);
	if (index >= 0) {
		ui->m_name_cb->setCurrentIndex(index);
	} else {
		ui->m_name_cb->setCurrentText(name);
	}

		//A combo box whose text did not really change says nothing, and
		//the two setters above have already validated a dialog without
		//this name: the preview is asked for here in any case.
	validate();
}

/**
	@brief AddTerminalStripDialog::generatedTerminalStrip
	Creates and returns the terminal strip described by this dialog, or null.
	The terminals themselves are not created here: the caller gets their IDs
	through terminalIds() and turns them into terminals of its own, since
	they are terminals which are not drawn in any folio yet.
	@see terminalStripOfTheDialog
*/
TerminalStrip *AddTerminalStripDialog::generatedTerminalStrip() const
{
	if (!m_project) {
		return nullptr;
	}

	auto *strip = m_project->newTerminalStrip(
		effectiveInstallation(),
		effectiveLocation(),
		effectiveName());
	if (!strip) {
		return nullptr;
	}
	strip->setComment(ui->m_comment_le->text());
	strip->setDescription(ui->m_description_te->toPlainText());

	return strip;
}

/**
	@brief AddTerminalStripDialog::terminalStripOfTheDialog
	@return the terminal strip of the project the name field currently
	designates, or null when there is none -- the caller then asks for a
	brand new strip through generatedTerminalStrip().
	A name picked from the list and a name typed by the user lead to the
	same terminal strip when they are the same: a name is never refused
	because it already exists, the terminals are simply added to it.
*/
TerminalStrip *AddTerminalStripDialog::terminalStripOfTheDialog() const
{
	return stripOfTheCurrentNode();
}

/**
	@brief AddTerminalStripDialog::describedTerminals
	@return the terminals this dialog describes: one per ID of the ID
	field, in the order of the preview tree, each with the properties the
	user gave it (@sa propertiesOfRow).
	The preview tree is read and not the ID field, because it is the only
	place which knows which terminals the user worked on, and because it is
	what the user looked at before pressing OK. The rows of the terminals
	the strip already holds are read out of it as well, being display only
	(@sa fillExistingTerminals): those exist already, and the OK button must
	not create them a second time.
	A list which can not be read at all leaves the tree on its last valid
	state, and the dialog refuses that state with its notice label, so the
	OK button is disabled before this could return something the user did
	not see.
*/
QList<AddTerminalStripDialog::TerminalDescription> AddTerminalStripDialog::describedTerminals() const
{
	QList<TerminalDescription> result;
	if (!m_root_item) {
		return result;
	}

	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		const auto *child = m_root_item->child(i);
			//The rows of the terminals the strip already holds are
			//display only (@sa fillExistingTerminals): they exist
			//already, and the OK button must not create them a second
			//time.
		if (child->data(0, ExistingTerminalRole).toBool()) {
			continue;
		}
		result.append({child->text(0), propertiesOfRow(child->text(0))});
	}

	return result;
}

/**
	@brief AddTerminalStripDialog::fillInstallationAndLocation
	Fills the plant and the location combo boxes with the values already used
	by the terminal strips of the project, the user of course being free to type
	another one (both combo boxes are editable).
*/
void AddTerminalStripDialog::fillInstallationAndLocation()
{
	if (!m_project) {
		return;
	}

	QSet<QString> installations, locations;
	for (const auto strip : m_project->terminalStrip()) {
		installations.insert(strip->installation());
		locations.insert(strip->location());
	}

	auto installation_list = installations.values();
	auto location_list = locations.values();
	std::sort(installation_list.begin(), installation_list.end(), QETUtils::naturalLessThan);
	std::sort(location_list.begin(), location_list.end(), QETUtils::naturalLessThan);

	ui->m_installation_cb->addItems(installation_list);
	ui->m_location_cb->addItems(location_list);
}

/**
	@brief AddTerminalStripDialog::fillNamesOfTheCurrentNode
	Fills the strip name combo box with the names of the terminal strips of the
	plant and the location currently selected, and empties that combo box: the
	name typed so far belongs to the node we just left.
	Whether the name ends up being one of those names or a name of his own
	changes nothing to the outcome: the terminals are added to the strip of
	that name, and a strip which does not exist yet is created.
*/
void AddTerminalStripDialog::fillNamesOfTheCurrentNode()
{
	QStringList names;
	if (m_project) {
		const auto installation = effectiveInstallation();
		const auto location = effectiveLocation();
		for (const auto strip : m_project->terminalStrip()) {
			if (strip->installation() == installation
			    && strip->location() == location) {
				names << strip->name();
			}
		}
		std::sort(names.begin(), names.end(), QETUtils::naturalLessThan);
	}

	ui->m_name_cb->clear();
	ui->m_name_cb->addItems(names);
	// An editable combo box selects its first item as soon as it gets one,
	// which would pre-fill the strip name with an existing one and, worse,
	// make that name look like a name picked from the list.
	ui->m_name_cb->setCurrentIndex(-1);

	validate();
}

/**
	@brief AddTerminalStripDialog::onTerminalsModeChanged
	@param checked : the radio button which just changed its state
	Enables the input the checked radio button names and disables the other
	one: @c m_number_sb with "Number of terminals", @c m_id_le with "ID's
	of terminals". The preview tree then shows what the active mode says,
	which is always what the OK button would create (@sa parseIds) -- the
	text typed into the ID field is kept as it is, so that switching back
	reads it again.
	The signal arrives twice for one switch, both radio buttons firing it:
	only the one which got checked has something to say.
*/
void AddTerminalStripDialog::onTerminalsModeChanged(bool checked)
{
	if (!checked) {
		return;
	}

	const bool ids_mode = ui->m_id_rb->isChecked();
	ui->m_id_le->setEnabled(ids_mode);
	ui->m_number_sb->setEnabled(!ids_mode);
	validate();
}

/**
	@brief AddTerminalStripDialog::validate
	Runs every check of the dialog and updates the preview tree, the notice
	label and the enable state of the OK button:
	- the ID list has to be readable and free of duplicates;
	- the IDs must not be used already in the node defined by the combo boxes;
	- the label 'strip name:ID' must not belong to an independent terminal.
	Nothing is checked about the name itself: a name which already belongs to
	a strip of the node designates that strip, whose terminals are extended
	instead of a new strip being created.
*/
void AddTerminalStripDialog::validate()
{
	QStringList messages;
	QStringList notes;
	const auto name = effectiveName();

		//The input naming the terminals is read even when the name is
		//empty: the preview tree follows it, and a list which can not be
		//read at all is worth telling about right away.
	const auto parsed = parseIds();
	if (!parsed.error.isEmpty()) {
		messages << parsed.error;
	} else if (!name.isEmpty()) {
		messages << checkIdsAgainstTheStrip(parsed.ids);
		messages << checkIdsAgainstFreeTerminals(parsed.ids);
	}

		//The generator stepped over IDs which were taken already: the
		//terminals go on where the free numbers are, which is worth
		//reading but not worth stopping for (@sa generatedIds).
	if (!parsed.skipped.isEmpty()) {
		notes << tr("IDs %1 are already used and were skipped")
				 .arg(parsed.skipped.join(QStringLiteral(", ")));
	}

		//The number of terminals at once is capped, and a spin box stops
		//growing without saying so: the cap itself is what the notice
		//label tells about, reached or attempted.
	if (ui->m_number_rb->isChecked()
	    && ui->m_number_sb->value() >= ui->m_number_sb->maximum()) {
		notes << tr("The number of terminals to add at once is limited to %1")
				 .arg(ui->m_number_sb->maximum());
	}

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
		!name.isEmpty() && messages.isEmpty());

		//An error is red, a hint is not: the notice label speaks of both,
		//and only the errors keep the OK button off.
	ui->m_notice_lb->setStyleSheet(
		messages.isEmpty() ? QString()
						  : QStringLiteral("QLabel {color: red;}"));
	ui->m_notice_lb->setText((messages + notes).join(QLatin1Char('\n')));
	ui->m_notice_lb->setVisible(!messages.isEmpty() || !notes.isEmpty());

	refreshPreviewTree(parsed);
}

/**
	@brief AddTerminalStripDialog::effectiveInstallation
	@return the plant of the terminal block to create, the default one being
	used when the user left the field empty.
*/
QString AddTerminalStripDialog::effectiveInstallation() const
{
	const auto installation = ui->m_installation_cb->currentText().trimmed();
	return installation.isEmpty() ? s_installation_default : installation;
}

/**
	@brief AddTerminalStripDialog::effectiveLocation
	@return the location of the terminal block to create, the default one being
	used when the user left the field empty.
*/
QString AddTerminalStripDialog::effectiveLocation() const
{
	const auto location = ui->m_location_cb->currentText().trimmed();
	return location.isEmpty() ? s_location_default : location;
}

/**
	@brief AddTerminalStripDialog::effectiveName
	@return the (trimmed) name of the terminal block to create.
*/
QString AddTerminalStripDialog::effectiveName() const
{
	return ui->m_name_cb->currentText().trimmed();
}

/**
	@brief AddTerminalStripDialog::fillExistingTerminals
	Shows what the strip of the current node already holds: one greyed out
	row per physical terminal of it which really holds terminals, at the top
	of the preview tree and ahead of the IDs typed below them -- the user
	reads what is there before he types the ID which continues it. The row
	shows the ID alone, the strip itself being the root item above it, and
	the name of the symbol the terminal is drawn with in the second column,
	empty when it is drawn with none -- the very reading the terminals typed
	below get (@sa symbolName). A drawn terminal keeps no symbol of its own,
	its element being the one to ask (@sa RealTerminal::symbol).
	Nothing to show when the name designates a strip which does not exist
	yet, or one which holds no terminal at all.

	Those rows carry ExistingTerminalRole, are not selectable and are left
	out of describedTerminals(): they are the display of what exists, never
	one of the terminals the OK button creates, and no symbol of the combo
	box ever reaches them -- a terminal drawn in a folio keeps the symbol it
	was drawn with (@sa selectedPreviewChildren).

	The rows are only rebuilt when they really say something else than what
	the tree shows: this runs on every keystroke in the ID field, and
	rebuilding them there would throw the list below them around for
	nothing.
*/
void AddTerminalStripDialog::fillExistingTerminals()
{
	if (!m_root_item) {
		return;
	}

		//The rows as they should be: one per physical terminal of the
		//strip which really holds terminals, in the order of the strip
		//-- the very order the navigator gives the items of a strip
		//(@sa checkIdsAgainstTheStrip).
	QStringList ids, symbol_names;
	if (auto *strip = stripOfTheCurrentNode())
	{
			//The ID only: the root item above those rows already says
			//which strip they belong to, and the ID is what the field
			//below them asks for (@sa idOfLabel).
		for (int i = 0 ; i < strip->physicalTerminalCount() ; ++i)
		{
			const auto physical_terminal = strip->physicalTerminal(i);
			if (!physical_terminal || physical_terminal->realTerminalCount() == 0) {
				continue;
			}

			QStringList terminal_ids;
			QStringList terminal_symbols;
			for (const auto &real_terminal : physical_terminal->realTerminals()) {
				terminal_ids << idOfLabel(real_terminal->label());
					//The name of the symbol the terminal is drawn
					//with, the very way the terminals typed get
					//theirs -- and nothing at all when it is drawn
					//with none (@sa symbolName). A drawn terminal
					//keeps no symbol string of its own: its symbol
					//is the one of its element, which is read out
					//of it the same way the symbol of a terminal
					//to be drawn is read out of its data (@sa
					//RealTerminal::unbindFromElement, which reads
					//it that way before letting the element go).
				auto symbol = real_terminal->symbol();
				if (symbol.isEmpty()
					&& real_terminal->isElement()
					&& real_terminal->element())
				{
					symbol = real_terminal->element()->location().toString();
				}
				const auto name = symbolName(symbol);
				if (!name.isEmpty() && !terminal_symbols.contains(name)) {
					terminal_symbols << name;
				}
			}
			ids << terminal_ids.join(QStringLiteral(", "));
			symbol_names << terminal_symbols.join(QStringLiteral(", "));
		}
	}

		//What the tree shows of them by now: the rows carrying the
		//marker, in the order they are shown in.
	QStringList shown_ids, shown_symbols;
	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		const auto *child = m_root_item->child(i);
		if (child->data(0, ExistingTerminalRole).toBool()) {
			shown_ids << child->text(0);
			shown_symbols << child->text(1);
		}
	}
	if (ids == shown_ids && symbol_names == shown_symbols) {
		return;
	}

		//They are replaced: only those rows are taken away, the IDs
		//typed so far stay where they are -- with their symbols and
		//their selection, which the rebuild below keeps.
	for (int i = m_root_item->childCount() - 1 ; i >= 0 ; --i) {
		if (m_root_item->child(i)->data(0, ExistingTerminalRole).toBool()) {
			delete m_root_item->child(i);
		}
	}
	if (ids.isEmpty()) {
		return;
	}

		//Everything left is an ID typed by the user: the rows of what
		//exists go in front of them.
	for (int i = 0 ; i < ids.count() ; ++i)
	{
		auto *child = new QTreeWidgetItem(QStringList(ids.at(i)));
		child->setText(1, symbol_names.at(i));
		child->setData(0, ExistingTerminalRole, true);
			//Display only: without flags the row is greyed out and
			//kept out of every selection, so that the symbol combo
			//box never changes a terminal which is drawn already
		child->setFlags(Qt::NoItemFlags);
		m_root_item->insertChild(i, child);
	}
}

/**
	@brief AddTerminalStripDialog::refreshPreviewTree
	Shows what the OK button will create: the strip as the root item of the
	preview tree, one child per ID of the ID field, the ID in the first
	column and the name of its symbol in the second one.
	The terminals the strip of the current node already holds come first,
	whatever the ID field says (@sa fillExistingTerminals): they are not
	one of the IDs below them, and are left out of every reading of it.
	The children are rebuilt only when the ID list really changed, since a
	rebuild throws away the selection the user is looking at; a list which
	can not be read at all leaves the last known children alone. What a
	rebuild adds is marked right away -- the terminals the dialog is about
	to create are the ones its buttons work on -- while a row which was
	there before keeps what the user did with it
	(@sa onPreviewSelectionChanged).
	A terminal the user gave properties keeps them across such a rebuild --
	they are kept by ID and not by row (@sa m_properties), so that retyping
	an ID list never silently gives the terminals other values than the ones
	the second column shows. What such a terminal was never given keeps
	following the defaults of the moment, since which strip the dialog is
	about can still change (@sa propertiesOfRow).
*/
void AddTerminalStripDialog::refreshPreviewTree(const ParsedIds &parsed)
{
	if (!m_root_item) {
		return;
	}

	m_root_item->setText(0, effectiveName());
	m_root_item->setIcon(0, QET::Icons::TerminalStrip);
	m_root_item->setFirstColumnSpanned(true);

	fillExistingTerminals();

	if (!parsed.error.isEmpty()) {
		return;
	}

		//The values the terminals which were never worked on are worth
		//come from the strip the dialog is about, and are read once here:
		//every row below asks for them, and there may be many
		//@sa defaultsForNewTerminals.
	const auto defaults = defaultsForNewTerminals();

	QStringList current_ids;
	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		const auto *child = m_root_item->child(i);
			//Only the IDs typed so far are compared, never the rows
			//of what the strip already holds (@sa
			//fillExistingTerminals).
		if (child->data(0, ExistingTerminalRole).toBool()) {
			continue;
		}
		current_ids << child->text(0);
	}

	if (current_ids != parsed.ids) {
		QStringList selected_ids;
		for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
			const auto *child = m_root_item->child(i);
			if (child->data(0, ExistingTerminalRole).toBool()) {
				continue;
			}
			if (child->isSelected()) {
				selected_ids << child->text(0);
			}
		}

		for (int i = m_root_item->childCount() - 1 ; i >= 0 ; --i) {
			if (!m_root_item->child(i)->data(0, ExistingTerminalRole).toBool()) {
				delete m_root_item->child(i);
			}
		}
		for (const auto &id : parsed.ids) {
			//No icon: those terminals are not drawn in any folio yet,
				//and the missing symbol is how the trees tell them apart
			auto *child = new QTreeWidgetItem(m_root_item, QStringList(id));
			const auto symbol = m_properties.value(id, defaults).symbol;
			child->setData(0, Qt::UserRole, symbol);
			child->setText(1, symbolName(symbol));
				//A terminal the dialog just added is marked the moment
				//its row appears, so that the button of the toolbar has
				//something to work on; one which was there before keeps
				//what the user did with it -- which is what current_ids
				//is for, the properties of a terminal the user worked on
				//being kept by ID all the same (@sa m_properties,
				//onPreviewSelectionChanged).
			if (!current_ids.contains(id)) {
				child->setSelected(true);
			}
		}

		for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
			if (selected_ids.contains(m_root_item->child(i)->text(0))) {
				m_root_item->child(i)->setSelected(true);
			}
		}
	}

		//What a terminal the user never worked on is worth follows the
		//strip the dialog is about, and the name field changes that
		//without the ID list changing at all: every row is asked for its
		//symbol again, and only the rows which really show something else
		//are written -- this runs on every keystroke of the ID field
		//(@sa propertiesOfRow).
	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		auto *child = m_root_item->child(i);
		if (child->data(0, ExistingTerminalRole).toBool()) {
			continue;
		}
		const auto symbol = m_properties.value(child->text(0), defaults).symbol;
		if (child->data(0, Qt::UserRole).toString() != symbol) {
			child->setData(0, Qt::UserRole, symbol);
			child->setText(1, symbolName(symbol));
		}
	}

	ui->m_preview_tree->expandItem(m_root_item);
}

/**
	@brief AddTerminalStripDialog::onSymbolListRead
	The standard collection just added its symbols to the list, which is
	what names them: the second column of a row shows the name of its
	symbol, and one which could not be read before it was listed keeps an
	empty column until now (@sa symbolName). Everything the preview tree
	shows is asked for again, the rows of what the strip already holds
	being read out of the strip itself (@sa validate).
*/
void AddTerminalStripDialog::onSymbolListRead()
{
	validate();
}

/**
	@brief AddTerminalStripDialog::onPreviewSelectionChanged
	The button of the toolbar above the tree follows the selection of the
	preview tree: it has something to work on only while terminals are
	marked (@sa onEditTerminalsTriggered).
*/
void AddTerminalStripDialog::onPreviewSelectionChanged()
{
	ui->m_edit_terminals->setEnabled(!selectedPreviewChildren().isEmpty());
}

/**
	@brief AddTerminalStripDialog::onEditTerminalsTriggered
	Opens the dialog for the terminals marked in the preview tree
	(@sa openTerminalsDialog). The button of the toolbar is only enabled
	while such terminals are marked, and the guard here is the very rule
	read once more: no dialog with nothing to work on.
*/
void AddTerminalStripDialog::onEditTerminalsTriggered()
{
	if (selectedPreviewChildren().isEmpty()) {
		return;
	}

	openTerminalsDialog();
}

/**
	@brief AddTerminalStripDialog::openTerminalsDialog
	Opens the dialog of the properties (@sa TerminalPropertiesDialog) for
	the terminals marked in the preview tree (@sa selectedPreviewChildren)
	and writes what it chose into those rows -- and only into those: the
	rows of terminals the strip already holds are display only and can not
	be marked at all (@sa fillExistingTerminals), and the properties of a
	terminal which was never worked on keep being the defaults of the
	moment (@sa propertiesOfRow).
	The dialog opens on the properties of the first marked terminal which
	has some -- the dialog gives every marked terminal one and the same set
	of values, which is the very point of marking many of them at once --,
	or on the defaults when none of them was worked on before.
*/
void AddTerminalStripDialog::openTerminalsDialog()
{
	const auto selected = selectedPreviewChildren();
	if (selected.isEmpty()) {
		return;
	}

	QStringList ids;
	for (const auto *item : selected) {
		ids << item->text(0);
	}

	TerminalPropertiesDialog dialog(
				m_symbols,
				m_properties.value(ids.first(), defaultsForNewTerminals()),
				TerminalPropertiesDialog::ShowSymbol,
				this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const auto properties = dialog.properties();
	for (const auto &id : ids) {
		m_properties.insert(id, properties);
	}
		//The rows follow right away: the second column shows the symbol
		//of every terminal, and the user trusts what he reads there
		//@sa symbolName.
	for (auto *item : selected) {
		item->setData(0, Qt::UserRole, properties.symbol);
		item->setText(1, symbolName(properties.symbol));
	}
}

/**
	@brief AddTerminalStripDialog::selectedPreviewChildren
	@return the children of the preview tree the user selected, which are
	the terminals the next symbol applies to.
*/
QList<QTreeWidgetItem *> AddTerminalStripDialog::selectedPreviewChildren() const
{
	QList<QTreeWidgetItem *> result;
	if (!m_root_item) {
		return result;
	}

	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		auto *child = m_root_item->child(i);
			//The rows of the terminals the strip already holds carry no
			//flags and can not be selected at all; they are skipped
			//here as well, so that no symbol ever reaches them
			//whatever the tree is asked (@sa fillExistingTerminals).
		if (child->isSelected()
			&& !child->data(0, ExistingTerminalRole).toBool()) {
			result << child;
		}
	}

	return result;
}

/**
	@brief AddTerminalStripDialog::symbolName
	@param symbol : the location string of a symbol
	@return the name to show for that symbol in the second column of the
	preview tree: the one the list of symbols gives it, since an element
	name is read out of its XML and is worth reading once only -- which is
	what the list keeps it for (@sa TerminalSymbolList).
*/
QString AddTerminalStripDialog::symbolName(const QString &symbol) const
{
	return m_symbols ? m_symbols->name(symbol) : QString();
}

/**
	@brief AddTerminalStripDialog::defaultsForNewTerminals
	@return the properties a terminal this dialog is never asked about is
	created with (@sa TerminalPropertiesDialog).
	Those are the very values the dialog of the marked terminals opens on,
	which is what keeps the two in step: what the user reads there is what
	the terminals get, whether he presses OK there or not. The symbol, the
	sizes and the color come from the last terminal the strip of the
	current node holds, since a value a user picked once is the one he is
	the most likely to want again; the type and the function stay generic
	and the flags stay off, since those are the values he chooses for every
	single terminal himself.
	Nothing is guessed for a strip which does not exist yet: it has no
	terminal to take a habit from, and its terminals are created with the
	generic values alone.
*/
TerminalProperties AddTerminalStripDialog::defaultsForNewTerminals() const
{
	TerminalProperties properties;
	auto *strip = stripOfTheCurrentNode();
	if (!strip) {
		return properties;
	}

		//The last terminal which holds one: the rows of the preview tree
		//are in the very order of the strip (@sa fillExistingTerminals),
		//so this is the row at the bottom of them.
	for (int i = strip->physicalTerminalCount() - 1 ; i >= 0 ; --i)
	{
		const auto physical_terminal = strip->physicalTerminal(i);
		if (!physical_terminal || physical_terminal->realTerminalCount() == 0) {
			continue;
		}

		const auto real_terminal = physical_terminal->realTerminal(
					physical_terminal->realTerminalCount() - 1);
		if (real_terminal.isNull()) {
			continue;
		}

		auto last = TerminalProperties::fromRealTerminal(real_terminal.data());
			//Only the values the dialog shows as the values of the last
			//terminal: the type and the function are the generic ones and
			//the two flags are off, as they are for a terminal the user
			//never worked on. The lock of the automatic numbering is the
			//one thing which is on: it is checked whenever the dialog
			//opens, and what the dialog shows is what those terminals get
			//(@sa TerminalProperties::auto_num_locked).
		last.type = ElementData::TTGeneric;
		last.function = ElementData::TFGeneric;
		last.exclude_from_bom = false;
		last.auto_num_locked = true;
		last.potential_isolating = false;
		return last;
	}

	return properties;
}

/**
	@brief AddTerminalStripDialog::propertiesOfRow
	@param id : the ID of a terminal to be created
	@return the properties of that terminal (@sa TerminalProperties).
	A terminal the user worked on keeps what the dialog of the marked
	terminals gave it (@sa m_properties); one he never worked on keeps the
	defaults of the moment, which follow the strip the dialog is about
	(@sa defaultsForNewTerminals).
*/
TerminalProperties AddTerminalStripDialog::propertiesOfRow(const QString &id) const
{
	return m_properties.value(id, defaultsForNewTerminals());
}

/**
	@brief AddTerminalStripDialog::stripOfTheCurrentNode
	@return the terminal strip of the project whose plant, location and name
	match the ones currently shown in this dialog, or null if there is none.
*/
TerminalStrip *AddTerminalStripDialog::stripOfTheCurrentNode() const
{
	if (!m_project) {
		return nullptr;
	}

	const auto installation = effectiveInstallation();
	const auto location = effectiveLocation();
	const auto name = effectiveName();
	if (name.isEmpty()) {
		return nullptr;
	}

	for (auto strip : m_project->terminalStrip()) {
		if (strip->installation() == installation
		    && strip->location() == location
		    && strip->name() == name) {
			return strip;
		}
	}

	return nullptr;
}

/**
	@brief AddTerminalStripDialog::parseIds
	Reads the ID field and expands its ranges: '1,2,3,4', '1-4' and '1,2-3,4'
	all lead to the four IDs 1, 2, 3 and 4. A descending range such as '4-1' is
	expanded in descending order, and the IDs are normalized, so that '007' and
	'7' are the same ID.
	@return the expanded IDs, or an error message when the field can not be read
	at all. An empty field is not an error: it simply means that no terminal
	will be added to the strip.
	Only IDs typed by hand are read here: as soon as the radio button of the
	number of terminals is the checked one, the IDs are generated instead of
	typed, and generatedIds() answers in this one's stead.
*/
AddTerminalStripDialog::ParsedIds AddTerminalStripDialog::parseIds() const
{
	if (ui->m_number_rb->isChecked()) {
		return generatedIds();
	}

	ParsedIds result;
	const auto text = ui->m_id_le->text().trimmed();
	if (text.isEmpty()) {
		return result;
	}

	const auto invalid = [&text, &result]() {
		result.ids.clear();
		result.error = tr("Invalid ID list: %1").arg(text);
	};

	for (const auto &raw_token : text.split(QLatin1Char(','), Qt::KeepEmptyParts)) {
		const auto token = raw_token.trimmed();
		const auto hyphens = token.count(QLatin1Char('-'));

		if (token.isEmpty() || hyphens > 1) {
			invalid();
			return result;
		}

		if (hyphens == 0) {
			if (isNumberId(token)) {
				result.ids << QString::number(token.toInt());
			} else if (isLetterId(token)) {
				result.ids << token;
			} else {
				invalid();
				return result;
			}
			continue;
		}

		// A range, such as 1-4, 4-1 or A-D.
		const auto bounds = token.split(QLatin1Char('-'));
		const auto first = bounds.value(0).trimmed();
		const auto last = bounds.value(1).trimmed();
		if (bounds.size() != 2 || first.isEmpty() || last.isEmpty()) {
			invalid();
			return result;
		}

		if (isNumberId(first) && isNumberId(last)) {
			int from = first.toInt();
			int to = last.toInt();
			const int step = from <= to ? 1 : -1;
			if (qAbs(from - to) >= max_id_count) {
				invalid();
				return result;
			}
			for (int id = from; ; id += step) {
				result.ids << QString::number(id);
				if (id == to) {
					break;
				}
			}
		} else if (isLetterId(first) && isLetterId(last)
		           && first.at(0).isUpper() == last.at(0).isUpper()) {
			int from = first.at(0).unicode();
			int to = last.at(0).unicode();
			const int step = from <= to ? 1 : -1;
			if (qAbs(from - to) >= max_id_count) {
				invalid();
				return result;
			}
			for (int id = from; ; id += step) {
				result.ids << QChar::fromLatin1(static_cast<char>(id));
				if (id == to) {
					break;
				}
			}
		} else {
			invalid();
			return result;
		}
	}

	QStringList already_used;
	for (const auto &id : qAsConst(result.ids)) {
		if (already_used.contains(id)) {
				//The message has to be built before the list is
				//emptied: @c id is a reference into it.
			result.error = tr("ID %1 is entered twice").arg(id);
			result.ids.clear();
			return result;
		}
		already_used << id;
	}

	return result;
}

/**
	@brief AddTerminalStripDialog::generatedIds
	@return as many IDs as @c m_number_sb asks for, continuing the IDs of the
	strip behind the terminals it holds already. The first one is the number
	right behind the last terminal and the highest ID among them alike -- a
	terminal block holding the IDs 2, 3, 5 and 7 goes on with 8, never with
	5 -- and an ID which is taken already (@sa takenIds) is stepped over and
	kept in ParsedIds::skipped, which is worth a hint but is no error: the
	terminals simply go on where the free numbers are.
	Zero terminals is no error either: the strip is then created holding no
	terminal at all, exactly what an empty ID field asks for.
*/
AddTerminalStripDialog::ParsedIds AddTerminalStripDialog::generatedIds() const
{
	ParsedIds result;
	const int count = ui->m_number_sb->value();
	if (count <= 0) {
		return result;
	}

		//Where the IDs continue: behind the rows the strip shows, counted
		//the way checkIdsAgainstTheStrip() counts them -- one per physical
		//terminal which really holds terminals, and a number up to that
		//count being the position of a terminal, which is taken already.
	int existing_rows = 0;
	int highest_id = 0;
	if (const auto strip = stripOfTheCurrentNode()) {
		for (int i = 0 ; i < strip->physicalTerminalCount() ; ++i)
		{
			const auto physical_terminal = strip->physicalTerminal(i);
			if (!physical_terminal || physical_terminal->realTerminalCount() == 0) {
				continue;
			}
			++existing_rows;
			for (const auto &real_terminal : physical_terminal->realTerminals()) {
				bool is_number = false;
				const int id = idOfLabel(real_terminal->label()).toInt(&is_number);
				if (is_number) {
					highest_id = qMax(highest_id, id);
				}
			}
		}
	}

	const auto taken = takenIds();
	int candidate = qMax(existing_rows, highest_id) + 1;
	while (result.ids.count() < count
	       && result.ids.count() + result.skipped.count() < max_id_count)
	{
		const auto id = QString::number(candidate);
		if (taken.contains(id)) {
			result.skipped << id;
		} else {
			result.ids << id;
		}
		++candidate;
	}

	return result;
}

/**
	@brief AddTerminalStripDialog::takenIds
	@return every ID which is taken already for the terminal block the combo
	boxes define: the IDs the terminals of that strip carry,
	and the ones a free terminal of the project claims for that very name.
	Those are the two cases checkIdsAgainstTheStrip() and
	checkIdsAgainstFreeTerminals() raise as errors for IDs typed by hand; for
	IDs generated they are stepped over instead
	(@sa generatedIds).
*/
QStringList AddTerminalStripDialog::takenIds() const
{
	QStringList taken;
	const auto name = effectiveName();
	if (const auto strip = stripOfTheCurrentNode()) {
		for (int i = 0 ; i < strip->physicalTerminalCount() ; ++i)
		{
			const auto physical_terminal = strip->physicalTerminal(i);
			if (!physical_terminal) {
				continue;
			}
			for (const auto &real_terminal : physical_terminal->realTerminals()) {
				taken << idOfLabel(real_terminal->label());
			}
		}
	}

	if (m_project && !name.isEmpty()) {
		const auto prefix = name + QLatin1Char(':');
		const auto free_terminals = ElementProvider(m_project).freeTerminal();
		for (auto terminal : free_terminals) {
			const auto label = terminal->actualLabel();
			if (label.startsWith(prefix)) {
				taken << idOfLabel(label);
			}
		}
	}

	taken.removeDuplicates();
	return taken;
}

/**
	@brief AddTerminalStripDialog::checkIdsAgainstTheStrip
	@param ids : the expanded IDs of the ID field
	@return one message per ID already used by the terminal block of the node
	currently defined by the combo boxes, or nothing.
	The ID of a terminal is its position in the strip node, so a terminal block
	already holding four terminals refuses the IDs 1 to 4; the labels of the
	child items are checked as well, which also catches the IDs made of a letter.
*/
QStringList AddTerminalStripDialog::checkIdsAgainstTheStrip(const QStringList &ids) const
{
	QStringList messages;
	const auto strip = stripOfTheCurrentNode();
	if (ids.isEmpty() || !strip) {
		return messages;
	}

	// Child items of the strip node, in the same order and with the same
	// labels as TerminalNavigatorWidget gives them: one item per physical
	// terminal that really holds terminals.
	QStringList child_items;
	for (int i = 0 ; i < strip->physicalTerminalCount() ; ++i) {
		const auto physical_terminal = strip->physicalTerminal(i);
		if (!physical_terminal || physical_terminal->realTerminalCount() == 0) {
			continue;
		}
		QStringList labels;
		for (const auto &real_terminal : physical_terminal->realTerminals()) {
			labels << real_terminal->label();
		}
		child_items << labels.join(QStringLiteral(", "));
	}

	const auto name = effectiveName();
	for (const auto &id : ids) {
		bool already_used = false;
		if (isNumberId(id)) {
			const int position = id.toInt();
			already_used = position >= 1 && position <= child_items.size();
		}
		if (!already_used) {
			already_used = child_items.contains(name + QStringLiteral(":") + id);
		}
		if (already_used) {
			messages << tr("ID %1 already exists in terminal block %2")
				.arg(id, name);
		}
	}

	return messages;
}

/**
	@brief AddTerminalStripDialog::checkIdsAgainstFreeTerminals
	@param ids : the expanded IDs of the ID field
	@return one message per label 'strip name:ID' already used by an
	independent (that is to say: free) terminal of the project, or nothing.
*/
QStringList AddTerminalStripDialog::checkIdsAgainstFreeTerminals(const QStringList &ids) const
{
	QStringList messages;
	if (ids.isEmpty() || !m_project) {
		return messages;
	}

	const auto name = effectiveName();
	const auto free_terminals = ElementProvider(m_project).freeTerminal();
	for (const auto &id : ids) {
		const auto label = name + QStringLiteral(":") + id;
		for (auto terminal : free_terminals) {
			if (terminal->actualLabel() == label) {
				messages << tr("Label %1 is already used by an independent terminal").arg(label);
				break;
			}
		}
	}

	return messages;
}

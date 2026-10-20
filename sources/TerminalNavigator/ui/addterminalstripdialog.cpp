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

#include "../../ElementsCollection/elementslocation.h"
#include "../../ElementsCollection/xmlelementcollection.h"
#include "../../TerminalStrip/physicalterminal.h"
#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../elementprovider.h"
#include "../../qet.h"
#include "../../qetapp.h"
#include "../../qeticons.h"
#include "../../qetproject.h"
#include "../../utils/qetutils.h"

#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QFutureWatcher>
#include <QSignalBlocker>
#include <QTextDocument>
#include <QTreeWidgetItem>
#include <QtConcurrentRun>

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

	/// Reads the root of an element file and tells whether it is a terminal
	/// symbol. Used by the background scan of the standard collection: a
	/// plain file read, no collection, no project, no shared state, so it
	/// can run off the GUI thread.
	bool fileIsATerminalSymbol(const QString &file_path)
	{
		QFile file(file_path);
		if (!file.open(QIODevice::ReadOnly)) {
			return false;
		}

		QDomDocument document;
		if (!document.setContent(&file)) {
			return false;
		}

		return document.documentElement().attribute(QStringLiteral("link_type"))
		        == QLatin1String("terminal");
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
	ui->m_notice_lb->setStyleSheet(QStringLiteral("QLabel {color: red;}"));
	ui->m_preview_tree->setHeaderLabels(
				{tr("ID"), tr("Symbol")});

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

		//The list of symbols is read on the first popup of its combo box
		//only, @sa fillSymbolCombo().
	ui->m_symbol_cb->addItem(QIcon(), tr("(no symbol)"), QString());
	const auto preselected = preselectedSymbolOfTheProject();
	if (!preselected.isEmpty()) {
		const ElementsLocation location(preselected);
		ui->m_symbol_cb->addItem(location.icon(), location.name(), preselected);
		ui->m_symbol_cb->setCurrentIndex(ui->m_symbol_cb->count() - 1);
	}
	ui->m_symbol_cb->installEventFilter(this);
	m_symbol_watcher = new QFutureWatcher<QStringList>(this);
	connect(m_symbol_watcher, &QFutureWatcher<QStringList>::finished,
		this, &AddTerminalStripDialog::onSymbolScanFinished);
		//The symbol shown by the combo box is the one the terminals typed
		//next are created with: @sa onSymbolPicked().
	m_default_symbol = symbolLocation();

	fillInstallationAndLocation();

	connect(ui->m_installation_cb, &QComboBox::currentTextChanged,
		this, &AddTerminalStripDialog::fillNamesOfTheCurrentNode);
	connect(ui->m_location_cb, &QComboBox::currentTextChanged,
		this, &AddTerminalStripDialog::fillNamesOfTheCurrentNode);
	connect(ui->m_name_cb, &QComboBox::currentTextChanged,
		this, &AddTerminalStripDialog::validate);
	connect(ui->m_id_le, &QLineEdit::textChanged,
		this, &AddTerminalStripDialog::validate);
	connect(ui->m_symbol_cb, &QComboBox::currentIndexChanged,
		this, &AddTerminalStripDialog::onSymbolPicked);
	connect(ui->m_preview_tree, &QTreeWidget::itemSelectionChanged,
		this, &AddTerminalStripDialog::onPreviewSelectionChanged);

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
	field, in the order of the preview tree, each with the symbol the user
	gave it.
	The preview tree is read and not the ID field, because it is the only
	place which knows the symbol of each single terminal, and because it is
	what the user looked at before pressing OK. A list which can not be
	read at all leaves the tree on its last valid state, and the dialog
	refuses that state with its notice label, so the OK button is disabled
	before this could return something the user did not see.
*/
QList<AddTerminalStripDialog::TerminalDescription> AddTerminalStripDialog::describedTerminals() const
{
	QList<TerminalDescription> result;
	if (!m_root_item) {
		return result;
	}

	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		const auto *child = m_root_item->child(i);
		result.append({child->text(0), child->data(0, Qt::UserRole).toString()});
	}

	return result;
}

/**
	@brief AddTerminalStripDialog::symbolLocation
	@return the location string of the symbol shown in the symbol combo
	box ('embed://...' for a symbol of the project, 'common://...' for one
	of the standard collection), or an empty string when that box shows
	none -- the terminals are then created without a symbol, and can be
	drawn later on with whatever symbol is chosen then.
*/
QString AddTerminalStripDialog::symbolLocation() const
{
	const int index = ui->m_symbol_cb->currentIndex();
	return index < 0 ? QString() : ui->m_symbol_cb->itemData(index).toString();
}

/**
	@brief AddTerminalStripDialog::eventFilter
	Starts the reading of the symbol list on the first interaction with the
	symbol combo box -- a key press or a click, both of which open it.
*/
bool AddTerminalStripDialog::eventFilter(QObject *watched, QEvent *event)
{
	if ((event->type() == QEvent::MouseButtonPress
	     || event->type() == QEvent::KeyPress)
	    && (watched == ui->m_symbol_cb
	        || watched == ui->m_symbol_cb->lineEdit())) {
		fillSymbolCombo();
	}

	return QDialog::eventFilter(watched, event);
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
	const auto name = effectiveName();

		//The ID field is read even when the name is empty: the preview tree
		//follows it, and a list which can not be read at all is worth
		//telling about right away.
	const auto parsed = parseIds();
	if (!parsed.error.isEmpty()) {
		messages << parsed.error;
	} else if (!name.isEmpty()) {
		messages << checkIdsAgainstTheStrip(parsed.ids);
		messages << checkIdsAgainstFreeTerminals(parsed.ids);
	}

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
		!name.isEmpty() && messages.isEmpty());
	ui->m_notice_lb->setText(messages.join(QLatin1Char('\n')));
	ui->m_notice_lb->setVisible(!messages.isEmpty());

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
	@brief AddTerminalStripDialog::refreshPreviewTree
	Shows what the OK button will create: the strip as the root item of the
	preview tree, one child per ID of the ID field, the ID in the first
	column and the name of its symbol in the second one.
	The children are rebuilt only when the ID list really changed, since a
	rebuild throws away the selection the user is looking at; a list which
	can not be read at all leaves the last known children alone.
	A terminal the user gave a symbol keeps it across such a rebuild -- only
	the IDs which are new get the symbol of the moment, so that retyping an
	ID list never silently gives the terminals another symbol than the one
	the second column shows.
*/
void AddTerminalStripDialog::refreshPreviewTree(const ParsedIds &parsed)
{
	if (!m_root_item) {
		return;
	}

	m_root_item->setText(0, effectiveName());
	m_root_item->setIcon(0, QET::Icons::TerminalStrip);
	m_root_item->setFirstColumnSpanned(true);

	if (!parsed.error.isEmpty()) {
		return;
	}

	QStringList current_ids;
	for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
		current_ids << m_root_item->child(i)->text(0);
	}

	if (current_ids != parsed.ids) {
		QStringList selected_ids;
		QHash<QString, QString> kept_symbols;
		for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
			const auto *child = m_root_item->child(i);
			if (child->isSelected()) {
				selected_ids << child->text(0);
			}
			kept_symbols.insert(child->text(0),
								child->data(0, Qt::UserRole).toString());
		}

		qDeleteAll(m_root_item->takeChildren());
		for (const auto &id : parsed.ids) {
				//No icon: those terminals are not drawn in any folio yet,
				//and the missing symbol is how the trees tell them apart
			auto *child = new QTreeWidgetItem(m_root_item, QStringList(id));
			const auto symbol = kept_symbols.value(id, m_default_symbol);
			child->setData(0, Qt::UserRole, symbol);
			child->setText(1, symbolName(symbol));
		}

		for (int i = 0 ; i < m_root_item->childCount() ; ++i) {
			if (selected_ids.contains(m_root_item->child(i)->text(0))) {
				m_root_item->child(i)->setSelected(true);
			}
		}
	}

	ui->m_preview_tree->expandItem(m_root_item);
}

/**
	@brief AddTerminalStripDialog::onSymbolPicked
	@param index : the item the symbol combo box just selected
	Applies the symbol of the combo box to the terminals the user selected
	in the preview tree. With nothing selected, the value only becomes the
	symbol the terminals typed next are created with -- it must never
	change the terminals already listed behind the back of the user, since
	the second column shows exactly that value.
*/
void AddTerminalStripDialog::onSymbolPicked(int index)
{
	const auto symbol = index < 0
			? QString()
			: ui->m_symbol_cb->itemData(index).toString();

	const auto selected = selectedPreviewChildren();
	if (selected.isEmpty()) {
		m_default_symbol = symbol;
		return;
	}

	for (auto *item : selected) {
		item->setData(0, Qt::UserRole, symbol);
		item->setText(1, symbolName(symbol));
	}
}

/**
	@brief AddTerminalStripDialog::onPreviewSelectionChanged
	Follows the selection of the preview tree in the symbol combo box, so
	that one selected terminal shows the symbol it carries and the user
	reads what he is about to change. Several selected terminals leave the
	combo box alone: they may well carry different symbols, and the next
	value applies to all of them.
*/
void AddTerminalStripDialog::onPreviewSelectionChanged()
{
	const auto selected = selectedPreviewChildren();
	if (selected.size() != 1) {
		return;
	}

	const auto symbol = selected.first()->data(0, Qt::UserRole).toString();
	if (symbol == symbolLocation()) {
		return;
	}

	const QSignalBlocker blocker(ui->m_symbol_cb);
	const int index = ui->m_symbol_cb->findData(symbol);
	ui->m_symbol_cb->setCurrentIndex(index < 0 ? 0 : index);
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
		if (auto *child = m_root_item->child(i); child->isSelected()) {
			result << child;
		}
	}

	return result;
}

/**
	@brief AddTerminalStripDialog::fillSymbolCombo
	Reads the terminal symbols the terminals can be drawn with, and fills
	the symbol combo box with them. The list is read on the first popup of
	that combo box only: a dialog opened to name a strip has no use for it,
	and the standard collection holds more than a thousand elements.
	The symbols of the project (its embedded collection, which lives in
	memory) are added right away; the ones of the standard collection are
	looked for in the background, since reading eleven hundred files here
	would freeze the interface.
*/
void AddTerminalStripDialog::fillSymbolCombo()
{
	if (m_symbol_scan_started) {
		return;
	}
	m_symbol_scan_started = true;

	if (m_project && m_project->embeddedElementCollection()) {
		const auto definitions = m_project->embeddedElementCollection()->root()
			.elementsByTagName(QStringLiteral("definition"));
		for (int i = 0 ; i < definitions.count() ; ++i) {
			const auto definition = definitions.at(i).toElement();
			if (definition.attribute(QStringLiteral("link_type")) != QLatin1String("terminal")) {
				continue;
			}
			appendSymbolItem(m_project->embeddedElementCollection()->domToLocation(
				definition.parentNode().toElement()));
		}
	}

	const auto common_dir = QETApp::commonElementsDirN();
	m_symbol_watcher->setFuture(QtConcurrent::run([common_dir]() {
		QStringList paths;
		QDirIterator it(common_dir,
						QStringList{QStringLiteral("*.elmt")},
						QDir::Files,
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const auto path = it.next();
			if (fileIsATerminalSymbol(path)) {
				paths << path;
			}
		}
		std::sort(paths.begin(), paths.end(), QETUtils::naturalLessThan);
		return paths;
	}));
}

/**
	@brief AddTerminalStripDialog::appendSymbolItem
	Adds one symbol to the symbol combo box, unless a symbol with the same
	location is listed already -- the same element may well be part of the
	project and of the standard collection at once.
*/
void AddTerminalStripDialog::appendSymbolItem(const ElementsLocation &location)
{
	const auto location_string = location.toString();
	if (location_string.isEmpty()) {
		return;
	}
	if (ui->m_symbol_cb->findData(location_string) >= 0) {
		return;
	}

	ui->m_symbol_cb->addItem(location.icon(), location.name(), location_string);
	m_symbol_names.insert(location_string, location.name());
}

/**
	@brief AddTerminalStripDialog::onSymbolScanFinished
	Adds the terminal symbols found in the standard collection to the ones
	already listed, and puts the symbol which was selected before back.
*/
void AddTerminalStripDialog::onSymbolScanFinished()
{
	const auto selected = symbolLocation();
	const QSignalBlocker blocker(ui->m_symbol_cb);

	const auto common_dir = QETApp::commonElementsDirN();
	for (const auto &path : m_symbol_watcher->result()) {
		auto relative_path = QDir(common_dir).relativeFilePath(path);
		relative_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
		appendSymbolItem(ElementsLocation(QStringLiteral("common://") + relative_path));
	}

	int index = ui->m_symbol_cb->findData(selected);
	if (index < 0 && !selected.isEmpty()) {
			//A symbol of the project which belongs to none of the
			//collections: keep it listed, since it is selected all the same
		appendSymbolItem(ElementsLocation(selected));
		index = ui->m_symbol_cb->findData(selected);
	}
	ui->m_symbol_cb->setCurrentIndex(index < 0 ? 0 : index);
}

/**
	@brief AddTerminalStripDialog::symbolName
	@param symbol : the location string of a symbol
	@return the name to show for that symbol in the second column of the
	preview tree: the one the symbol combo box lists when the symbol is
	there, since an element name is read out of its XML and is worth
	reading once only -- which is what the cache is for.
*/
QString AddTerminalStripDialog::symbolName(const QString &symbol) const
{
	if (symbol.isEmpty()) {
		return QString();
	}
	if (!m_symbol_names.contains(symbol)) {
		const int index = ui->m_symbol_cb->findData(symbol);
		const auto name = index >= 0
				? ui->m_symbol_cb->itemText(index)
				: ElementsLocation(symbol).name();
		m_symbol_names.insert(symbol, name);
	}

	return m_symbol_names.value(symbol);
}

/**
	@brief AddTerminalStripDialog::preselectedSymbolOfTheProject
	@return the location string of the symbol the terminals should be drawn
	with by default, which is the first terminal symbol of the project, or
	an empty string when the project holds none.
	The project is where the user keeps the symbols he drew his terminals
	with, so the first of them is the one he is the most likely to want
	again. Nothing is guessed from the standard collection: a project which
	has no symbol of its own has no habit worth copying.
*/
QString AddTerminalStripDialog::preselectedSymbolOfTheProject() const
{
	if (!m_project || !m_project->embeddedElementCollection()) {
		return QString();
	}

	QList<ElementsLocation> locations;
	const auto definitions = m_project->embeddedElementCollection()->root()
		.elementsByTagName(QStringLiteral("definition"));
	for (int i = 0 ; i < definitions.count() ; ++i) {
		const auto definition = definitions.at(i).toElement();
		if (definition.attribute(QStringLiteral("link_type")) != QLatin1String("terminal")) {
			continue;
		}
		auto location = m_project->embeddedElementCollection()->domToLocation(
			definition.parentNode().toElement());
		if (!location.isNull()) {
			locations << location;
		}
	}
	if (locations.isEmpty()) {
		return QString();
	}

	std::sort(locations.begin(), locations.end(),
		[](const ElementsLocation &a, const ElementsLocation &b) {
			return QETUtils::naturalLessThan(a.name(), b.name());
		});

	return locations.first().toString();
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
*/
AddTerminalStripDialog::ParsedIds AddTerminalStripDialog::parseIds() const
{
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

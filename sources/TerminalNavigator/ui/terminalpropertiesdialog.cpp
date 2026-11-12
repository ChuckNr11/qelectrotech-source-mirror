/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file terminalpropertiesdialog.cpp
	@brief Implementation of the TerminalPropertiesDialog class
 */
#include "terminalpropertiesdialog.h"
#include "ui_terminalpropertiesdialog.h"

#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalcatalog.h"
#include "../../TerminalStrip/terminalcolor.h"
#include "../../qet.h"
#include "../../qetgraphicsitem/element.h"

#include <QLocale>
#include <QLineEdit>
#include <QRegularExpression>

namespace {
	/// Writes @a flag in @a informations the way a flag is stored there
	/// (@sa QET::infoFlagIsTrue), or takes the key away when the flag is
	/// off -- which is exactly what the editor of an element does, and
	/// what keeps an untouched terminal out of the way of the values its
	/// symbol is drawn with.
	void setInfoFlag(DiagramContext &informations, const QString &key, bool flag)
	{
		if (flag) {
			informations.addValue(key, QStringLiteral("true"));
		} else {
			informations.remove(key);
		}
	}

	/**
	 * @brief leadingNumber
	 * Extract the first number of a text like "2,5 mm²" or "5.1 mm - WAGO".
	 * The comma and the dot are both accepted as decimal separator.
	 * @return the number, or 0 if the text is empty or has no number.
	 */
	double leadingNumber(const QString &text)
	{
		static const QRegularExpression re(QStringLiteral("(\\d+(?:[.,]\\d+)?)"));
		const auto match = re.match(text);
		if (!match.hasMatch())
			return 0;
		auto number = match.captured(1);
		number.replace(QLatin1Char(','), QLatin1Char('.'));
		return number.toDouble();
	}

	/// Value of a terminal size combo (picked in the list or typed).
	double comboValue(const QComboBox *cb)
	{
		return leadingNumber(cb->currentText());
	}

	/// Color of a terminal combo as "#rrggbb" or "#rrggbb/#rrggbb" (empty = none).
	/// A text which is not understood keeps the value the combo was loaded with.
	QString colorSpec(const QComboBox *cb)
	{
		const auto index = cb->findText(cb->currentText());
		if (index >= 0)
			return cb->itemData(index).toString();

		QColor main_, stripe_;
		if (TerminalColor::parse(cb->currentText(), &main_, &stripe_))
			return TerminalColor::toSpec(main_, stripe_);
		return cb->property("origSpec").toString();
	}

	/// Role of the width combo items holding the label of the catalog entry
	const int CatalogEntryRole = Qt::UserRole + 1;

	/// Label of the catalog entry selected in the width combo,
	/// empty if the width was typed or "None" is selected.
	QString catalogEntry(const QComboBox *cb)
	{
		const auto index = cb->findText(cb->currentText());
		return index > 0 ? cb->itemData(index, CatalogEntryRole).toString() : QString();
	}

	/// Select the item of the catalog entry @a label in the width combo.
	/// @return false if the combo has no such item
	bool selectCatalogEntry(QComboBox *cb, const QString &label)
	{
		if (label.isEmpty())
			return false;
		const auto index = cb->findData(label, CatalogEntryRole);
		if (index < 0)
			return false;
		cb->setCurrentIndex(index);
		return true;
	}

	/// Set a width not tied to a terminal of the catalog (typed, or stored before
	/// the catalog entry was) : written as text, so that it doesn't take the first
	/// terminal of the catalog with the same width.
	void setPlainWidth(QComboBox *cb, double width, const QString &suffix)
	{
		if (width <= 0) {
			cb->setCurrentIndex(0);
			return;
		}
		cb->setCurrentText(QLocale().toString(width, 'g', 6) + suffix);
	}

	/// Set the text of an editable combo : select the item with this value or write the value.
	void setComboValue(QComboBox *cb, double value, const QString &suffix)
	{
		if (value <= 0) {
			cb->setCurrentIndex(0);
			return;
		}
		for (int i = 1; i < cb->count(); ++i) {
			if (qFuzzyCompare(1 + cb->itemData(i).toDouble(), 1 + value)) {
				cb->setCurrentIndex(i);
				return;
			}
		}
		cb->setCurrentText(QLocale().toString(value, 'g', 6) + suffix);
	}
}

/**
	@brief TerminalProperties::applyTo
	Writes those properties into @a data (@sa TerminalProperties).
	Only the values the user really chose are written: the generic type,
	the generic function and a size or a color which says nothing are left
	out, so that a terminal keeps the generic values its symbol is drawn
	with -- which is what the model reads back out of the data
	(@sa ElementData::terminalType, and BindTerminalCommand::takenOverData,
	which takes a generic value for a value nobody chose).
 */
void TerminalProperties::applyTo(ElementData &data) const
{
		//The type and the function are set as overrides, which is the way
		//a terminal which is drawn in no folio keeps them: those values
		//override the ones of the symbol it is drawn with later on, and
		//they are the ones RealTerminal::toXml saves for such a terminal.
	data.setTerminalType(type);
	data.setTerminalFunction(function);

	setInfoFlag(data.m_informations, QStringLiteral("exclude_from_bom"), exclude_from_bom);
	setInfoFlag(data.m_informations, QStringLiteral("auto_num_locked"), auto_num_locked);
	setInfoFlag(data.m_informations, QStringLiteral("potential_isolating"), potential_isolating);

		//Section (mm²) and width (mm), stored with the dot as decimal
		//separator, whatever the locale
	if (section > 0) {
		data.m_informations.addValue(RealTerminal::sectionInfoKey(),
									 QString::number(section, 'g', 6));
	} else {
		data.m_informations.remove(RealTerminal::sectionInfoKey());
	}
	if (width > 0) {
		data.m_informations.addValue(RealTerminal::widthInfoKey(),
									 QString::number(width, 'g', 6));
	} else {
		data.m_informations.remove(RealTerminal::widthInfoKey());
	}

		//The terminal chosen in the catalog, its manufacturer and its
		//reference -- written only when the user did not type a width
		//which belongs to none of them (@sa applyCatalogEntry)
	RealTerminal::applyCatalogEntry(data.m_informations,
									width > 0 ? catalog_entry : QString());

	if (color_spec.isEmpty()) {
		data.m_informations.remove(RealTerminal::colorInfoKey());
	} else {
		data.m_informations.addValue(RealTerminal::colorInfoKey(), color_spec);
	}
}

/**
	@brief TerminalProperties::fromRealTerminal
	@param terminal : the terminal to read, which may be null -- nothing
	is then read at all, and the defaults of the struct are returned.
	@return what @a terminal holds, read the way the editor of an element
	reads its terminal values (@sa TerminalProperties).
 */
TerminalProperties TerminalProperties::fromRealTerminal(const RealTerminal *terminal)
{
	TerminalProperties properties;
	if (!terminal) {
		return properties;
	}

	const auto data = terminal->terminalData();
	const auto &informations = data.m_informations;

		//A terminal drawn in a folio keeps no symbol string of its own:
		//its symbol is the one of its element, which is the very value
		//the trees of this dialog show for it (@sa
		//AddTerminalsDialog::fillExistingTerminals). Id-free, as
		//every symbol string of this dialog is: collectionPath() and not
		//toString(), since toString() prefixes the path with the runtime
		//id of the project, an id which means nothing in a later session
		//(@sa TerminalSymbolList).
	properties.symbol = terminal->symbol();
	if (properties.symbol.isEmpty()
		&& terminal->isElement()
		&& terminal->element())
	{
		properties.symbol = terminal->element()->location().collectionPath();
	}

	properties.type = data.terminalType();
	properties.function = data.terminalFunction();
	properties.section = informations.value(RealTerminal::sectionInfoKey()).toString().toDouble();
	properties.width = informations.value(RealTerminal::widthInfoKey()).toString().toDouble();
	properties.catalog_entry = informations.value(RealTerminal::catalogEntryInfoKey()).toString();
	properties.color_spec = informations.value(RealTerminal::colorInfoKey()).toString();
	properties.exclude_from_bom = QET::infoFlagIsTrue(
		informations.value(QStringLiteral("exclude_from_bom")).toString());
	properties.auto_num_locked = QET::infoFlagIsTrue(
		informations.value(QStringLiteral("auto_num_locked")).toString());
	properties.potential_isolating = QET::infoFlagIsTrue(
		informations.value(QStringLiteral("potential_isolating")).toString());

	return properties;
}

/**
	@brief TerminalPropertiesDialog::TerminalPropertiesDialog
	@param values
	@param parent
 */
TerminalPropertiesDialog::TerminalPropertiesDialog(
		const TerminalProperties &values,
		QWidget *parent) :
	QDialog(parent),
	ui(new Ui::TerminalPropertiesDialog)
{
	ui->setupUi(this);

	fillTypeAndFunctionCombos();
	ui->m_terminal_type_cb->setCurrentIndex(
		ui->m_terminal_type_cb->findData(values.type));
	ui->m_terminal_func_cb->setCurrentIndex(
		ui->m_terminal_func_cb->findData(values.function));

		//The lock of the automatic numbering is always checked when
		//the dialog opens: it is a choice the user makes here, never
		//the display of what a terminal carries already -- what a
		//terminal is worth is what OK writes for it, whether the box
		//was looked at or not
		//@sa TerminalProperties::auto_num_locked.
	ui->m_auto_num_locked_cb->setChecked(true);
	ui->m_potential_isolating_cb->setChecked(values.potential_isolating);
	ui->m_exclude_from_bom_cb->setChecked(values.exclude_from_bom);

	populateTerminalSizeCombos(values.section, values.width, values.catalog_entry);
	populateTerminalColorCombo(values.color_spec);

		//The width list follows the section and the manufacturer from here
		//on: the two combo boxes the widths depend on were just filled, so
		//the connections are made after that, not before
		//@sa refreshTerminalWidthCombo.
	connect(ui->m_terminal_section_cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &TerminalPropertiesDialog::refreshTerminalWidthCombo);
	connect(ui->m_terminal_section_cb->lineEdit(), &QLineEdit::editingFinished,
			this, [this]() { refreshTerminalWidthCombo(); });
	connect(ui->m_terminal_manufacturer_cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &TerminalPropertiesDialog::refreshTerminalWidthCombo);

	QET::trackDialogGeometry(this);
}

/**
	@brief TerminalPropertiesDialog::~TerminalPropertiesDialog
 */
TerminalPropertiesDialog::~TerminalPropertiesDialog()
{
	delete ui;
}

/**
	@brief TerminalPropertiesDialog::properties
	@return what the OK button chose, read out of the fields the way the
	editor of an element reads them (@sa TerminalPropertiesDialog).
 */
TerminalProperties TerminalPropertiesDialog::properties() const
{
	TerminalProperties result;

		//The symbol is not among them: the dialog which creates the
		//terminals picks that one itself
		//(@sa AddTerminalsDialog::m_symbol_cb).
	result.type = ui->m_terminal_type_cb->currentData().value<ElementData::TerminalType>();
	result.function = ui->m_terminal_func_cb->currentData().value<ElementData::TerminalFunction>();

	result.exclude_from_bom = ui->m_exclude_from_bom_cb->isChecked();
	result.auto_num_locked = ui->m_auto_num_locked_cb->isChecked();
	result.potential_isolating = ui->m_potential_isolating_cb->isChecked();

		//Section (mm²) and width (mm) are read out of the fields, since
		//both are editable: a value typed by the user is kept, and is
		//stored with the dot as decimal separator, whatever the locale
	result.section = comboValue(ui->m_terminal_section_cb);
	result.width = comboValue(ui->m_terminal_width_cb);
	result.catalog_entry = catalogEntry(ui->m_terminal_width_cb);
	result.color_spec = colorSpec(ui->m_terminal_color_cb);

	return result;
}

/**
	@brief TerminalPropertiesDialog::fillTypeAndFunctionCombos
	Fills the type and the function combo boxes with the very values the
	terminal group box of the element editor offers
	(@sa ElementPropertiesEditorWidget::setUpInterface), so that a
	terminal created here and one created in the editor read the same way
	in the terminal strip editor.
 */
void TerminalPropertiesDialog::fillTypeAndFunctionCombos()
{
	ui->m_terminal_type_cb->addItem(tr("Generic"),    ElementData::TTGeneric);
	ui->m_terminal_type_cb->addItem(tr("Fuse"),      ElementData::TTFuse);
	ui->m_terminal_type_cb->addItem(tr("Sectionable"), ElementData::TTSectional);
	ui->m_terminal_type_cb->addItem(tr("Diode"),        ElementData::TTDiode);
	ui->m_terminal_type_cb->addItem(tr("Ground"),        ElementData::TTGround);
	ui->m_terminal_type_cb->addItem(tr("Anti-parallel diodes"), ElementData::TTAntiParallelDiodes);
	ui->m_terminal_type_cb->addItem(tr("Resistor"),      ElementData::TTResistor);
	ui->m_terminal_type_cb->addItem(tr("Varistor"),      ElementData::TTVaristor);
	ui->m_terminal_type_cb->addItem(tr("Test socket"),   ElementData::TTTestSocket);
	ui->m_terminal_type_cb->addItem(tr("Diode (reversed)"), ElementData::TTDiodeReversed);

	ui->m_terminal_func_cb->addItem(tr("Generic"), ElementData::TFGeneric);
	ui->m_terminal_func_cb->addItem(tr("Phase"),     ElementData::TFPhase);
	ui->m_terminal_func_cb->addItem(tr("Neutral"),    ElementData::TFNeutral);
}

/**
	@brief TerminalPropertiesDialog::populateTerminalSizeCombos
	Fill the section and width combo boxes with the catalog terminals and
	select @a section / @a width (mm² and mm).
	The combo boxes are editable : a value not in the list can be typed.
 */
void TerminalPropertiesDialog::populateTerminalSizeCombos(double section, double width, const QString &catalog_entry)
{
	QSignalBlocker blocker(ui->m_terminal_section_cb);
	ui->m_terminal_section_cb->clear();
	ui->m_terminal_section_cb->addItem(tr("None (default)"), 0.0);
	for (const auto section_ : TerminalCatalog::sections()) {
		ui->m_terminal_section_cb->addItem(
			TerminalCatalog::sectionText(section_, tr("%1 mm²").arg(QLocale().toString(section_, 'g', 6))),
			section_);
	}
	setComboValue(ui->m_terminal_section_cb, section, tr(" mm²"));

		//Unlike the editor of an element, this dialog has the manufacturer
		//row in its .ui file already (@c m_terminal_manufacturer_cb): it is
		//filled, never created, and the width list follows it from here on.
	auto *man_cb = ui->m_terminal_manufacturer_cb;
	{
		QSignalBlocker man_blocker(man_cb);
		man_cb->clear();
		man_cb->addItem(tr("All manufacturers"), QString());
		for (const auto &name_ : TerminalCatalog::manufacturers()) {
			man_cb->addItem(name_, name_);
		}
			//Select the manufacturer of the terminal chosen in the catalog
		const auto *entry_ = TerminalCatalog::entryByLabel(catalog_entry);
		const auto entry_index_ = entry_ ? man_cb->findData(QString::fromUtf8(entry_->manufacturer)) : -1;
		man_cb->setCurrentIndex(entry_index_ > 0 ? entry_index_ : 0);
	}

	refreshTerminalWidthCombo();
	if (!selectCatalogEntry(ui->m_terminal_width_cb, catalog_entry)) {
		setPlainWidth(ui->m_terminal_width_cb, width, tr(" mm"));
	}
}

/**
	@brief TerminalPropertiesDialog::populateTerminalColorCombo
	Fill the color combo box with the palette of the terminal strip
	and select @a spec ("#rrggbb" or "#rrggbb/#rrggbb", empty = none).
	The combo box is editable : a color can be typed as a name or as "#rrggbb".
 */
void TerminalPropertiesDialog::populateTerminalColorCombo(const QString &spec)
{
	auto *cb = ui->m_terminal_color_cb;
	cb->setProperty("origSpec", spec);
	cb->clear();
	cb->addItem(tr("None (default)"), QString());
	for (const auto &entry : TerminalColor::palette())
	{
		QColor main_, stripe_;
		TerminalColor::parse(TerminalColor::specOf(entry), &main_, &stripe_);
		cb->addItem(QIcon(TerminalColor::swatch(main_, stripe_)),
					TerminalColor::translatedName(entry),
					TerminalColor::specOf(entry));
	}

	if (spec.isEmpty()) {
		cb->setCurrentIndex(0);
		return;
	}
	const auto index = cb->findData(spec);
	if (index >= 0) {
		cb->setCurrentIndex(index);
	} else {
		//The color is not in the palette, keep it in the list
		QColor main_, stripe_;
		TerminalColor::parse(spec, &main_, &stripe_);
		cb->addItem(QIcon(TerminalColor::swatch(main_, stripe_)), spec, spec);
		cb->setCurrentIndex(cb->count() - 1);
	}
}

/**
	@brief TerminalPropertiesDialog::refreshTerminalWidthCombo
	List the manufacturer terminals compatible with the selected section.
	If the section has no match in the catalog (or is not set),
	all the widths are listed. The current width is kept.
 */
void TerminalPropertiesDialog::refreshTerminalWidthCombo()
{
	const auto current_width = comboValue(ui->m_terminal_width_cb);
	const auto current_entry = catalogEntry(ui->m_terminal_width_cb);
	const auto section = comboValue(ui->m_terminal_section_cb);

	auto item_text = [](const TerminalCatalog::WidthEntry &entry) {
		return tr("%1 mm").arg(QLocale().toString(entry.mm, 'g', 6))
			   + QStringLiteral(" - ") + QString::fromUtf8(entry.label);
	};

	const auto manufacturer = ui->m_terminal_manufacturer_cb->currentData().toString();
	auto manufacturer_ok = [&manufacturer](const TerminalCatalog::WidthEntry &entry) {
		return manufacturer.isEmpty() || manufacturer == QString::fromUtf8(entry.manufacturer);
	};

	//Prefer the terminals of the selected section, if the selected manufacturer has some
	bool has_compatible_entry = false;
	if (section > 0) {
		for (const auto &entry : TerminalCatalog::widthEntries()) {
			if (manufacturer_ok(entry) && qFuzzyCompare(1 + entry.section, 1 + section)) {
				has_compatible_entry = true;
				break;
			}
		}
	}

	QSignalBlocker blocker(ui->m_terminal_width_cb);
	ui->m_terminal_width_cb->clear();
	ui->m_terminal_width_cb->addItem(tr("None (default)"), 0.0);
	for (const auto &entry : TerminalCatalog::widthEntries()) {
		if (manufacturer_ok(entry)
			&& (!has_compatible_entry || qFuzzyCompare(1 + entry.section, 1 + section))) {
			ui->m_terminal_width_cb->addItem(item_text(entry), entry.mm);
			ui->m_terminal_width_cb->setItemData(ui->m_terminal_width_cb->count() - 1,
												  QString::fromUtf8(entry.label), CatalogEntryRole);
		}
	}
	if (!selectCatalogEntry(ui->m_terminal_width_cb, current_entry)) {
		setPlainWidth(ui->m_terminal_width_cb, current_width, tr(" mm"));
	}
}

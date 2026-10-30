/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file terminalpropertiesdialog.h
	@brief Header of the TerminalPropertiesDialog class
 */
#ifndef TERMINALPROPERTIESDIALOG_H
#define TERMINALPROPERTIESDIALOG_H

#include "../../properties/elementdata.h"

#include <QDialog>
#include <QString>
#include <QStringList>

class RealTerminal;
namespace Ui {
	class TerminalPropertiesDialog;
}

/**
	@brief The TerminalProperties struct
	What one terminal of a terminal strip is: the symbol it is drawn with,
	its type and its function, the section, the width and the color it is
	kept with, and the three flags the element editor asks for as well.
	It is the same set of values the element editor asks for in its
	terminal group box (@sa ElementPropertiesEditorWidget), and it is
	written the very same way -- a terminal keeps the values the editor
	would write into its element, since a terminal which is drawn in no
	folio keeps data of its own (@sa RealTerminal::setSpareData).

	Only the value the user really chose is written: the generic type, the
	generic function and a section, a width or a color which says nothing
	are kept out of the data, so that a terminal drawn later on still takes
	what its symbol says for the values nobody chose
	(@sa BindTerminalCommand::takenOverData).
*/
struct TerminalProperties
{
	/// Location string of the symbol the terminal is drawn with, id-free
	/// and empty when it should be drawn with none (@sa TerminalSymbolList).
	QString symbol;
	ElementData::TerminalType type = ElementData::TTGeneric;
	ElementData::TerminalFunction function = ElementData::TFGeneric;
	/// In mm² and in mm, zero when the terminal has none -- which is the
	/// value the catalogs store for "None (default)".
	double section{0.0};
	double width{0.0};
	/// Label of the terminal of the catalog the width was picked from,
	/// empty when the width was typed or picked without a catalog entry.
	QString catalog_entry;
	/// "#rrggbb" or "#rrggbb/#rrggbb", empty when there is no color.
	QString color_spec;
	bool exclude_from_bom{false};
		/// Locked by default: the dialog of the properties always opens
		/// with this one checked, so that a terminal the user creates is
		/// the one the automatic numbering of the project leaves alone
		/// (@sa TerminalPropertiesDialog).
	bool auto_num_locked{true};
	bool potential_isolating{false};

	/**
		@brief applyTo
		Writes those properties into @a data, the data of a terminal which
		is drawn in no folio -- the values which say nothing are taken out
		again, so that a terminal the user never touched keeps the generic
		type and function its symbol is drawn with.
	*/
	void applyTo(ElementData &data) const;
	/**
		@brief fromRealTerminal
		@return what @a terminal holds, read the way the editor of an
		element reads it (@sa ElementPropertiesEditorWidget): out of its
		data for the type and the function, out of its informations for
		the sizes, the catalog entry and the color.
	*/
	static TerminalProperties fromRealTerminal(const RealTerminal *terminal);
};

/**
	@brief The TerminalPropertiesDialog class
	Dialog which gives the terminals the user marked their properties:
	symbol, type, function, sizes, color and flags. It holds every value
	the terminal group box of the element editor asks for
	(@sa ElementPropertiesEditorWidget, which is where those fields and
	their semantics come from), plus the symbol, which a terminal drawn in
	no folio can have of its own. It serves the terminals the strip
	dialog is about to create and the ones which exist already in the
	terminal navigator alike
	(@sa AddTerminalsDialog::openTerminalsDialog,
	TerminalNavigatorWidget::editSelection).

	The dialog works on the marked terminals alone: the OK button hands
	the same properties to all of them, which is the very point of marking
	many of them at once -- one terminal keeps the values it was created
	with, and every marked one gets what the dialog shows
	(@sa TerminalProperties).

	The values the dialog opens on are the ones of the last terminal the
	strip already holds, when there is one (@sa the @a values argument):
	the symbol, the section, the width and the color a user picked once
	are the ones he is the most likely to want again. The type and the
	function stay generic and the two other flags stay off, since those
	are the values he has to choose for every terminal himself, while the
	automatic numbering is locked whenever the dialog opens -- which is
	what he wants from the terminals he creates -- and every terminal the
	dialog is not asked about keeps exactly those values
	(@sa AddTerminalsDialog::defaultsForNewTerminals).

	What the dialog asks for is what is worth asking: the symbol a
	terminal is drawn with is not among it. The dialog which creates
	terminals picks that symbol once, in the row of symbols its own
	toolbar carries (@sa AddTerminalsDialog), and a terminal which exists
	already is drawn with the symbol of its element -- so there is
	nothing this dialog could ask about it. It is also why the list of
	the symbols stays where it is: this dialog holds none
	(@sa TerminalSymbolList).
*/
class TerminalPropertiesDialog : public QDialog
{
	Q_OBJECT

	public:
		/**
			@brief TerminalPropertiesDialog
			@param values : the properties the dialog opens on
			@param parent
		*/
		TerminalPropertiesDialog(const TerminalProperties &values,
		                         QWidget *parent = nullptr);
		~TerminalPropertiesDialog() override;

		/**
			@brief properties
			@return what the OK button chose: the values shown in the
			fields, read the way the element editor reads them -- a value
			which was typed into an editable combo box is read as it is
			shown, and a value which says nothing is left out.
		*/
		TerminalProperties properties() const;

	private:
		void fillTypeAndFunctionCombos();
		void populateTerminalSizeCombos(double section, double width, const QString &catalog_entry);
		void populateTerminalColorCombo(const QString &spec);
		/// @brief refreshTerminalWidthCombo
		/// Lists the manufacturer terminals compatible with the selected
		/// section. If the section has no match in the catalog (or is not
		/// set), all the widths are listed. The current width is kept.
		void refreshTerminalWidthCombo();

		Ui::TerminalPropertiesDialog *ui;
};

#endif // TERMINALPROPERTIESDIALOG_H

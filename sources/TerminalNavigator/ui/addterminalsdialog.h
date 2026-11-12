/*
	Copyright 2006-2026  The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file addterminalsdialog.h
	@brief Header of the AddTerminalsDialog class
*/
#ifndef ADDTERMINALSDIALOG_H
#define ADDTERMINALSDIALOG_H

#include "terminalpropertiesdialog.h"

#include <QDialog>
#include <QHash>
#include <QList>

class QTreeWidgetItem;
class QETProject;
class TerminalStrip;
class TerminalSymbolList;
namespace Ui {
	class AddTerminalsDialog;
}

/**
	@brief The AddTerminalsDialog class
	This dialog creates a terminal strip from the TerminalNavigator, that is
	to say: without going through the terminal strip editor.
	It is deliberately *not* the TerminalStripCreatorDialog of the terminal strip
	editor, because it gathers two extra informations the editor does not ask
	for (see below), and because the editor must keep working unchanged.
	@see terminalstripcreatordialog.h

	The identity of a terminal strip follows the structure designations of the
	project tree:
	@code
	RootItem                       = Project
	   Plant                       = '=Inst' (installation)
	      Location                 = '+Loc'  (location)
	         Klemmleistenname      = Name     (one strip is made of many terminals)
	            :                  = separator between strip name and ID
	            ID                 = number of the terminal in the strip
	@endcode
	The three first levels are gathered by the (editable) combo boxes and
	the strip name by @c m_name_cb. The terminals to add are named in one
	of two ways, said by the radio buttons: the number of them
	(@c m_number_rb, @c m_number_sb -- whole numbers from 0 to 99) or the
	IDs of them typed by hand (@c m_id_rb, @c m_id_le). The number way is
	the one the dialog opens on, and each radio button enables its own
	input and disables the other one
	(@sa onTerminalsModeChanged, generatedIds).

	Besides the strip, this dialog describes the terminals it holds: one per
	ID, labelled 'strip name:ID', each one with a set of properties of its
	own, because the OK button creates terminals which are not drawn in any
	folio yet, and only the model half of them.
	@see TerminalNavigatorWidget::createTerminalStrip, which turns those
	descriptions into real terminals.

	@c m_preview_tree shows those terminals while the user types: the strip
	as its root item spanning both columns, one child per ID, the ID in the
	first column and the name of the symbol it is drawn with in the second
	one. The properties of that terminal -- of which the location string of
	its symbol is one -- are kept in Qt::UserRole and in
	@m_properties, which is where a later drag out of this tree reads the
	symbol and where the OK button reads the rest
	(@sa propertiesOfRow).

	That tree also shows what the strip of the current node already holds,
	as soon as its name is shown (@c setName): one greyed out row per
	physical terminal of it, at the top of the list and ahead of the IDs
	typed below them, so that the user reads what is there before he types
	the ID which continues it. Such a row carries the ID alone in the first
	column -- the root item above already says which strip they belong to --
	and the name of the symbol the terminal is drawn with in the second one,
	empty when it is drawn with none -- the element of a drawn terminal
	being the one to ask for its symbol, since it keeps none of its own
	(@c RealTerminal::symbol). It is not selectable and is marked
	with ExistingTerminalRole -- it is the display of what exists, never
	one of the terminals the OK button creates
	(@sa describedTerminals).

	Above that tree is a toolbar holding one button (@c m_edit_terminals),
	which opens @c TerminalPropertiesDialog for the terminals the user
	marked. The button follows the selection of the tree, and a terminal
	the dialog is about to create is marked the moment its row appears --
	what it creates is what its buttons work on
	(@sa onPreviewSelectionChanged, refreshPreviewTree).

	The dialog is where a terminal gets its properties: symbol, type,
	function, section, width, color and flags. It works on the marked
	terminals alone -- one, several or all of them, marked with the mouse
	or the keyboard -- and the OK button gives every one of them the very
	values it shows, which is the point of marking many of them at once
	(@sa openTerminalsDialog). The dialog opens on the values of the
	terminal the strip already holds, when there is one: the symbol, the
	sizes and the color a user picked once are the ones he is the most
	likely to want again, while the type and the function stay generic and
	the two other flags stay off, since those are the values he chooses
	for every terminal himself -- the automatic numbering is locked
	whenever the dialog opens, which is what he wants from the terminals
	he creates (@sa defaultsForNewTerminals). A terminal the dialog
	was never asked about keeps exactly those default values, which is why
	never silently changing what a terminal carries works out: the second
	column shows the symbol of every terminal, and the user trusts what he
	reads there.

	Both dialogs share one list of the symbols of the project and of the
	standard collection (@c TerminalSymbolList): the strip dialog names
	the symbols its rows show with it, and the properties dialog shows the
	list itself and starts reading the standard collection, which holds
	more than a thousand elements and is read in the background.
*/
class AddTerminalsDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit AddTerminalsDialog(QETProject *project, QWidget *parent = nullptr);
		~AddTerminalsDialog() override;

		void setInstallation(const QString &installation);
		void setLocation(const QString &location);
		/**
			@brief setName
			Pre-selects the strip name (structure designation 'Name'),
			to be called after setInstallation() and setLocation():
			those two reload the names of the node they select, and
			empty the name field with them (@sa
			fillNamesOfTheCurrentNode).
			A name which belongs to a strip of that node is picked
			from the list, the same way a user picks it; the preview
			tree then shows the terminals that strip already holds.
		*/
		void setName(const QString &name);
		TerminalStrip *generatedTerminalStrip() const;
		/// @return the terminal strip of the project whose node (plant,
		/// location, name) is the one currently shown, or null when the
		/// user asked for a strip which does not exist yet.
		TerminalStrip *terminalStripOfTheDialog() const;
		/// @brief TerminalDescription
		/// One terminal the OK button is about to create: an ID of the ID
		/// field, and the properties the user gave it -- the symbol it
		/// should be drawn with, its type and function, the sizes and the
		/// color it is kept with, and the flags (@sa TerminalProperties).
	struct TerminalDescription {
		QString id;
		TerminalProperties properties;
	};

		/// @return the terminals this dialog describes, one per ID the OK
		/// button is about to create (@sa parseIds) and in the order of the
		/// preview tree, each with the properties the user gave it. Nothing
		/// when the input naming them can not be read at all -- which the
		/// dialog refuses as an error anyway.
		/// The rows of the terminals the strip holds already are display
		/// only and are left out: they exist, the OK button must not
		/// create them a second time.
	QList<TerminalDescription> describedTerminals() const;

		/**
			@brief accept
			Takes what the dialog describes as the terminals to create
			(@sa describedTerminals) and remembers the symbol the last of
			them is created with as the one used last -- what the dialog
			opens on the next time, for a project which holds terminals
			already (@sa initialSymbol).
		*/
		void accept() override;

	private:
		/// Result of the reading of the input which names the terminals to
		/// add -- the ID field or the number of them (@sa parseIds): a list
		/// of IDs to add to the terminal block, or a message explaining why
		/// the list could not be read at all. The IDs the generator stepped
		/// over are kept apart in @c skipped: they are worth a hint in the
		/// notice label, but they are no error
		/// (@sa generatedIds, validate).
		struct ParsedIds {
			QStringList ids;
			QStringList skipped;
			QString error;
		};

		void fillInstallationAndLocation();
		void fillNamesOfTheCurrentNode();
		/**
			@brief onSymbolListRead
			Names the symbols the reading of the standard collection just
			added to the list: the second column of a row shows the name of
			its symbol, and a symbol read before it was listed keeps an
			empty name until this runs (@sa TerminalSymbolList). The
			combo box of the symbols is filled from that very list, and
			learns what arrived the same way (@sa fillSymbolCombo).
		*/
		void onSymbolListRead();
		/**
			@brief fillSymbolCombo
			@param selected : the location string of the symbol to select,
			or an empty string to select "no symbol"
			Fills the combo box with every symbol the list holds -- the
			symbols of the project first, then the ones of the standard
			collection as soon as they are read (@sa onSymbolListRead) --
			and selects @a selected.
		*/
		void fillSymbolCombo(const QString &selected);
		/**
			@brief setupSymbolCompleter
			The symbol combo box is editable, and is given a completer
			which completes on what is typed anywhere in a symbol name --
			'm10' finds 'Terminal M10', which the prefix matching of the
			combo box alone would not. The completer works on the names of
			the symbols listed, since that is what the user reads in the
			combo box.
		*/
		void setupSymbolCompleter();
		/**
			@brief onSymbolChanged
			Gives the symbol the combo box shows to the terminals marked in
			the preview tree -- and to those alone: a terminal which is not
			marked keeps the symbol it carries, and the rows the OK button
			is about to add are created with what the combo box shows at
			the moment they appear (@sa symbolOfRow).
		*/
		void onSymbolChanged();
		/// @brief onChooseSymbol
		/// Opens the dialog which chooses a terminal symbol in the
		/// collections and puts what it chose into the combo box
		/// (@sa chooseSymbol).
		void onChooseSymbol();
		/**
			@brief chooseSymbol
			@return the location string of the terminal symbol the user
			chose in the collections, id-free and empty when he chose
			none (@sa onChooseSymbol).
		*/
		QString chooseSymbol();
		/**
			@brief onSaveDefaultSymbol
			Keeps the symbol the combo box shows as the one the dialog
			opens on (@sa initialSymbol): its path goes into the settings,
			and one which says nothing -- "(no symbol)" -- takes that
			entry away again. What the project holds stays untouched: this
			is about the next dialog of the next project.
		*/
		void onSaveDefaultSymbol();
		/// @return the location string of the symbol the combo box shows,
		/// id-free and empty when it shows "(no symbol)".
		QString currentSymbol() const;
		/// Shows @p symbol in the combo box, listed or not (@sa
		/// fillSymbolCombo).
		void setCurrentSymbol(const QString &symbol);
		/**
			@brief symbolOfRow
			@param id : the ID of a terminal to be created
			@return the symbol that terminal is created with: what the
			user gave that very row, and what the combo box shows for a
			row he never worked on (@sa onSymbolChanged).
		*/
		QString symbolOfRow(const QString &id) const;
		/**
			@brief initialSymbol
			@return the symbol the dialog opens on, and the one the rows
			which were never worked on are created with: the symbol used
			last when the project holds terminals already -- which is the
			one the settings remember, and the one of the last terminal of
			the strip when they remember none -- and the default the
			button of the toolbar saved for a project which holds none
			(@sa onSaveDefaultSymbol). "(no symbol)" when there is nothing
			to take anything from.
		*/
		QString initialSymbol() const;
		/**
			@brief onTerminalsModeChanged
			@param checked : the radio button which just changed its state
			Enables the input the checked radio button names and disables the
			other one: @c m_number_sb with "Number of terminals", @c m_id_le
			with "ID's of terminals". The preview tree follows the mode which
			is active, so what it shows is always what the OK button would
			create (@sa parseIds).
		*/
		void onTerminalsModeChanged(bool checked);
		/**
			@brief onPreviewSelectionChanged
			Follows the selection of the preview tree in the button of the
			toolbar above it: that button has something to work on only
			while terminals are marked (@sa openTerminalsDialog).
		*/
		void onPreviewSelectionChanged();
		/// @brief onEditTerminalsTriggered
		/// Opens the dialog for the terminals marked in the preview tree
		/// (@sa openTerminalsDialog).
		void onEditTerminalsTriggered();
		QList<QTreeWidgetItem *> selectedPreviewChildren() const;
		void fillExistingTerminals();
		void refreshPreviewTree(const ParsedIds &parsed);
		void validate();
		/**
			@brief openTerminalsDialog
			Opens the dialog which works on the terminals marked in the
			preview tree (@sa selectedPreviewChildren), and writes what it
			chose back into those rows -- and only into those, since the
			rows carry the very properties the OK button reads
			(@sa propertiesOfRow).
		*/
		void openTerminalsDialog();

		QString effectiveInstallation() const;
		QString effectiveLocation() const;
		QString effectiveName() const;
		/// @return the name to show for a symbol location, read out of the
		/// list of symbols once and then kept -- an element name is read
		/// out of its XML, which is not worth doing twice.
		QString symbolName(const QString &symbol) const;
		/**
			@brief defaultsForNewTerminals
			@return the properties a terminal the dialog is never asked
			about is created with: the generic type and function, the
			automatic numbering locked and no other flag, -- and the
			symbol, the sizes and the color of the last terminal
			the strip of the current node holds, when there is one. Those
			are the very values the dialog of the marked terminals opens
			on, which is what keeps the two in step: what the user reads in
			the dialog is what the terminals get, whether he presses OK
			there or not.
			A strip which does not exist yet has no terminal to take
			anything from: its terminals are created with the generic
			values alone, and the user gives them the rest if and when he
			opens the dialog.
		*/
		TerminalProperties defaultsForNewTerminals() const;
		/**
			@brief propertiesOfRow
			@param id : the ID of a terminal to be created
			@return what the dialog gave that terminal -- the values the
			user chose for it (@sa m_properties) when he chose some, and
			the defaults of the moment otherwise
			(@sa defaultsForNewTerminals).
			The defaults are read here and not remembered when the row
			appears: which strip the dialog is about can still change, and
			a terminal which was never worked on keeps following the strip
			the user picks last.
		*/
		TerminalProperties propertiesOfRow(const QString &id) const;
		TerminalStrip *stripOfTheCurrentNode() const;
		/**
			@brief parseIds
			@return the IDs of the terminals the dialog is about to add,
			read out of the input which names them: the number of terminals
			when its radio button is checked (@sa generatedIds), the IDs
			typed when the other one is (@c m_id_le).
		*/
		ParsedIds parseIds() const;
		/**
			@brief generatedIds
			@return as many IDs as @c m_number_sb asks for, continuing the
			IDs of the strip behind the terminals it holds already: the
			first one is the number right behind the last terminal and the
			highest ID among them -- a terminal block holding the IDs 2, 3,
			5 and 7 goes on with 8, never with 5. An ID which is taken
			already (@sa takenIds) is stepped over and kept in
			ParsedIds::skipped.
		*/
		ParsedIds generatedIds() const;
		/**
			@brief takenIds
			@return every ID which is taken already for the terminal block
			the combo boxes define: the IDs the terminals of that strip
			carry, and the ones a free terminal of the project claims for
			that very name. The two cases are the ones
			checkIdsAgainstTheStrip() and checkIdsAgainstFreeTerminals()
			raise as errors for IDs typed by hand -- for IDs generated they
			are simply stepped over.
		*/
		QStringList takenIds() const;
		QStringList checkIdsAgainstTheStrip(const QStringList &ids) const;
		QStringList checkIdsAgainstFreeTerminals(const QStringList &ids) const;

	private:
		/// Marker of the first column of a preview row which shows a
		/// terminal the strip of the current node already holds: such a
		/// row is greyed out and display only -- it never reaches the OK
		/// button (@sa fillExistingTerminals, describedTerminals).
		static constexpr int ExistingTerminalRole{Qt::UserRole + 2};

		/// The number of terminals to add is a whole number between these
		/// two, zero being the value the dialog opens on: the same values
		/// the .ui file sets on @c m_number_sb, set again in the
		/// constructor so that the dialog and its code never disagree
		/// (@sa generatedIds).
		static constexpr int min_terminal_count{0};
		static constexpr int max_terminal_count{99};

		Ui::AddTerminalsDialog *ui;
		QETProject *m_project;
		/// Root item of the preview tree: the strip itself, whose children
		/// are the terminals of the ID field.
		QTreeWidgetItem *m_root_item = nullptr;
		/// Every terminal symbol of the project and of the standard
		/// collection, read once and asked for by the row of symbols and
		/// by the dialog which chooses a symbol in the collections: this
		/// dialog names the symbols its rows show with it, lists them in
		/// the combo box, and starts reading the standard collection in
		/// the background (@sa TerminalSymbolList).
		TerminalSymbolList *m_symbols = nullptr;
		/// What the dialog of the marked terminals gave them, by ID: what
		/// the user chose for those terminals, kept while the rows are
		/// rebuilt. An ID which is not in there is one he never worked on,
		/// and keeps the defaults of the moment (@sa propertiesOfRow).
		QHash<QString, TerminalProperties> m_properties;
		/// The symbol every row appeared with, by ID: what the row of
		/// symbols showed at that very moment, kept while the rows are
		/// rebuilt (@sa symbolOfRow). A row the user never worked on
		/// keeps this and never what the row of symbols shows afterwards.
		QHash<QString, QString> m_row_symbols;

		static const QString s_installation_default;
		static const QString s_location_default;
};

#endif // ADDTERMINALSDIALOG_H

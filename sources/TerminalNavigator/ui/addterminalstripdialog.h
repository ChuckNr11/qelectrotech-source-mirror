/*
	Copyright 2006-2026  The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file addterminalstripdialog.h
	@brief Header of the AddTerminalStripDialog class
*/
#ifndef ADDTERMINALSTRIPDIALOG_H
#define ADDTERMINALSTRIPDIALOG_H

#include <QDialog>
#include <QFutureWatcher>
#include <QHash>
#include <QList>

class ElementsLocation;
class QTreeWidgetItem;
class QETProject;
class TerminalStrip;
namespace Ui {
	class AddTerminalStripDialog;
}

/**
	@brief The AddTerminalStripDialog class
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
	ID, labelled 'strip name:ID', each one with a symbol of its own, because
	the OK button creates terminals which are not drawn in any folio yet, and
	only the model half of them.
	@see TerminalNavigatorWidget::createTerminalStrip, which turns those
	descriptions into real terminals.

	@c m_preview_tree shows those terminals while the user types: the strip
	as its root item spanning both columns, one child per ID, the ID in the
	first column and the name of the symbol it is drawn with in the second
	one. The location string of that symbol is kept in Qt::UserRole, which is
	where a later drag out of this tree reads it.

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
	which opens the dialog for the terminals the user marked. The button
	follows the selection of the tree, and a terminal the dialog is about
	to create is marked the moment its row appears -- what it creates is
	what its buttons work on (@sa onPreviewSelectionChanged,
	refreshPreviewTree).

	A symbol is picked for a terminal the way a text is changed in a list:
	the user selects the terminals it belongs to -- one, several or all of
	them, with the mouse or the keyboard -- and picks the symbol in
	@c m_symbol_cb, which applies it to the selected terminals only. Picking
	a symbol with nothing selected does not touch the terminals already
	listed: the value becomes the one the terminals typed next get. Never
	silently changing what a terminal carries is the whole point, since the
	second column shows that value, and the user trusts what he reads there.
*/
class AddTerminalStripDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit AddTerminalStripDialog(QETProject *project, QWidget *parent = nullptr);
		~AddTerminalStripDialog() override;

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
		/// field, and the location string of the symbol it should be drawn
		/// with -- an empty one when the user gave it none, the terminal
		/// then only waiting for a symbol of its own.
	struct TerminalDescription {
		QString id;
		QString symbol;
	};

		/// @return the terminals this dialog describes, one per ID the OK
		/// button is about to create (@sa parseIds) and in the order of the
		/// preview tree, each with the symbol the user gave it. Nothing
		/// when the input naming them can not be read at all -- which the
		/// dialog refuses as an error anyway.
		/// The rows of the terminals the strip holds already are display
		/// only and are left out: they exist, the OK button must not
		/// create them a second time.
	QList<TerminalDescription> describedTerminals() const;

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

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
		void fillSymbolCombo();
		void appendSymbolItem(const ElementsLocation &location);
		void onSymbolScanFinished();
		/// @brief onSymbolPicked
		/// Applies the symbol of the combo box to the terminals selected in
		/// the preview tree, or, with nothing selected, remembers it as the
		/// symbol the terminals typed next are created with.
		void onSymbolPicked(int index);
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
			Follows the selection of the preview tree in the symbol combo
			box and in the button of the toolbar above it: that button has
			something to work on only while terminals are marked
			(@sa openTerminalsDialog).
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
			preview tree (@sa selectedPreviewChildren). The dialog itself
			is defined elsewhere and later -- this is the place it opens,
			and what it is given to work on.
		*/
		void openTerminalsDialog();

		QString effectiveInstallation() const;
		QString effectiveLocation() const;
		QString effectiveName() const;
		/// @return the location string the symbol combo box currently
		/// shows, or an empty string when it shows none.
		QString symbolLocation() const;
		/// @return the name to show for a symbol location, read from the
		/// combo box when the symbol is listed there -- an element name is
		/// read out of its XML, which is not worth doing twice.
		QString symbolName(const QString &symbol) const;
		QString preselectedSymbolOfTheProject() const;
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

		Ui::AddTerminalStripDialog *ui;
		QETProject *m_project;
		/// Root item of the preview tree: the strip itself, whose children
		/// are the terminals of the ID field.
		QTreeWidgetItem *m_root_item = nullptr;
		/// The symbol list is read on the first popup of its combo box only:
		/// the standard collection holds more than a thousand elements.
		bool m_symbol_scan_started = false;
		/// Fills the symbols of the standard collection, in the background.
		QFutureWatcher<QStringList> *m_symbol_watcher = nullptr;
		/// Symbol the terminals typed next are created with. The combo box
		/// only writes to the terminals the user selected, so its current
		/// value has to be remembered for the ones which do not exist yet.
		QString m_default_symbol;
		/// Names of the symbols already named once, so that a column of the
		/// preview tree never reads the same element XML twice.
		mutable QHash<QString, QString> m_symbol_names;

		static const QString s_installation_default;
		static const QString s_location_default;
};

#endif // ADDTERMINALSTRIPDIALOG_H

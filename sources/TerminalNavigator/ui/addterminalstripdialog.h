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
	The three first levels are gathered by the (editable) combo boxes, the
	strip name by @c m_name_cb, and the ID of the terminals to add by
	@c m_id_le.

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

		/// @return the terminals this dialog describes, one per ID of the
		/// ID field and in the order of the preview tree, each with the
		/// symbol the user gave it. Nothing when that field can not be
		/// read at all -- which the dialog refuses as an error anyway.
	QList<TerminalDescription> describedTerminals() const;

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

	private:
		/// Result of the reading of the ID field: either a list of IDs to add
		/// to the terminal block, or a message explaining why the list could
		/// not be read at all.
		struct ParsedIds {
			QStringList ids;
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
		void onPreviewSelectionChanged();
		QList<QTreeWidgetItem *> selectedPreviewChildren() const;
		void refreshPreviewTree(const ParsedIds &parsed);
		void validate();

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
		ParsedIds parseIds() const;
		QStringList checkIdsAgainstTheStrip(const QStringList &ids) const;
		QStringList checkIdsAgainstFreeTerminals(const QStringList &ids) const;

	private:
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

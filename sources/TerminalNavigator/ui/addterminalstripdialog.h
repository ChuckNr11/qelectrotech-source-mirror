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
	ID, labelled 'strip name:ID', and all drawn with the symbol picked in
	@c m_symbol_cb -- once they are, because the OK button creates terminals
	which are not drawn in any folio yet, and only the model half of them.
	@see TerminalNavigatorWidget::createTerminalStrip, which turns those
	descriptions into real terminals.

	@c m_preview_tree shows those terminals while the user types: the strip
	as its root item, one child per ID. The children carry the symbol in
	Qt::UserRole, which is where a later drag out of this tree reads it.
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
		/// @return the IDs of the terminals described by this dialog, in
		/// the order of the ID field, or nothing when that field can not
		/// be read at all.
		QStringList terminalIds() const;
		/// @return the location string of the symbol the terminals are to
		/// be drawn with, or an empty string when the user picked none.
		QString symbolLocation() const;

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
		void refreshPreviewTree(const ParsedIds &parsed);
		void validate();

		QString effectiveInstallation() const;
		QString effectiveLocation() const;
		QString effectiveName() const;
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

		static const QString s_installation_default;
		static const QString s_location_default;
};

#endif // ADDTERMINALSTRIPDIALOG_H

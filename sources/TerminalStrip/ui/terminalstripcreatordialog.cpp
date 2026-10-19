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
#include "terminalstripcreatordialog.h"
#include "ui_terminalstripcreatordialog.h"
#include "../terminalstrip.h"
#include "../../qetproject.h"

#include "../../qet.h"
#include "../../utils/qetutils.h"

#include <algorithm>
/**
 * @brief TerminalStripCreatorDialog::TerminalStripCreatorDialog
 * @param project : Project to add a new terminal strip
 * @param parent : parent widget
 */
TerminalStripCreatorDialog::TerminalStripCreatorDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::TerminalStripCreatorDialog),
	m_project(project)
{
	ui->setupUi(this);
	fillWithExistingValues();
	QET::trackDialogGeometry(this);
}

/**
 * @brief TerminalStripCreatorDialog::~TerminalStripCreatorDialog
 */
TerminalStripCreatorDialog::~TerminalStripCreatorDialog() {
	delete ui;
}

/**
 * @brief TerminalStripCreatorDialog::setInstallation
 * Set the installation field string. @p installation does not have to be
 * one the combo box already offers: a value the project does not know yet
 * is accepted as it is typed.
 * @param installation
 */
void TerminalStripCreatorDialog::setInstallation(const QString &installation) {
	ui->m_installation_cb->setCurrentText(installation);
	setCursorToEmptyLine();
}

/**
 * @brief TerminalStripCreatorDialog::setLocation
 * Set the location field string. @p location does not have to be one the
 * combo box already offers: a value the project does not know yet is
 * accepted as it is typed.
 * @param location
 */
void TerminalStripCreatorDialog::setLocation(const QString &location) {
	ui->m_location_cb->setCurrentText(location);
	setCursorToEmptyLine();
}

/**
 * @brief TerminalStripCreatorDialog::generatedTerminalStrip
 * @return A new terminal Strip according to the value set by user.
 * The terminal strip is already added to the terminalStrip list of the project
 * so it's ready to use.
 */
TerminalStrip *TerminalStripCreatorDialog::generatedTerminalStrip() const
{
	QString installation_ = ui->m_installation_cb->currentText();
	QString location_     = ui->m_location_cb->currentText();
	QString name_         = ui->m_name_le->text();

	if (installation_.isEmpty()) {
		installation_ = QStringLiteral("=INST"); }
	if (location_.isEmpty()) {
		location_ = QStringLiteral("+LOC"); }
	if (name_.isEmpty()) {
		name_ = QStringLiteral("X"); }

	auto strip = m_project->newTerminalStrip(installation_,
											 location_,
											 name_);
	strip->setComment(ui->m_comment_le->text());
	strip->setDescription(ui->m_description_te->toPlainText());

	return strip;
}

/**
 * @brief TerminalStripCreatorDialog::setCursorToEmptyLine
 * Set the cursor to the first empty field.
 * It's useful when user creates a new terminal strip
 * with some value prefilled, to increase productivity.
 */
void TerminalStripCreatorDialog::setCursorToEmptyLine()
{
	if (ui->m_installation_cb->currentText().isEmpty()) {
		return;
	}
	if (ui->m_location_cb->currentText().isEmpty()) {
		ui->m_location_cb->setFocus();
		return;
	}
	ui->m_name_le->setFocus();
}

/**
 * @brief TerminalStripCreatorDialog::fillWithExistingValues
 * Offer the plants and locations the strips of the project already use,
 * so the same one is picked instead of typed a second time -- the
 * navigator groups by those two values, and two look-alike plants are
 * two branches which are hard to tell apart afterwards.
 *
 * Sorted naturally (Cabinet 2 before Cabinet 10), each value once, and a
 * project without any strip simply leaves the two combo boxes empty:
 * they still work as the plain text fields they were.
 */
void TerminalStripCreatorDialog::fillWithExistingValues()
{
	if (!m_project) {
		return;
	}

	QStringList installations_;
	QStringList locations_;
	for (auto *strip : m_project->terminalStrip())
	{
		if (!strip) {
			continue;
		}
		const auto installation_ = strip->installation();
		const auto location_ = strip->location();
		if (!installation_.isEmpty() && !installations_.contains(installation_)) {
			installations_.append(installation_);
		}
		if (!location_.isEmpty() && !locations_.contains(location_)) {
			locations_.append(location_);
		}
	}

	const auto natural_order_ = [](const QString &a, const QString &b) {
		return QETUtils::naturalLessThan(a, b);
	};
	std::sort(installations_.begin(), installations_.end(), natural_order_);
	std::sort(locations_.begin(), locations_.end(), natural_order_);

	ui->m_installation_cb->addItems(installations_);
	ui->m_location_cb->addItems(locations_);
}

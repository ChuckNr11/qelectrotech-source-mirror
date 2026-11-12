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
#include "changeelementinformationcommand.h"
#include <qetproject.h>
#include <diagram.h>
#include <qetgraphicsitem/element.h>

#include <QObject>

/**
	@brief ChangeElementInformationCommand::ChangeElementInformationCommand
	Default constructor
	@param elmt : element to change information
	@param old_info : old info of element
	@param new_info : new info of element
	@param parent
*/
ChangeElementInformationCommand::ChangeElementInformationCommand(
		Element *elmt,
		const DiagramContext &old_info,
		const DiagramContext &new_info,
		QUndoCommand *parent) :
	QUndoCommand (parent)
{
	m_map.insert(QPointer<Element>(elmt), qMakePair(old_info, new_info));
	setText(QObject::tr("Edit information of the element : %1")
			.arg(elmt -> name()));
}

ChangeElementInformationCommand::ChangeElementInformationCommand(QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext> > map,
																 QUndoCommand *parent) :
	QUndoCommand(parent),
	m_map(map)
{
	setText(QObject::tr("Edit information for multiple items"));
}

bool ChangeElementInformationCommand::mergeWith(const QUndoCommand *other)
{
	if (id() != other->id())
		return false;

	ChangeElementInformationCommand const *other_undo = static_cast<const ChangeElementInformationCommand*>(other);

		//A row this command moved out of its strip is remembered here
		//and given back by undo(); merging would keep the move standing
		//while the merged command forgets it, so a change which moves a
		//row stays the two steps it was typed as.
	if (!m_rehomes.isEmpty() || !other_undo->m_rehomes.isEmpty()) {
		return false;
	}

		//In case of other undo_undo have the same elements as keys
	if (m_map.size() == other_undo->m_map.size())
	{
		for (auto key : other_undo->m_map.keys()) {
			if (!m_map.contains(key)) {
				return false;
			}
		}

			//Other_undo will be merged with this undo :
			//Replace the new_info values of this m_map
			//by the new_info values of other_undo's m_map
		for (auto key : other_undo->m_map.keys())
		{
			m_map.insert(key,
						 qMakePair(
							 m_map.value(key).first,
							 other_undo->m_map.value(key).second));
		}
		return true;
	}
	else {
		return false;
	}
}

/**
	@brief ChangeElementInformationCommand::undo
*/
void ChangeElementInformationCommand::undo()
{
	unhome();
	for (auto element : m_map.keys()) {
		if (element) {
			element->setElementInformations(m_map.value(element).first);
			if (m_unlinked_reports.contains(element)) {
				Element *partner = m_unlinked_reports.value(element).data();
				if (partner) {
					element->linkToElement(partner);
				}
			}
		}
	}
	updateProjectDB();
}

/**
	@brief ChangeElementInformationCommand::redo
*/
void ChangeElementInformationCommand::redo()
{
		//The labels before the change, so that only a name the user
		//really changed moves a row out of its strip
		//(@sa TerminalStripRehome).
	QVector<QPair<Element *, QString>> labels;
	for (const auto &element : m_map.keys()) {
		labels << qMakePair(element.data(), TerminalStripRehome::labelOf(element.data()));
	}

	for (auto element : m_map.keys()) {
		if (element) {
			if (element->linkType() & Element::AllReport) {
				bool label_changed = m_map.value(element).first.value("label").toString() !=
				                     m_map.value(element).second.value("label").toString();
				if (label_changed && element->diagram() && element->diagram()->project() &&
				    element->diagram()->project()->chainedReports()) {
					if (!element->isFree()) {
						Element *partner = element->jumpTarget();
						if (partner) {
							m_unlinked_reports.insert(element, partner);
						}
						element->unlinkAllElements();
					}
				}
			}
			element->setElementInformations(m_map.value(element).second);
		}
	}
	updateProjectDB();
	rehome(labels);
}

/**
	@brief ChangeElementInformationCommand::rehome
	@param labels : the label of each element before the change
	What the new names do to the rows of the terminals this command
	names (@sa TerminalStripRehome): a name calling for another strip
	takes its row there, one no strip answers to takes it out of its
	strip, and the rows are remembered for undo (@sa unhome).
*/
void ChangeElementInformationCommand::rehome(const QVector<QPair<Element *, QString>> &labels)
{
	m_rehomes.clear();
	for (const auto &label_ : labels)
	{
		const auto plan = TerminalStripRehome::planFor(label_.first, label_.second);
		if (plan.isEmpty()) {
			continue;
		}
		TerminalStripRehome::apply(plan);
		m_rehomes << plan;
	}
}

/**
	@brief ChangeElementInformationCommand::unhome
	The undo of rehome(): every row goes back to the strip this command
	took it from, before the names themselves go back.
*/
void ChangeElementInformationCommand::unhome()
{
	for (auto it = m_rehomes.crbegin() ; it != m_rehomes.crend() ; ++it) {
		TerminalStripRehome::revert(*it);
	}
	m_rehomes.clear();
}

void ChangeElementInformationCommand::updateProjectDB()
{
	auto elmt = m_map.keys().first().data();
	if(elmt && elmt->diagram())
	{
			//need to have a list of element instead of QPointer<Element>
			//for the function elementInfoChange of the database
		QList<Element *> list_;
		for (auto p_elmt : m_map.keys())
			list_ << p_elmt.data();

		elmt->diagram()->project()->dataBase()->elementInfoChanged(list_);
	}
}

ChangeStoredElementInformationCommand::ChangeStoredElementInformationCommand(
		QETProject *project, const QUuid &element, const DiagramContext &old_info,
		const DiagramContext &new_info, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_element(element),
	m_old_info(old_info),
	m_new_info(new_info)
{
	setText(QObject::tr("Change the information of the element"));
}

void ChangeStoredElementInformationCommand::undo()
{
	if (m_project)
		m_project->dataBase()->setPlacedElementInformation(m_element, m_old_info);
}

void ChangeStoredElementInformationCommand::redo()
{
	if (m_project)
		m_project->dataBase()->setPlacedElementInformation(m_element, m_new_info);
}

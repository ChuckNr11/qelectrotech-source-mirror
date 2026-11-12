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
#ifndef CHANGEELEMENTINFORMATIONCOMMAND_H
#define CHANGEELEMENTINFORMATIONCOMMAND_H

#include <TerminalStrip/terminalstriprehome.h>
#include <diagramcontext.h>

#include <QPair>
#include <QPointer>
#include <QString>
#include <QUndoCommand>
#include <QUuid>
#include <QVector>

class Element;
class QETProject;

/**
	@brief The ChangeElementInformationCommand class
	This class manage undo/redo to change the element information.

	What the new information names a terminal does to its row in the
	terminal strips goes with it in the same undo step: a name calling
	for another strip moves the row there, one no strip answers to makes
	the terminal independent (@sa TerminalStripRehome). Undo gives the
	rows their home back along with the information.
*/
class ChangeElementInformationCommand : public QUndoCommand
{
	public:
		ChangeElementInformationCommand(
				Element *elmt,
				const DiagramContext &old_info,
				const DiagramContext &new_info,
				QUndoCommand *parent = nullptr);

		ChangeElementInformationCommand(QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> map,
										QUndoCommand *parent = nullptr);

		int id() const override {return 1;}
		bool mergeWith(const QUndoCommand *other) override;
		void undo() override;
		void redo() override;

	private:
		void updateProjectDB();
		void rehome(const QVector<QPair<Element *, QString>> &labels);
		void unhome();

	private:
		QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> m_map;
		QMap<QPointer<Element>, QPointer<Element>> m_unlinked_reports;
		/// The rows this command moved out of their strip, given back
		/// by undo() (@sa TerminalStripRehome)
		QVector<TerminalStripRehome::Plan> m_rehomes;
};

/**
	@brief The ChangeStoredElementInformationCommand class
	Change the information of a symbol given by its uuid, built or not
	(lazy folios): through the symbol when it is built, through the
	project database when it is not (projectDataBase::canEditUnbuilt()).
*/
class ChangeStoredElementInformationCommand : public QUndoCommand
{
	public:
		ChangeStoredElementInformationCommand(QETProject *project, const QUuid &element,
											  const DiagramContext &old_info,
											  const DiagramContext &new_info,
											  QUndoCommand *parent = nullptr);
		void undo() override;
		void redo() override;

	private:
		QPointer<QETProject> m_project;
		QUuid m_element;
		DiagramContext m_old_info, m_new_info;
};

#endif // CHANGEELEMENTINFORMATIONCOMMAND_H

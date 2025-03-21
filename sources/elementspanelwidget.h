/*
	Copyright 2006-2025 The QElectroTech Team
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
#ifndef ELEMENTS_PANEL_WIDGET_H
#define ELEMENTS_PANEL_WIDGET_H

#include "elementspanel.h"

/**
	@brief The ElementsPanelWidget class
	This class embeds an elements panel under a toolbar
	providing various actions to manage elements.
	@see ElementsPanel
*/
class ElementsPanelWidget : public QWidget {
	Q_OBJECT
	
	// constructors, destructor
	public:
	ElementsPanelWidget(QWidget * = nullptr);
	~ElementsPanelWidget() override;
	
	private:
	ElementsPanelWidget(const ElementsPanelWidget &);
	
	// attributes
	private:
	ElementsPanel *elements_panel;
	QAction *open_directory, *copy_path;
	QAction *prj_activate,
		*prj_close,
		*prj_edit_prop,
		*prj_prop_diagram,
		*prj_add_diagram,
		*prj_del_diagram,
		*prj_move_diagram_up,
		*prj_move_diagram_top,
		*prj_move_diagram_down,
		*prj_move_diagram_upx10,
		*prj_move_diagram_upx100,
		*prj_move_diagram_downx10,
		*prj_move_diagram_downx100;
	QAction *tbt_add, *tbt_edit, *tbt_remove;
	QMenu *context_menu;
	QLineEdit *filter_textfield;
	
	// methods
	public:
	inline ElementsPanel &elementsPanel() const;
	
	signals:
	void requestForProject(QETProject *);
	void requestForNewDiagram(QETProject *);
	void requestForProjectClosing(QETProject *);
	void requestForProjectPropertiesEdition(QETProject *);
	void requestForDiagramPropertiesEdition(Diagram *);
	void requestForDiagramDeletion(Diagram *);
	void requestForDiagramMoveUp(Diagram *);
	void requestForDiagramMoveDown(Diagram *);
	void requestForDiagramMoveUpTop(Diagram *);
	void requestForDiagramMoveUpx10(Diagram *);
	void requestForDiagramMoveUpx100(Diagram *);
	void requestForDiagramMoveDownx10(Diagram *);
	void requestForDiagramMoveDownx100(Diagram *);
	
	public slots:
	void openDirectoryForSelectedItem();
	void copyPathForSelectedItem();
	void reloadAndFilter();
	void activateProject();
	void closeProject();
	void editProjectProperties();
	void editDiagramProperties();
	void newDiagram();
	void deleteDiagram();
	void moveDiagramUp();
	void moveDiagramDown();
	void moveDiagramUpTop();
	void moveDiagramUpx10();
	void moveDiagramUpx100();
	void moveDiagramDownx10();
	void moveDiagramDownx100();
	void addTitleBlockTemplate();
	void editTitleBlockTemplate();
	void removeTitleBlockTemplate();
	void updateButtons();
	void handleContextMenu(const QPoint &);
	void filterEdited(const QString &);

	protected:
	void keyPressEvent (QKeyEvent *e) override;
	
	private:
	QString previous_filter_;
};

/**
	@brief ElementsPanelWidget::elementsPanel
	@return The elements panel embedded within this widget.
*/
inline ElementsPanel &ElementsPanelWidget::elementsPanel() const
{
	return(*elements_panel);
}

#endif

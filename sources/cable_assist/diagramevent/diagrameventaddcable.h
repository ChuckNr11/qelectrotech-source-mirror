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
#ifndef DIAGRAMEVENTADDCABLE_H
#define DIAGRAMEVENTADDCABLE_H

#include "sources/diagramevent/diagrameventinterface.h"
#include "qgraphicsitem.h"
#include "sources/qetgraphicsitem/conductor.h"
#include "sources/qetgraphicsitem/diagramtextitem.h"
#include "sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "sources/qetgraphicsitem/independenttextitem.h"
#include "sources/qetgraphicsitem/qetshapeitem.h"

/**
	@brief The DiagramEventAddShape class
	This event manage the creation of a cable.
*/
class DiagramEventAddCable : public DiagramEventInterface
{
	Q_OBJECT

  public:
	DiagramEventAddCable(Diagram *diagram);

	~DiagramEventAddCable() override;

	void mousePressEvent       (QGraphicsSceneMouseEvent *event) override;
	void mouseMoveEvent        (QGraphicsSceneMouseEvent *event) override;
	void mouseReleaseEvent     (QGraphicsSceneMouseEvent *event) override;
	void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event) override;
	void hoverEnterEvent	   (QGraphicsSceneHoverEvent *event) override;
	void hoverLeaveEvent	   (QGraphicsSceneHoverEvent *event) override;
	void keyPressEvent		   (QKeyEvent *event) override;
	void keyReleaseEvent	   (QKeyEvent *event) override;
	void init() override;

  private:
	void updateHelpCross (const QPointF &p);
	void drawSelectionLine (const QPointF &p);
	void getSelectedConductors();
	void makeCable();
	QETDiagramEditor* diagramEditor() const;

  protected:
	QString m_integrate_path;
	Conductor *m_last_cond=nullptr;
	QGraphicsLineItem
		*m_help_horiz,
		*m_help_verti,
		*m_selection_line;

	QGraphicsRectItem
		*m_collide_rect;

	QPointF
		m_selection_start_pos,
		m_last_cond_pos;

	QetShapeItem *m_cable_line;
	IndependentTextItem *m_cabel_label;
	QetShapeItem::ShapeType	 m_shapeType = QetShapeItem::Line;
	QGraphicsItemGroup *m_group;

	bool	m_start_selection;

	QList<Conductor *> m_cond_list;
	QList<QPointF> m_cond_pos;
	QStringList *m_cable_data;
	QStringList m_cable_data_;
};

#endif // DIAGRAMEVENTADDCABLE_H

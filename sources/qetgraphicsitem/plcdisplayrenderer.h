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
#ifndef PLCDISPLAYRENDERER_H
#define PLCDISPLAYRENDERER_H

#include "properties/elementdata.h"

#include <QPointF>
#include <QSizeF>

class QPainter;

/**
	@brief The PlcDisplayRenderer namespace

	Draws a PLC master's IO data as the module box or as the rack
	overview, the two displays beside the plain table which
	Element::drawPlcTable and PartPlcTable draw themselves. Both the
	sheet and the element editor's preview draw through here, so what
	the editor shows is what the sheet gets.

	The terminal stubs these displays draw are marks for the symbol's
	own terminals: the connection points stay terminals of the symbol,
	placed in the element editor like any other.
*/
namespace PlcDisplayRenderer
{
		/// Size the module box needs, in mm
	QSizeF boxSize(const ElementData::PlcMasterData &data);
		/// Size the rack overview needs, in mm
	QSizeF rackSize(const ElementData::PlcMasterData &data);

		/// Draw the module box with its top left corner at \p anchor.
		/// \p cross_refs holds one entry per IO row of \p data.
	void drawBox(QPainter *painter, const ElementData::PlcMasterData &data,
				 const QPointF &anchor, const QStringList &cross_refs);
		/// Draw the rack overview with its top left corner at \p anchor.
	void drawRack(QPainter *painter, const ElementData::PlcMasterData &data,
				  const QPointF &anchor);

		/**
		 * Expand the placeholders of \p tmpl for one IO row:
		 * %address, %function, %comment, %type, %label (the row's
		 * terminals), %xref, %rang (the row number), %module (the row's
		 * module label) and %slot (the row's module number).
		 * %% is a literal percent sign.
		 */
	QString expand(const QString &tmpl, const ElementData::PlcIO &io, int index,
				   const QString &module_label, const QString &cross_ref);
}

#endif // PLCDISPLAYRENDERER_H

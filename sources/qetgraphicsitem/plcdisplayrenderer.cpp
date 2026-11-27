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
#include "plcdisplayrenderer.h"

#include <qetapp.h>

#include <QPainter>

#include <algorithm>

namespace {

qreal rowHeight(const ElementData::PlcMasterData &data)
{
	return data.rowHeight > 0 ? data.rowHeight : 8.0;
}

bool isOutput(ElementData::PlcIOType type)
{
	return type == ElementData::SortieDigitale
		|| type == ElementData::SortieAnalogique
		|| type == ElementData::SortieUniverselle;
}

	/**
		The IO rows a display draws: every row but the ones marked as
		an unused channel, which are kept in the data and left out of
		the drawing, in the order they were entered.
	*/
QList<int> visibleRows(const ElementData::PlcMasterData &data)
{
	QList<int> rows;
	for (int i = 0; i < data.ios.size(); ++i) {
		if (!data.ios.at(i).hidden) {
			rows.append(i);
		}
	}
	return rows;
}

	/**
		The modules of the rack overview, in slot order: the distinct
		module numbers of the rows which belong to one.
	*/
QList<int> moduleIndices(const ElementData::PlcMasterData &data)
{
	QList<int> modules;
	for (int row : visibleRows(data)) {
		const int module = data.ios.at(row).moduleIndex;
		if (module >= 0 && !modules.contains(module)) {
			modules.append(module);
		}
	}
	std::sort(modules.begin(), modules.end());
	return modules;
}

	/**
		The name a module is drawn with: its label if it has one,
		and its number otherwise.
	*/
QString moduleLabelFor(const ElementData::PlcMasterData &data, int module)
{
	if (module < 0) {
		return QString();
	}
	const QString label = data.moduleLabels.value(module);
	if (!label.isEmpty()) {
		return label;
	}
	return QObject::tr("Module %1").arg(module);
}

QFont headerFont(const ElementData::PlcMasterData &data)
{
	QFont font = data.headerFont;
	if (font.family().isEmpty()) {
		font = QETApp::diagramTextsFont();
		font.setBold(true);
	}
	return font;
}

QFont cellFont(const ElementData::PlcMasterData &data)
{
	QFont font = data.cellFont;
	if (font.family().isEmpty()) {
		font = QETApp::diagramTextsFont();
	}
	return font;
}

	/**
		One IO row's text and the side its terminal stub is drawn on.
	*/
QString rowText(const ElementData::PlcMasterData &data, int row,
				const QString &cross_ref)
{
	const auto &io = data.ios.at(row);
	QString tmpl = data.box_settings.rowTemplate;
	if (tmpl.isEmpty()) {
		tmpl = QStringLiteral("%label  %address  %function");
	}
	return PlcDisplayRenderer::expand(tmpl, io, row,
		moduleLabelFor(data, io.moduleIndex), cross_ref);
}

QString rowSide(const ElementData::PlcMasterData &data, int row)
{
	const auto &io = data.ios.at(row);
	if (!io.side.isEmpty()) {
		return io.side;
	}
	if (data.box_settings.splitSides && isOutput(io.type)) {
		return QStringLiteral("right");
	}
	return QStringLiteral("left");
}

	/**
		The address range of one module: the address of its first row
		up to the address of its last one.
	*/
QString addressRange(const ElementData::PlcMasterData &data, int module)
{
	QString first;
	QString last;
	for (int row : visibleRows(data)) {
		const auto &io = data.ios.at(row);
		if (io.moduleIndex != module || io.address.isEmpty()) {
			continue;
		}
		if (first.isEmpty()) {
			first = io.address;
		}
		last = io.address;
	}
	if (first.isEmpty()) {
		return QString();
	}
	if (first == last) {
		return first;
	}
	return first + QStringLiteral(" – ") + last;
}

	/// The rack overview's title, already expanded; empty when there
	/// is nothing to title the frame with
QString rackTitle(const ElementData::PlcMasterData &data)
{
	const QList<int> modules = moduleIndices(data);
	if (modules.isEmpty()) {
		return QString();
	}
	const QList<int> rows = visibleRows(data);
	const int first_module = modules.first();
	QString tmpl = data.rack_settings.titleTemplate;
	if (tmpl.isEmpty()) {
		tmpl = QStringLiteral("%module");
	}
	int first_row = -1;
	for (int row : rows) {
		if (data.ios.at(row).moduleIndex == first_module) {
			first_row = row;
			break;
		}
	}
	const QString label = modules.size() == 1
		? moduleLabelFor(data, first_module) : QString();
	return PlcDisplayRenderer::expand(tmpl, data.ios.at(qMax(first_row, 0)),
		qMax(first_row, 0), label, QString());
}

	/// A terminal stub: a short mark crossing the box border
void drawStub(QPainter *painter, const QPointF &at, const QString &side)
{
	const qreal dx = side == QLatin1String("right") ? 2.0 : -2.0;
	painter->drawLine(QPointF(at.x() - dx, at.y()), QPointF(at.x() + dx, at.y()));
}

}

/**
	@brief PlcDisplayRenderer::expand
	Expand the placeholders of \p tmpl for the IO row \p index of
	\p data: %address, %function, %comment, %type, %label, %xref,
	%rang, %module and %slot, where \p module_label is the name of
	the row's module and \p cross_ref what the row's slave is drawn
	with. %% is a literal percent sign.
*/
QString PlcDisplayRenderer::expand(const QString &tmpl,
								   const ElementData::PlcIO &io, int index,
								   const QString &module_label,
								   const QString &cross_ref)
{
	QString out = tmpl;
	out.replace(QStringLiteral("%%"), QStringLiteral("\x01"));
	out.replace(QStringLiteral("%address"), io.address);
	out.replace(QStringLiteral("%function"), io.functionText);
	out.replace(QStringLiteral("%comment"), io.comment);
	out.replace(QStringLiteral("%type"),
				ElementData::translatedPlcIOType(io.type));
	out.replace(QStringLiteral("%label"),
				io.effectiveTerminals().join(QStringLiteral(", ")));
	out.replace(QStringLiteral("%xref"), cross_ref);
	out.replace(QStringLiteral("%rang"), QString::number(index + 1));
	out.replace(QStringLiteral("%module"), module_label);
	out.replace(QStringLiteral("%slot"),
				io.moduleIndex >= 0 ? QString::number(io.moduleIndex)
									: QString());
	out.replace(QStringLiteral("\x01"), QStringLiteral("%"));
	return out;
}

/**
	@brief PlcDisplayRenderer::boxSize
	The size the module box needs: the title band, one row per
	visible IO and, if it is shown, the supply row.
*/
QSizeF PlcDisplayRenderer::boxSize(const ElementData::PlcMasterData &data)
{
	const auto &box = data.box_settings;
	qreal height = qMax(box.headerHeight, 0.0)
		+ visibleRows(data).size() * rowHeight(data);
	if (box.showSupplyRow) {
		height += rowHeight(data);
	}
	return QSizeF(qMax(box.width, 10.0), height);
}

/**
	@brief PlcDisplayRenderer::drawBox
	Draw the module box with its top left corner at \p anchor: a
	frame with the title band on top, one row per visible IO -- each
	with its terminal stub on the left or the right border -- and the
	supply row at the bottom.
*/
void PlcDisplayRenderer::drawBox(QPainter *painter,
								 const ElementData::PlcMasterData &data,
								 const QPointF &anchor,
								 const QStringList &cross_refs)
{
	if (!painter) {
		return;
	}

	const auto &box = data.box_settings;
	const QSizeF size = boxSize(data);
	const QRectF frame(anchor, size);
	const qreal rh = rowHeight(data);
	const QList<int> rows = visibleRows(data);
	const QPen border_pen(Qt::black, 0.5);

	painter->save();
	painter->setPen(border_pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawRect(frame);

		// Title band
	const QRectF header(anchor, QSizeF(size.width(), box.headerHeight));
	if (box.headerHeight > 0) {
		painter->save();
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(220, 220, 220));
		painter->drawRect(header);
		painter->restore();

		painter->setPen(border_pen);
		painter->drawLine(header.bottomLeft(), header.bottomRight());

		QString title;
		if (!rows.isEmpty()) {
			const auto &first = data.ios.at(rows.first());
			QString tmpl = box.titleTemplate;
			if (tmpl.isEmpty()) {
				tmpl = QStringLiteral("%module");
			}
			title = expand(tmpl, first, rows.first(),
				moduleLabelFor(data, first.moduleIndex),
				cross_refs.value(rows.first()));
		}
		painter->setFont(headerFont(data));
		painter->drawText(header.adjusted(1, 0, -1, 0), Qt::AlignCenter, title);
	}

		// IO rows
	painter->setFont(cellFont(data));
	qreal y = anchor.y() + box.headerHeight;
	for (int row : rows) {
		const QRectF row_rect(anchor.x(), y, size.width(), rh);
		if (y > anchor.y() + box.headerHeight) {
			painter->setPen(border_pen);
			painter->drawLine(row_rect.topLeft(), row_rect.topRight());
		}

		const QString side = rowSide(data, row);
		drawStub(painter, QPointF(side == QLatin1String("right")
			? row_rect.right() : row_rect.left(), y + rh / 2.0), side);

		const QRectF text_rect = side == QLatin1String("right")
			? row_rect.adjusted(1, 0, -3, 0)
			: row_rect.adjusted(3, 0, -1, 0);
		painter->save();
		painter->setClipRect(text_rect, Qt::IntersectClip);
		painter->drawText(text_rect,
			(side == QLatin1String("right") ? Qt::AlignRight : Qt::AlignLeft)
				| Qt::AlignVCenter | Qt::TextWordWrap,
			rowText(data, row, cross_refs.value(row)));
		painter->restore();
		y += rh;
	}

		// Supply row
	if (box.showSupplyRow) {
		const QRectF supply(anchor.x(), y, size.width(), rh);
		painter->setPen(border_pen);
		painter->drawLine(supply.topLeft(), supply.topRight());
		drawStub(painter, QPointF(supply.left(), y + rh / 2.0),
				 QStringLiteral("left"));
		drawStub(painter, QPointF(supply.right(), y + rh / 2.0),
				 QStringLiteral("right"));
		QString label = box.supplyLabel;
		if (label.isEmpty()) {
			label = QObject::tr("L+   M   FE", "PLC box supply terminals");
		}
		painter->drawText(supply.adjusted(3, 0, -3, 0),
						  Qt::AlignCenter | Qt::TextWordWrap, label);
	}

	painter->restore();
}

/**
	@brief PlcDisplayRenderer::rackSize
	The size the rack overview needs: the title band, the frame's
	padding and one plate per module.
*/
QSizeF PlcDisplayRenderer::rackSize(const ElementData::PlcMasterData &data)
{
	const auto &rack = data.rack_settings;
	const qreal pad = 4.0;
	const qreal plate_w = qMax(rack.slotWidth, 10.0);
	const qreal plate_h = qMax(rack.slotHeight, 10.0);
	const int modules = moduleIndices(data).size();
	const qreal width = 2 * pad + qMax(modules, 1) * plate_w;
	const qreal title_h = rackTitle(data).isEmpty()
		? 0.0 : rowHeight(data) + 2.0;
	const qreal height = title_h + 2 * pad + plate_h;
	return QSizeF(width, height);
}

/**
	@brief PlcDisplayRenderer::drawRack
	Draw the one-line rack overview with its top left corner at
	\p anchor: a frame titled at the top, with one plate per module
	in slot order. The plates carry the module's name and, if it is
	shown, its address range; the channels themselves are left to the
	slaves on the folios.
*/
void PlcDisplayRenderer::drawRack(QPainter *painter,
								  const ElementData::PlcMasterData &data,
								  const QPointF &anchor)
{
	if (!painter) {
		return;
	}

	const auto &rack = data.rack_settings;
	const QList<int> modules = moduleIndices(data);
	const QList<int> rows = visibleRows(data);
	const qreal pad = 4.0;
	const qreal plate_w = qMax(rack.slotWidth, 10.0);
	const qreal plate_h = qMax(rack.slotHeight, 10.0);

		// The title band only takes room when there is a title
	const QString title = rackTitle(data);
	const qreal title_h = title.isEmpty() ? 0.0 : rowHeight(data) + 2.0;
	const QSizeF size = rackSize(data);
	const QRectF frame(anchor, size);
	const QPen border_pen(Qt::black, 0.5);

	painter->save();
	painter->setPen(border_pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawRect(frame);

	if (title_h > 0) {
		const QRectF header(frame.x(), frame.y(), frame.width(), title_h);
		painter->save();
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(220, 220, 220));
		painter->drawRect(header);
		painter->restore();
		painter->setPen(border_pen);
		painter->drawLine(header.bottomLeft(), header.bottomRight());
		painter->setFont(headerFont(data));
		painter->drawText(header.adjusted(1, 0, -1, 0), Qt::AlignCenter, title);
	}

		// One plate per module, in slot order
	painter->setFont(cellFont(data));
	qreal x = frame.x() + pad;
	const qreal y = frame.y() + title_h + pad;
	for (int module : modules) {
		const QRectF plate(x, y, plate_w, plate_h);
		painter->setPen(border_pen);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(plate);

		int first_row = -1;
		for (int row : rows) {
			if (data.ios.at(row).moduleIndex == module) {
				first_row = row;
				break;
			}
		}
		QString tmpl = rack.plateTemplate;
		if (tmpl.isEmpty()) {
			tmpl = QStringLiteral("%module");
		}
		const QString text = expand(tmpl, data.ios.at(qMax(first_row, 0)),
			qMax(first_row, 0), moduleLabelFor(data, module), QString());

		QRectF text_rect = plate.adjusted(1, 1, -1, -1);
		if (rack.showAddressRange) {
			const QString range = addressRange(data, module);
			if (!range.isEmpty()) {
				QRectF range_rect = text_rect;
				range_rect.setTop(text_rect.bottom() - rowHeight(data));
				text_rect.setBottom(range_rect.top());
				painter->save();
				painter->setClipRect(range_rect, Qt::IntersectClip);
				painter->drawText(range_rect,
								  Qt::AlignCenter | Qt::TextWordWrap, range);
				painter->restore();
			}
		}
		painter->save();
		painter->setClipRect(text_rect, Qt::IntersectClip);
		painter->drawText(text_rect, Qt::AlignCenter | Qt::TextWordWrap, text);
		painter->restore();

		x += plate_w;
	}

	painter->restore();
}

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
#ifndef REALTERMINAL_H
#define REALTERMINAL_H

#include <QSharedPointer>
#include <QColor>
#include <QDomElement>
#include <QUuid>
#include "../properties/elementdata.h"

class TerminalStrip;
class Element;
class TerminalElement;
class PhysicalTerminal;
class TerminalStripBridge;

/**
 * @brief The RealTerminal class
 * Represent a real terminal.
 * A real terminal can be a drawn terminal in a folio
 * or a terminal set by user but not present
 * on any folio (for example a reserved terminal).
 *
 * The second case is what createPending() builds: a terminal which is
 * already in a strip -- with a label and a symbol to be drawn with later --
 * but which holds no element, so isElement() is false and label() falls back
 * on the label the terminal was created with.
 *
 * When create a new instance of RealTerminal you must
 * call sharedRef() and only use the returned QSharedPointer
 * instead of the raw pointer
 */
class RealTerminal
{
		friend class TerminalElement;
		friend class PhysicalTerminal;

	private:
		RealTerminal(Element *element);

		QSharedPointer<RealTerminal> sharedRef();
		QSharedPointer<RealTerminal> sharedRef() const;
		QWeakPointer<RealTerminal> weakRef();

		void setPhysicalTerminal(const QSharedPointer<PhysicalTerminal> &phy_t);

			///Where the wires of a cable land, worked out from the project:
			///empty when no cable lands on a piece worth naming (cableTarget()
			///then falls back on the text typed in by hand).
		QVector<QString> automaticCableTarget() const;

	public:
		~RealTerminal();
		static QSharedPointer<RealTerminal> createSpare(const ElementData &data, const QUuid &uuid = QUuid());
		static QSharedPointer<RealTerminal> spareFromXml(const QDomElement &xml_element);
		bool isSpare() const;
		ElementData spareData() const;
		void setSpareData(const ElementData &data);
		ElementData terminalData() const;
		TerminalStrip *parentStrip() const noexcept;
		QSharedPointer<PhysicalTerminal> physicalTerminal() const noexcept;

		/**
		 * @brief createPending
		 * Build a terminal which belongs to a strip but which is not drawn
		 * in any folio yet -- the terminals the terminal navigator creates
		 * out of the ID field of its strip creation dialog, before the user
		 * drags their symbol onto a folio.
		 *
		 * Such a terminal carries its own label (@p label, the same
		 * 'strip name:ID' a drawn terminal gets from its element) and the
		 * location of the element it should be drawn with (@p symbol, a
		 * location string such as 'embed://...' or 'common://...'), neither
		 * of which any element could provide here.
		 *
		 * @param label : the label of the terminal, may be empty
		 * @param symbol : location string of the symbol to draw it with, may be empty
		 * @param uuid : identity of the terminal, a new one is made when null
		 * @return a shared reference, which is the only usable handle on it
		 * (@sa the class description about sharedRef())
		 */
		static QSharedPointer<RealTerminal> createPending(const QString &label,
														  const QString &symbol = QString(),
														  const QUuid &uuid = QUuid());

		QDomElement toXml(QDomDocument &parent_document) const;

		int level() const;
		QString label() const;
		QString symbol() const;
		QString Xref() const;
		QString cable() const;
		QString cableWire() const;
		bool cableIsAutomatic() const;
		bool isShield() const;
		QVector<QString> cableTarget() const;
			///True when cableTarget() is worked out from the project (read only),
			///false when it is the text typed in by hand (which may be edited)
		bool cableTargetIsAutomatic() const;
		QColor color() const;
		QColor stripeColor() const;
		qreal section() const;
		qreal widthMm() const;
		QString catalogEntry() const;
		bool hasSeparatorAfter() const;

		//Keys used to store the cable data in the element informations
		static QString cableInfoKey()     { return QStringLiteral("terminal_cable"); }
		static QString cableWireInfoKey() { return QStringLiteral("terminal_cable_wire"); }
			//Target typed in by hand, only read when the cables give none
		static QString cableTargetInfoKey() { return QStringLiteral("terminal_cable_target"); }
		static QString shieldInfoKey()    { return QStringLiteral("terminal_shield"); }
		//Keys used to store the color and the section (mm2) of the terminal
		static QString colorInfoKey()     { return QStringLiteral("terminal_color"); }
		static QString sectionInfoKey()   { return QStringLiteral("terminal_section"); }
		//Key used to store the width (mm) of the terminal, drawn in the terminal strip
		static QString widthInfoKey()     { return QStringLiteral("terminal_width"); }
		//Key used to store the terminal chosen in the catalog (TerminalCatalog::WidthEntry::label)
		static QString catalogEntryInfoKey() { return QStringLiteral("terminal_catalog_entry"); }
		//Key used to store if a separator plate is mounted after the terminal
		static QString separatorInfoKey() { return QStringLiteral("terminal_separator"); }

		static void applyCatalogEntry(DiagramContext &informations, const QString &entry_label);

		QString conductor() const;

		ElementData::TerminalType type() const;
		ElementData::TerminalFunction function() const;

		bool isLed() const;
		bool isElement() const;
		bool isBridged() const;

		QSharedPointer<TerminalStripBridge> bridge() const;

		Element* element() const;
		/**
		 * @brief elementUuid
		 * @return the uuid of the element this terminal is drawn with, or a
		 * null uuid when the terminal is not drawn (yet).
		 */
		QUuid elementUuid() const;
		/**
		 * @brief uuid
		 * @return the identity to address this terminal with: the uuid of
		 * its element for a drawn terminal -- unchanged, every bridge and
		 * every command of the terminal strip editor keeps working -- and
		 * the uuid of the terminal itself when it is not drawn yet, which
		 * elementUuid() can not provide since there is no element at all.
		 */
		QUuid uuid() const;

		static QString xmlTagName();

	private :
		bool hasData() const;

		QPointer<Element> m_element;
			//A spare terminal has no element : it is on no folio, its data are its own
		bool m_spare = false;
		QUuid m_spare_uuid;
		ElementData m_spare_data;
		QWeakPointer<RealTerminal> m_this_weak;
		QSharedPointer<PhysicalTerminal> m_physical_terminal;
		/// Identity of a terminal which holds no element yet, @sa uuid()
		QUuid m_uuid;
		/// Label and symbol of a terminal which holds no element yet,
		/// @sa createPending()
		QString m_label, m_symbol;
};

#endif // REALTERMINAL_H

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
#include <QPair>
#include <QStringList>
#include <QVector>
#include <QUuid>
#include <properties/elementdata.h>

class TerminalStrip;
class Element;
class TerminalElement;
class PhysicalTerminal;
class TerminalStripBridge;
class ElementsLocation;
class QETProject;

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
 * Such a terminal is not drawn anywhere, but it is described like a spare
 * one: type, function, LED, cable, color... are its own data too, edited
 * in the terminal strip editor and saved with it (@sa isPending()), until
 * its symbol is drawn on a folio and the element takes over. What it is
 * not is a spare terminal: isSpare() stays false, so the nomenclature of
 * the project counts neither it nor its data, only the spare terminals.
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
		 * @brief isPending
		 * @return true for a terminal which is already in a strip but
		 * which is drawn in no folio yet: it holds no element and carries
		 * an identity of its own instead (@sa createPending()).
		 *
		 * Such a terminal has data of its own -- the ones the terminal
		 * strip editor edits for it, stored the way a spare terminal
		 * stores its own (@sa setSpareData()) -- but it is not a spare
		 * terminal: isSpare() stays false, so the nomenclature of the
		 * project, which counts the spare terminals, counts it neither.
		 * It becomes a terminal of an element when its symbol is drawn
		 * on a folio, and the data of that element are its data then.
		 */
		bool isPending() const;

		/**
		 * @brief unbindFromElement
		 * Take this terminal off the element it is drawn with, leaving it in
		 * the state of a terminal which is not drawn yet (@sa isPending()):
		 * it keeps the label, the symbol and the data of that element as its
		 * own, carries an identity of its own again, and can be drawn on any
		 * folio later, like any terminal of the terminal navigator.
		 *
		 * This is what "delete from the drawing only" does with a terminal
		 * of the drawing: the symbol leaves the folio, but the terminal
		 * itself stays in its strip, waiting to be drawn again -- instead of
		 * leaving the strip with its symbol, which is what deleting the row
		 * does. The reverse is rebindToElement(), which the undo of this
		 * calls.
		 *
		 * @return true if there was an element to unbind from, false if
		 * this terminal was already unbound
		 */
		bool unbindFromElement();
		/**
		 * @brief rebindToElement
		 * @param element : the element this terminal belongs to, must not be
		 * null. Bind this terminal back to @a element, undoing
		 * unbindFromElement(): the label, the symbol and the data of the
		 * terminal are the ones of the element again, and the identity of
		 * its own goes away, since a terminal drawn on a folio is told
		 * apart by its element.
		 */
		void rebindToElement(Element *element);

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
		 * @param symbol : location string of the symbol to draw it with, may be empty.
		 * The runtime project id some projects still carry in such a string
		 * ('project0+embed://...') is dropped here: it means nothing outside
		 * the session which wrote it, so the terminal keeps the plain path
		 * (@sa symbolWithoutProjectId)
		 * @param uuid : identity of the terminal, a new one is made when null
		 * @return a shared reference, which is the only usable handle on it
		 * (@sa the class description about sharedRef())
		 */
		static QSharedPointer<RealTerminal> createPending(const QString &label,
														  const QString &symbol = QString(),
														  const QUuid &uuid = QUuid());
		static QSharedPointer<RealTerminal> pendingFromXml(const QDomElement &xml_element);

		QDomElement toXml(QDomDocument &parent_document) const;

		int level() const;
		QString label() const;
		QString symbol() const;

		/**
		 * @brief symbolWithoutProjectId
		 * @param symbol : a location string of a terminal symbol, the way
		 * it was saved in a project or handed around
		 * @return @a symbol without the runtime project id some projects
		 * still save in it: 'project0+embed://...' becomes 'embed://...'.
		 *
		 * Such an id is worth nothing outside the session which wrote it.
		 * Projects are numbered with an ever increasing counter and a
		 * number is never reused (@sa QETApp::registerProject), so in
		 * almost every later session 'project0' names another project, or
		 * no project at all -- a location read out of such a string is
		 * then empty, or points into the wrong project, and a terminal
		 * could not be drawn with its own symbol anymore. Symbols
		 * therefore live without that id, in memory and in the files,
		 * and the readers bind the path back to the project it belongs
		 * to (@sa symbolLocation).
		 */
		static QString symbolWithoutProjectId(const QString &symbol);

		/**
		 * @brief symbolLocation
		 * @param symbol : location string of a terminal symbol, with or
		 * without a runtime project id in it
		 * @param project : the project the symbol belongs to, may be null
		 * @return the location @a symbol names, resolved through
		 * @p project for a symbol of that project's own collection.
		 *
		 * Only 'embed://...' needs the project: it names a path inside
		 * the embedded collection of one project, which no session-wide
		 * registry can answer for once the id is gone. 'common://...' and
		 * the other file system collections name a file and resolve on
		 * their own -- and the two-argument constructor would take such a
		 * path for an embedded one and wrongly prepend 'embed://' to it
		 * (@sa ElementsLocation::setPath), which is why the project is
		 * only passed on for an embedded symbol.
		 */
		static ElementsLocation symbolLocation(const QString &symbol,
											   QETProject *project);

		/**
		 * @brief symbolLocation
		 * @return the location this terminal's symbol names, resolved
		 * through the project of the strip this terminal belongs to
		 * (@sa parentStrip()), or a location which resolves nothing when
		 * the terminal is in no strip yet -- no project could answer for
		 * an embedded symbol then.
		 */
		ElementsLocation symbolLocation() const;
		QString Xref() const;
		QString cable() const;
		QString cableWire() const;
		bool cableIsAutomatic() const;
		bool isShield() const;
		QVector<QString> cableTarget() const;
			///True when cableTarget() is worked out from the project (read only),
			///false when it is the text typed in by hand (which may be edited)
		bool cableTargetIsAutomatic() const;
			///One entry per wire of this terminal which is no core of a
			///cable: (what the other end is called -- "12K1:A1" --, what
			///the wire is made of in the order type, colour and section
			///with the empty ones left out). A terminal with a twin
			///sleeve carries two wires and gets two entries.
		QVector<QPair<QString, QStringList>> wires() const;
			///Whether this terminal has a wire of its own at all
		bool hasWire() const;
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
			//A terminal which holds no element but data of its own: a spare
			//one (@sa createSpare()) or a pending one (@sa createPending()).
			//Both keep them in m_spare_data, which is therefore the data of
			//a terminal drawn on no folio, whoever created it.
		bool hasOwnData() const {return m_spare || isPending();}

		QPointer<Element> m_element;
			//A spare terminal has no element : it is on no folio, its data are its own
		bool m_spare = false;
		QUuid m_spare_uuid;
			//The data of a terminal drawn on no folio: a spare one, or one
			//which is not drawn yet (@sa hasOwnData())
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

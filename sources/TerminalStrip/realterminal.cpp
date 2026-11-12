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
#include "realterminal.h"
#include "terminalstrip.h"
#include <ElementsCollection/elementslocation.h>
#include <qetgraphicsitem/terminalelement.h>
#include "physicalterminal.h"
#include "terminalstriplabels.h"
#include <diagram.h>
#include "terminalcolor.h"
#include "terminalcatalog.h"
#include <qetgraphicsitem/conductor.h>
#include <qetgraphicsitem/element.h>
#include <qetgraphicsitem/terminal.h>
#include <qetinformation.h>
#include <diagram.h>
#include <cable/cable.h>
#include <cable/cablemanager.h>
#include <qetproject.h>

#include <QHash>
#include <QSet>

namespace
{
		/**
		 * The cable cores wired to one terminal element, worked out from
		 * the cables of the project (PR #1438). A core knows its
		 * conductor, and CableManager::refreshLabels() writes the cable
		 * and the core into the properties of that conductor, so reading
		 * the conductors of the element is enough.
		 * Each pair is (cable designation, core colour or number).
		 */
	QList<QPair<QString, QString>> wiredCores(Element *element)
	{
		QList<QPair<QString, QString>> cores;
		if (!element || !element->diagram()) {
			return cores;
		}
		const auto project = element->diagram()->project();
		for (const auto terminal : element->terminals())
		{
			for (const auto conductor : terminal->conductors())
			{
				const auto properties = conductor->ownProperties();
				const auto cable = CableManager::cableByUuid(project, properties.m_cable_uuid);
				if (!cable || cable->designation().isEmpty()) {
					continue;
				}
				auto wire = cable->colorOfCore(properties.m_cable_slot);
				if (wire.isEmpty() && properties.m_cable_slot >= 0) {
					wire = QString::number(properties.m_cable_slot + 1);
				}
				const QPair<QString, QString> core(cable->designation(), wire);
				if (!cores.contains(core)) {
					cores.append(core);
				}
			}
		}
		return cores;
	}

		/**
		 * What the far end of a wire standing at @p terminal is called:
		 * a terminal of a strip by its strip and its position on it
		 * ("X1:7"), anything else by its element and its terminal
		 * ("K1:A1").
		 */
	QString endDesignation(Terminal *terminal)
	{
		if (!terminal) {
			return QString();
		}
		Element *element = terminal->parentElement();
		if (!element) {
			return QString();
		}

		auto *terminal_element = qobject_cast<TerminalElement *>(element);
		if (terminal_element)
		{
				//A terminal says it in its own label -- the very number
				//the strip draws on it ("X3:29"). The position it stands
				//at is a different numbering, which runs out of step with
				//the labels as soon as one of them is reordered, so it is
				//only used for a terminal which has no label at all.
				//Taken as it stands: it is the very thing the strip
				//writes in the box of this terminal, so the two can
				//never disagree about which terminal is meant
			const QString own = element->actualLabel();
			if (!own.isEmpty()) {
				return own;
			}

			if (TerminalStrip *strip = terminal_element->parentTerminalStrip())
			{
				const auto real = terminal_element->realTerminal();
				const auto physical = real ? real->physicalTerminal() : nullptr;
				const QString number = physical
					? QString::number(physical->pos() + 1) : QString();
				if (!strip->name().isEmpty() && !number.isEmpty()) {
					return strip->name() + QLatin1Char(':') + number;
				}
				if (!number.isEmpty()) {
					return number;
				}
			}
		}

		const QString name = terminal->name();
		QString label = element->actualLabel();
		if (label.isEmpty()) {
			return name;
		}
		if (name.isEmpty()) {
			return label;
		}
		return label + QLatin1Char(':') + name;
	}

		/**
		 * Whether @p properties says the conductor is a core of a cable:
		 * the reference the cable gave it, or -- in the projects saved
		 * before that reference existed -- the text of the cable field,
		 * which is generated from the cable and never typed in.
		 */
	bool isCableCore(const ConductorProperties &properties)
	{
		return !properties.m_cable_uuid.isNull()
			|| !properties.m_cable.isEmpty();
	}

		/**
		 * The wires of @p element which are no core of a cable: a cable
		 * says where its cores go and what they are made of itself, and
		 * is drawn under the strip with the rest of the cable. Every
		 * terminal of the element may carry one, so a terminal with a
		 * twin sleeve comes back with two of them.
		 */
	QList<Conductor *> ownWiresOf(Element *element)
	{
		QList<Conductor *> wires;
		if (!element) {
			return wires;
		}
		for (const auto terminal : element->terminals())
		{
			for (const auto conductor : terminal->conductors())
			{
				if (!isCableCore(conductor->ownProperties())) {
					wires << conductor;
				}
			}
		}
		return wires;
	}

		/**
		 * What @p conductor is made of, in the order type, colour and
		 * section, the empty ones left out.
		 */
	QStringList definitionOf(const Conductor *conductor)
	{
		const ConductorProperties properties = conductor->ownProperties();
		QStringList definition;
		for (const QString &value : {properties.m_conductor_type,
					     properties.m_wire_color,
					     properties.m_wire_section})
		{
			if (!value.isEmpty()) {
				definition << value;
			}
		}
		return definition;
	}
}

#include <QRegularExpression>

/**
 * @brief RealTerminal
 * @param parent_strip : parent terminal strip
 * @param terminal : terminal element (if any) in a folio
 */
RealTerminal::RealTerminal(Element *terminal) :
	m_element(terminal)
{}

/**
 * @brief RealTerminal::createSpare
 * Create a spare terminal (a reserved terminal) : a terminal of a strip
 * which is not drawn on any folio. Its data (label, type, section,
 * width...) are its own, stored with the terminal strip.
 * @param data : the data of the terminal, only the terminal type,
 * function, led and the informations are used.
 * @param uuid : the uuid of the spare terminal
 * @return the new spare terminal, not yet added to a strip
 */
QSharedPointer<RealTerminal> RealTerminal::createSpare(const ElementData &data, const QUuid &uuid)
{
	auto raw_ = new RealTerminal(nullptr);
	raw_->m_spare = true;
	raw_->m_spare_uuid = uuid.isNull() ? QUuid::createUuid() : uuid;
	raw_->m_spare_data.m_type = ElementData::Terminal;
	raw_->m_spare_data.setTerminalType(data.terminalType());
	raw_->m_spare_data.setTerminalFunction(data.terminalFunction());
	raw_->m_spare_data.setTerminalLED(data.terminalLed());
	raw_->m_spare_data.m_informations = data.m_informations;
	return raw_->sharedRef();
}

/**
 * @brief ownDataFromXml
 * Read the data a terminal drawn on no folio keeps for itself out of the
 * element @a xml_element those data were saved in: type, function, LED
 * and informations, which is what RealTerminal::toXml() writes for a
 * spare terminal and for one which is not drawn yet alike.
 * @param xml_element : a real terminal saved by toXml()
 * @return the data read, empty when @a xml_element holds none of them
 */
static ElementData ownDataFromXml(const QDomElement &xml_element)
{
	ElementData data_;
		//Those data belong to a terminal, never to an element -- which is
		//what the data of a spare terminal are set to as well
		//(@sa createSpare())
	data_.m_type = ElementData::Terminal;
	data_.setTerminalType(ElementData::terminalTypeFromString(
							  xml_element.attribute(QStringLiteral("type"))));
	data_.setTerminalFunction(ElementData::terminalFunctionFromString(
								  xml_element.attribute(QStringLiteral("function"))));
	data_.setTerminalLED(QET::infoFlagIsTrue(xml_element.attribute(QStringLiteral("led"))));
	const auto xml_info = xml_element.firstChildElement(QStringLiteral("informations"));
	if (!xml_info.isNull()) {
		data_.m_informations.fromXml(xml_info, QStringLiteral("information"));
	}
	return data_;
}

/**
 * @brief RealTerminal::spareFromXml
 * @param xml_element : a real terminal saved by toXml()
 * @return the spare terminal saved in @a xml_element,
 * or a null pointer if @a xml_element is not a spare terminal.
 */
QSharedPointer<RealTerminal> RealTerminal::spareFromXml(const QDomElement &xml_element)
{
	if (xml_element.tagName() != xmlTagName() ||
		!xml_element.hasAttribute(QStringLiteral("spare_uuid"))) {
		return QSharedPointer<RealTerminal>();
	}

	return createSpare(ownDataFromXml(xml_element),
					   QUuid(xml_element.attribute(QStringLiteral("spare_uuid"))));
}

/**
 * @brief RealTerminal::pendingFromXml
 * @param xml_element : a real terminal saved by toXml()
 * @return the terminal which is drawn in no folio yet and whose identity
 * is saved in @a xml_element, with the data the terminal strip editor
 * edited for it (@sa isPending()),
 * or a null pointer if @a xml_element is not such a terminal.
 */
QSharedPointer<RealTerminal> RealTerminal::pendingFromXml(const QDomElement &xml_element)
{
	if (xml_element.tagName() != xmlTagName() ||
		!xml_element.hasAttribute(QStringLiteral("terminal_uuid"))) {
		return QSharedPointer<RealTerminal>();
	}

	auto real_t = createPending(
		xml_element.attribute(QStringLiteral("label")),
		xml_element.attribute(QStringLiteral("symbol")),
		QUuid(xml_element.attribute(QStringLiteral("terminal_uuid"))));
	real_t->m_spare_data = ownDataFromXml(xml_element);

	return real_t;
}

/**
 * @brief RealTerminal::isSpare
 * @return true if this terminal is a spare terminal, not drawn on any folio
 * @sa createSpare
 */
bool RealTerminal::isSpare() const {
	return m_spare;
}

/**
 * @brief RealTerminal::spareData
 * @return the data this terminal keeps for itself -- those of a spare
 * terminal, and equally those of one which is drawn in no folio yet
 * (@sa isPending()) -- an empty ElementData if it keeps none of its own:
 * a terminal drawn on a folio takes them from its element, and one left
 * behind by an element deleted in an older version has none at all.
 */
ElementData RealTerminal::spareData() const {
	return m_spare_data;
}

/**
 * @brief RealTerminal::setSpareData
 * Set the data this terminal holds itself -- the data of a spare
 * terminal, and equally those of one which is drawn in no folio yet
 * (@sa isPending()) -- and tell the strip, so that it is redrawn.
 * Nothing is done for a terminal drawn on a folio: its data belong to
 * its element and are set through that element
 * (@sa ChangeElementDataCommand).
 * @param data
 */
void RealTerminal::setSpareData(const ElementData &data)
{
	if (!hasOwnData()) {
		return;
	}
	m_spare_data.setTerminalType(data.terminalType());
	m_spare_data.setTerminalFunction(data.terminalFunction());
	m_spare_data.setTerminalLED(data.terminalLed());
	m_spare_data.m_informations = data.m_informations;

	if (auto strip_ = parentStrip()) {
		strip_->terminalDataChanged();
	}
}

/**
 * @brief RealTerminal::hasData
 * @return true if this terminal has data : it is linked to an element,
 * or it holds data of its own because it is drawn nowhere -- a spare
 * terminal, or one which is not drawn yet (@sa isPending()).
 */
bool RealTerminal::hasData() const {
	return hasOwnData() || !m_element.isNull();
}

/**
 * @brief RealTerminal::terminalData
 * @return the data of this terminal : the data of its element,
 * or its own data if it holds none of its own -- a spare terminal,
 * or one which is drawn nowhere yet (@sa isPending()).
 */
ElementData RealTerminal::terminalData() const
{
	if (hasOwnData()) {
		return m_spare_data;
	}
	if (m_element) {
		return m_element->elementData();
	}
	return ElementData();
}

RealTerminal::~RealTerminal()
{
	if (m_physical_terminal) {
		m_physical_terminal->removeTerminal(sharedRef());
	}
}

/**
 * @brief RealTerminal::createPending
 * Build a terminal which holds no element, that is to say a terminal of a
 * strip which is not drawn in any folio yet.
 * @param label : the label of the terminal, 'strip name:ID' the way a drawn
 * terminal gets it from its element
 * @param symbol : location string of the element to draw it with later,
 * may be empty when the user did not choose one
 * @param uuid : identity of the terminal, a new one is made when null
 * @return a shared reference on the new terminal
 */
QSharedPointer<RealTerminal> RealTerminal::createPending(const QString &label,
														 const QString &symbol,
														 const QUuid &uuid)
{
	auto *real_t = new RealTerminal(nullptr);
	real_t->m_label = label;
		//The symbol lives without the runtime project id of the session
		//which handed it over, whatever the caller passes: a saved
		//'project0+embed://...' names another project -- or none at all --
		//in every later session, and the symbol could not be found for
		//the terminal anymore. This is the one place symbols are set
		//(@sa pendingFromXml, the strip creation dialog), so keeping the
		//plain path here keeps it everywhere; the readers bind it back to
		//their own project instead (@sa symbolLocation).
	real_t->m_symbol = symbolWithoutProjectId(symbol);
	real_t->m_uuid = uuid.isNull() ? QUuid::createUuid() : uuid;
		//Its data are its own the way a spare terminal's are, stored in
		//the same place: the strip editor edits them before the terminal
		//is drawn anywhere, @sa isPending()
	real_t->m_spare_data.m_type = ElementData::Terminal;

	return real_t->sharedRef();
}

/**
 * @brief RealTerminal::isPending
 * @return true for a terminal which is already in a strip but which is
 * drawn in no folio yet: it holds no element and carries an identity of
 * its own instead (@sa createPending()).
 *
 * Such a terminal is described like a spare one -- its data are its own
 * (@sa hasOwnData()), edited in the terminal strip editor and saved with
 * it -- but isSpare() stays false for it: the nomenclature of the project
 * counts the spare terminals and the terminals drawn on folios, and a
 * terminal which is drawn nowhere yet joins it when its symbol is drawn.
 */
bool RealTerminal::isPending() const {
	return !m_spare && m_element.isNull() && !m_uuid.isNull();
}

/**
 * @brief RealTerminal::unbindFromElement
 * @return true if there was an element to unbind from, false if this
 * terminal was already unbound.
 *
 * Everything the terminal takes from its element becomes its own, kept
 * exactly the way a pending terminal keeps it (@sa createPending()), so
 * that nothing changes for the user between the drawn terminal and the
 * same terminal not drawn anymore: the label it shows, the symbol it is
 * drawn with, the data the terminal strip editor edits, and an identity
 * of its own to be saved and dragged by. The data of the element are
 * copied as a whole -- type and function included -- the way
 * terminalData() would have answered for the drawn terminal, so the
 * strip editor shows the same values before and after.
 *
 * The element itself is only let go, never touched: it stays alive and
 * bound to the drawing until that is deleted (or put back by the undo of
 * this, @sa rebindToElement()).
 */
bool RealTerminal::unbindFromElement()
{
	if (m_element.isNull()) {
		return false;
	}
	Element *element_ = m_element.data();

		//Read the label and the symbol while the element is still
		//there: both are answered by the element for a drawn
		//terminal (@sa label(), symbol()).
	m_label = label();
	m_symbol = symbolWithoutProjectId(element_->location().toString());
	m_spare_data = element_->elementData();
	m_uuid = QUuid::createUuid();

	m_element = nullptr;
	if (auto strip_ = parentStrip()) {
		strip_->terminalDataChanged();
	}
	return true;
}

/**
 * @brief RealTerminal::rebindToElement
 * @param element : the element this terminal belongs to, must not be null.
 *
 * The undo of unbindFromElement(): the terminal goes back to being the
 * terminal of @a element, told apart by that element again -- the
 * identity of its own and the label, symbol and data copied out of the
 * element go away, since they belong to no terminal of a strip anymore
 * and must not be saved with it (@sa toXml, which writes the element
 * uuid then).
 */
void RealTerminal::rebindToElement(Element *element)
{
	if (!element) {
		return;
	}
	m_element = element;
	m_uuid = QUuid();
	m_label.clear();
	m_symbol.clear();
	m_spare_data = ElementData();

	if (auto strip_ = parentStrip()) {
		strip_->terminalDataChanged();
	}
}

/**
 * @brief RealTerminal::sharedRef
 * @return a QSharedPointer of this
 */
QSharedPointer<RealTerminal> RealTerminal::sharedRef()
{
	QSharedPointer<RealTerminal> this_shared(this->weakRef());
	if (this_shared.isNull())
	{
		this_shared = QSharedPointer<RealTerminal>(this);
		m_this_weak = this_shared.toWeakRef();
	}

	return this_shared;
}

/**
 * @brief RealTerminal::sharedRef
 * @return a shared reference of this, not that because
 * this method is const, the shared reference can be null if not already
 * used in another part of the code.
 */
QSharedPointer<RealTerminal> RealTerminal::sharedRef() const {
	return QSharedPointer<RealTerminal>(m_this_weak);
}

/**
 * @brief RealTerminal::weakRef
 * @return a QWeakPointer of this, weak pointer can be bull
 */
QWeakPointer<RealTerminal> RealTerminal::weakRef() {
	return m_this_weak;
}

/**
 * @brief toXml
 * @param parent_document
 * @return this real terminal to xml
 */
QDomElement RealTerminal::toXml(QDomDocument &parent_document) const
{
	auto root_elmt = parent_document.createElement(this->xmlTagName());

		//Type, function, LED and informations of a terminal which is drawn
		//on no folio and keeps its data for itself: a spare one, or one
		//which is not drawn yet (@sa isPending()). Both save them the same
		//way, so that reading one or the other is one and the same thing.
	const auto set_own_data = [&root_elmt, &parent_document](const ElementData &data_)
	{
		root_elmt.setAttribute(QStringLiteral("type"), ElementData::terminalTypeToString(data_.terminalType()));
		root_elmt.setAttribute(QStringLiteral("function"), ElementData::terminalFunctionToString(data_.terminalFunction()));
		if (data_.terminalLed()) {
			root_elmt.setAttribute(QStringLiteral("led"), QStringLiteral("true"));
		}
		auto xml_info = parent_document.createElement(QStringLiteral("informations"));
		data_.m_informations.toXml(xml_info, QStringLiteral("information"));
		root_elmt.appendChild(xml_info);
	};

	if (m_spare)
	{
			//A spare terminal is on no folio : its data are saved here
		root_elmt.setAttribute(QStringLiteral("spare_uuid"), m_spare_uuid.toString());
		set_own_data(m_spare_data);
	}
	else if (m_element) {
		root_elmt.setAttribute(QStringLiteral("element_uuid"), m_element->uuid().toString());
	} else if (!m_uuid.isNull()) {
		//A terminal which is not drawn in any folio yet carries its own
		//identity, its label and the symbol it should be drawn with, so
		//that the project round trip keeps it. A terminal which holds no
		//element and no identity of its own -- what an element deleted in
		//an older version left behind -- is still written empty and still
		//dropped at reading time, @sa TerminalStrip::fromXml().
		//A spare terminal and such a pending terminal never both describe
		//one terminal: a spare has spare_uuid and no terminal_uuid, a
		//pending one the other way round.
		root_elmt.setAttribute(QStringLiteral("terminal_uuid"), m_uuid.toString());
		const auto label_ = label();
		if (!label_.isEmpty()) {
			root_elmt.setAttribute(QStringLiteral("label"), label_);
		}
		if (!m_symbol.isEmpty()) {
				//Id-free, as it already is in memory (@sa
				//createPending): what is read back from the file later
				//on must not depend on the session which saved it
			root_elmt.setAttribute(QStringLiteral("symbol"),
								   symbolWithoutProjectId(m_symbol));
		}
			//The data the terminal strip editor edited for it, before its
			//symbol is drawn anywhere: label included, since label() reads
			//it back from them, @sa pendingFromXml()
		set_own_data(m_spare_data);
	}

	return root_elmt;
}

/**
 * @brief RealTerminal::setPhysicalTerminal
 * Set the parent physical terminal of this real terminal
 * @param phy_t
 */
void RealTerminal::setPhysicalTerminal(const QSharedPointer<PhysicalTerminal> &phy_t) {
	m_physical_terminal = phy_t;
		//Every way into or out of a strip comes here: the terminal may now
		//head, or no longer head, a strip on its folio
	if (m_element && m_element->diagram())
		TerminalStripLabels::schedule(m_element->diagram());
}

/**
* @brief parentStrip
* @return parent terminal strip or nullptr
*/
TerminalStrip *RealTerminal::parentStrip() const noexcept {
	if (m_physical_terminal) {
		return m_physical_terminal->terminalStrip();
	} else {
		return nullptr;
	}
}

/**
 * @brief RealTerminal::physicalTerminal
 * @return The parent physical terminal of this terminal.
 * The returned QSharedPointer can be null
 */
QSharedPointer<PhysicalTerminal> RealTerminal::physicalTerminal() const noexcept{
	return m_physical_terminal;
}

/**
 * @brief RealTerminal::level
 * @return
 */
int RealTerminal::level() const
{
	if (m_physical_terminal &&
		sharedRef()) {
		return m_physical_terminal->levelOf(sharedRef());
	}

	return -1;
}

/**
 * @brief label
 * @return the label of this real terminal. A terminal which is not drawn in
 * any folio yet has no element to take a label from, and falls back on the
 * label it was created with (@sa createPending()) -- unless the terminal
 * strip editor edited that label, which is stored with the data of the
 * terminal, the same way the label of a spare terminal is stored with its
 * own data.
 */
QString RealTerminal::label() const {
	if (m_spare) {
		return m_spare_data.m_informations.value(QStringLiteral("label")).toString();
	} else if (!m_element.isNull()) {
		return m_element->actualLabel();
	} else if (m_spare_data.m_informations.contains(QStringLiteral("label"))) {
		return m_spare_data.m_informations.value(QStringLiteral("label")).toString();
	} else {
		return m_label;
	}
}

/**
 * @brief symbol
 * @return the location string of the element this terminal should be drawn
 * with, for a terminal which is not drawn yet. A drawn terminal takes its
 * symbol from its own element and has nothing to say here.
 * The string never carries the runtime project id of a session -- it was
 * dropped when the terminal was built and is not written back either
 * (@sa symbolWithoutProjectId, symbolLocation).
 */
QString RealTerminal::symbol() const {
	return m_symbol;
}

/**
 * @brief RealTerminal::symbolWithoutProjectId
 * @param symbol : a location string of a terminal symbol, the way it was
 * saved in a project or handed around
 * @return @a symbol without the runtime project id some projects still
 * save in it: 'project0+embed://...' becomes 'embed://...'.
 *
 * That id is the number the application gave the project while the file
 * was written, and numbers are handed out with an ever increasing
 * counter which never reuses one (@sa QETApp::registerProject). Reading
 * such a string back in a later session resolves it against another
 * project, or against none -- ElementsLocation::setPath even reads a
 * single digit only, so 'project10+' never resolved at all. Every
 * symbol is therefore stripped here, in memory as well as on the way
 * into the file, and the project it belongs to is told apart by the
 * strip the terminal lives in instead (@sa symbolLocation).
 */
QString RealTerminal::symbolWithoutProjectId(const QString &symbol)
{
	static const QRegularExpression project_id_prefix(
				QStringLiteral("^project[0-9]+\\+"));
	return QString(symbol).remove(project_id_prefix);
}

/**
 * @brief RealTerminal::symbolLocation
 * @param symbol : location string of a terminal symbol, with or without
 * a runtime project id in it
 * @param project : the project the symbol belongs to, may be null
 * @return the location @a symbol names, resolved through @p project for
 * a symbol of that project's own collection.
 *
 * Only 'embed://...' needs the project: it names a path inside the
 * embedded collection of one project, which no session-wide registry
 * can answer for once the id is gone. 'common://...' and the other file
 * system collections name a file and resolve on their own -- and the
 * two-argument constructor would take such a path for an embedded one
 * and wrongly prepend 'embed://' to it (@sa ElementsLocation::setPath),
 * which is why the project is only passed on for an embedded symbol.
 */
ElementsLocation RealTerminal::symbolLocation(
		const QString &symbol,
		QETProject *project)
{
	const auto path_ = symbolWithoutProjectId(symbol);
	if (project && path_.startsWith(QLatin1String("embed://"))) {
		return ElementsLocation(path_, project);
	}
	return ElementsLocation(path_);
}

/**
 * @brief RealTerminal::symbolLocation
 * @return the location this terminal's symbol names, resolved through
 * the project of the strip this terminal belongs to (@sa parentStrip()),
 * or a location which resolves nothing when the terminal is in no strip
 * yet -- no project could answer for an embedded symbol then.
 */
ElementsLocation RealTerminal::symbolLocation() const
{
	auto *strip_ = parentStrip();
	return symbolLocation(m_symbol, strip_ ? strip_->project() : nullptr);
}

/**
 * @brief RealTerminal::Xref
 * @return Convenient method to get the XRef
 * formatted to string
 */
QString RealTerminal::Xref() const
{
	if (!m_element.isNull()) {
		return autonum::AssignVariables::genericXref(m_element.data());
	} else {
		return QString();
	}
}

/**
 * @brief RealTerminal::cable
 * @return The name of the cable (hose) connected to this terminal.
 * The value is stored in the informations of the element.
 */
QString RealTerminal::cable() const
{
	const auto cores = wiredCores(m_element.data());
	if (!cores.isEmpty())
	{
		QStringList cables;
		for (const auto &core : cores) {
			if (!cables.contains(core.first)) {
				cables.append(core.first);
			}
		}
		return cables.join(QStringLiteral(", "));
	}

	if (hasData()) {
		return terminalData().m_informations.value(cableInfoKey()).toString();
	}
	return QString();
}

/**
 * @brief RealTerminal::cableIsAutomatic
 * @return true when the cable and the core of this terminal come from
 * a cable of the project wired to it, false when they are the values
 * typed in by the user in the terminal strip editor.
 */
bool RealTerminal::cableIsAutomatic() const {
	return !wiredCores(m_element.data()).isEmpty();
}

/**
 * @brief RealTerminal::cableWire
 * @return The color / number of the wire of the cable, connected to this terminal.
 */
QString RealTerminal::cableWire() const
{
	const auto cores = wiredCores(m_element.data());
	if (!cores.isEmpty())
	{
		QStringList wires;
		for (const auto &core : cores) {
			wires.append(core.second);
		}
		return wires.join(QStringLiteral(", "));
	}

	if (hasData()) {
		return terminalData().m_informations.value(cableWireInfoKey()).toString();
	}
	return QString();
}

/**
 * @brief RealTerminal::isShield
 * @return true if this terminal is the shield of the cable
 * and not one of its wires.
 */
bool RealTerminal::isShield() const {
	if (hasData()) {
		return QET::infoFlagIsTrue(
			terminalData().m_informations.value(shieldInfoKey()).toString());
	}
	return false;
}

/**
 * @brief RealTerminal::cableTarget
 * Where the cable of this terminal goes at its far end: the lines to
 * write in the box under the cable. What the project itself says when
 * a cable lands on a piece (see automaticCableTarget()), and the text
 * typed in by hand when none does -- the Target cell of the terminal
 * strip editor is read only in the first case, so a target the drawing
 * cannot work out is still worth naming by hand. A text typed in is
 * written one line per field, Installation, Location and Label (BMK),
 * the way the cell shows them: only a line break breaks it into fields.
 * A "/" written inside a field (a name like "ASP/01") is part of that
 * field and never breaks anything.
 * @return the lines to write in the box under the cable
 */
QVector<QString> RealTerminal::cableTarget() const
{
	const auto automatic_{automaticCableTarget()};
	if (!automatic_.isEmpty()) {
		return automatic_;
	}

	if (hasData())
	{
		const auto typed_{terminalData().m_informations
						  .value(cableTargetInfoKey()).toString()};
		if (!typed_.isEmpty())
		{
				//One line by field, and only a line break breaks the text
				//into fields : a "/" written inside a field stays where it is
			QString text_{typed_};
			text_.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
			text_.replace(QLatin1Char('\r'), QLatin1Char('\n'));

			QVector<QString> lines_;
			for (const QString &field_
				 : text_.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
				lines_ << field_;
			}
			return lines_;
		}
	}
	return QVector<QString>();
}

/**
 * @brief RealTerminal::wires
 * @return one entry per wire of this terminal which is no core of a
 * cable: (what the other end is called, what the wire is made of).
 * A terminal with a twin sleeve carries two wires and comes back with
 * two entries; empty when there is none.
 */
QVector<QPair<QString, QStringList>> RealTerminal::wires() const
{
	QVector<QPair<QString, QStringList>> result;
	if (m_spare || m_element.isNull()) {
		return result;
	}
	for (const Conductor *conductor : ownWiresOf(m_element.data()))
	{
		if (!conductor->terminal1 || !conductor->terminal2) {
			continue;
		}
			//The end which does not hang on this element is the
			//far end the wire is named by
		Terminal *attached = conductor->terminal1->parentElement() == m_element
			? conductor->terminal1 : conductor->terminal2;
		Terminal *far_end = attached == conductor->terminal1
			? conductor->terminal2 : conductor->terminal1;
		result.append({endDesignation(far_end), definitionOf(conductor)});
	}
	return result;
}

/**
 * @brief RealTerminal::hasWire
 * @return true when this terminal has a wire of its own which is no core
 * of a cable -- a cable is drawn under the strip with the rest of itself
 */
bool RealTerminal::hasWire() const
{
	return !ownWiresOf(m_element.data()).isEmpty();
}

/**
 * @brief RealTerminal::cableTargetIsAutomatic
 * @return true when the target of this terminal is worked out from the
 * project -- one of its cables lands on a piece worth naming -- and
 * false when only a text typed in by hand is there to show.
 */
bool RealTerminal::cableTargetIsAutomatic() const
{
	return !automaticCableTarget().isEmpty();
}

/**
 * @brief RealTerminal::automaticCableTarget
 * Where the cable of this terminal goes at its far end: the
 * Installation, Location and Label (BMK) of the first piece the
 * wires of the cable land on, past the strip's own terminals, in
 * plan order -- the same three fields the cable list shows for a
 * target, in the same order. An empty field is left out; a terminal
 * with no cable, or a cable which lands on nothing else, gives no
 * lines at all.
 *
 * The cable is the one the wires drawn to this terminal carry; a
 * cable name typed by hand, with no wire behind it, is looked up by
 * its name among the cables of the project.
 * @return the lines to write in the box under the cable
 */
QVector<QString> RealTerminal::automaticCableTarget() const
{
	if (m_spare || m_element.isNull()) {
		return QVector<QString>();
	}
	TerminalStrip *strip_ = parentStrip();
	if (!strip_ || !strip_->project()) {
		return QVector<QString>();
	}
	QETProject *project_{strip_->project()};

		//The cable the wires drawn to this terminal are part of
	QUuid cable_uuid_;
	for (Terminal *terminal_ : m_element->terminals())
	{
		if (!terminal_) {
			continue;
		}
		for (Conductor *conductor_ : terminal_->conductors())
		{
			if (conductor_ && !conductor_->properties().m_cable_uuid.isNull()) {
				cable_uuid_ = conductor_->properties().m_cable_uuid;
				break;
			}
		}
		if (!cable_uuid_.isNull()) {
			break;
		}
	}
	if (cable_uuid_.isNull())
	{
			//A cable name typed by hand, no wire behind it: the first
			//cable of the project drawn under that name
		const auto name_{cable()};
		if (name_.isEmpty()) {
			return QVector<QString>();
		}
		for (Cable *cable_ : project_->cables())
		{
			if (cable_ && cable_->designation() == name_) {
				cable_uuid_ = cable_->uuid();
				break;
			}
		}
		if (cable_uuid_.isNull()) {
			return QVector<QString>();
		}
	}

		//The own side: this strip's terminals never are the target
	QSet<QUuid> own_;
	for (const auto &physical_ : strip_->physicalTerminal())
	{
		if (!physical_) {
			continue;
		}
		for (const auto &real_ : physical_->realTerminals()) {
			if (real_ && real_->element()) {
				own_.insert(real_->element()->uuid());
			}
		}
	}

		//Where the wires of the cable land: one piece whatever the
		//number of its cores, never a report (the cable continues on
		//the sheet the report points to) and never the strip's own
		//terminals. Same rule as the cable list (cablelistrows.cpp).
	QHash<QUuid, Element *> landed_;
	for (Diagram *diagram_ : project_->diagrams())
	{
		for (Conductor *conductor_ : diagram_->conductors())
		{
			if (!conductor_ || conductor_->properties().m_cable_uuid != cable_uuid_) {
				continue;
			}
			Terminal *ends_[2] = {conductor_->terminal1, conductor_->terminal2};
			for (Terminal *terminal_ : ends_)
			{
				if (!terminal_) {
					continue;
				}
				Element *element_ = terminal_->parentElement();
				if (!element_ || (element_->linkType() & Element::AllReport)) {
					continue;
				}
				if (own_.contains(element_->uuid())) {
					continue;
				}
				landed_.insert(element_->uuid(), element_);
			}
		}
	}
	if (landed_.isEmpty()) {
		return QVector<QString>();
	}

		//The first piece in plan order is the target, as the cable
		//list chooses it
	Element *target_{nullptr};
	for (Element *element_ : std::as_const(landed_)) {
		if (!target_ || comparPos(element_, target_)) {
			target_ = element_;
		}
	}

	const DiagramContext informations_{target_->elementInformations()};
	const auto installation_{informations_.value(QETInformation::ELMT_PLANT).toString()};
	const auto location_{informations_.value(QETInformation::ELMT_LOCATION).toString()};
	const auto label_{informations_.value(QETInformation::ELMT_LABEL).toString()};

	QVector<QString> lines_;
	if (!installation_.isEmpty()) {
		lines_ << installation_;
	}
	if (!location_.isEmpty()) {
		lines_ << location_;
	}
	if (!label_.isEmpty()) {
		lines_ << label_;
	}
	return lines_;
}

/**
 * @brief RealTerminal::color
 * @return The color set by the user for this terminal, drawn in the
 * terminal strip. An invalid color mean that the user did not set a color.
 * The value is stored in the informations of the element.
 * @sa stripeColor
 */
QColor RealTerminal::color() const
{
	QColor main_, stripe_;
	if (hasData()) {
		TerminalColor::parse(terminalData().m_informations.value(colorInfoKey()).toString(),
							 &main_, &stripe_);
	}
	return main_;
}

/**
 * @brief RealTerminal::stripeColor
 * @return The second color of a two-tone color (green/yellow for example)
 * or an invalid color if the color of this terminal is plain or not set.
 */
QColor RealTerminal::stripeColor() const
{
	QColor main_, stripe_;
	if (hasData()) {
		TerminalColor::parse(terminalData().m_informations.value(colorInfoKey()).toString(),
							 &main_, &stripe_);
	}
	return stripe_;
}

/**
 * @brief RealTerminal::section
 * @return The section in mm2 set by the user for this terminal
 * or 0 if no section was set.
 * The value is stored in the informations of the element,
 * with the dot as decimal separator, whatever the locale.
 */
qreal RealTerminal::section() const
{
	if (hasData())
	{
		bool ok_ = false;
		const auto section_ = terminalData().m_informations.value(sectionInfoKey()).toString().toDouble(&ok_);
		if (ok_ && section_ > 0) {
			return section_;
		}
	}
	return 0;
}

/**
 * @brief RealTerminal::widthMm
 * @return The width in mm of this terminal (the width of the terminal
 * sold by the manufacturer), set by the user, or 0 if no width was set.
 * The value is stored in the informations of the element,
 * with the dot as decimal separator, whatever the locale.
 */
qreal RealTerminal::widthMm() const
{
	if (hasData())
	{
		bool ok_ = false;
		const auto width_ = terminalData().m_informations.value(widthInfoKey()).toString().toDouble(&ok_);
		if (ok_ && width_ > 0) {
			return width_;
		}
	}
	return 0;
}

/**
 * @brief RealTerminal::catalogEntry
 * @return the label of the terminal chosen in the catalog
 * (TerminalCatalog::WidthEntry::label), for example "Phoenix Contact ST 4",
 * or an empty string if the width was typed or not set.
 * Several terminals of the catalog can have the same width,
 * the width alone doesn't say which one was chosen.
 */
QString RealTerminal::catalogEntry() const
{
	if (hasData()) {
		return terminalData().m_informations.value(catalogEntryInfoKey()).toString();
	}
	return QString();
}

/**
 * @brief RealTerminal::applyCatalogEntry
 * Store in @a informations the terminal chosen in the catalog,
 * and write its manufacturer and its reference in the manufacturer and
 * manufacturer reference informations (used by the bill of materials).
 * These two informations are only written when they are empty,
 * or when they still hold what the previous entry of the catalog wrote :
 * a value typed by the user is never overwritten.
 * @param informations : the informations of the element
 * @param entry_label : label of an entry of the catalog,
 * an empty string (or a label unknown by the catalog) removes the entry.
 */
void RealTerminal::applyCatalogEntry(DiagramContext &informations, const QString &entry_label)
{
	const auto *old_entry = TerminalCatalog::entryByLabel(
				informations.value(catalogEntryInfoKey()).toString());
	const auto *new_entry = TerminalCatalog::entryByLabel(entry_label);

	if (new_entry) {
		informations.addValue(catalogEntryInfoKey(), QString::fromUtf8(new_entry->label));
	} else if (informations.contains(catalogEntryInfoKey())) {
		informations.remove(catalogEntryInfoKey());
	}

	const auto update_ = [&informations](const QString &key, const QString &old_value, const QString &new_value)
	{
		const auto current_ = informations.value(key).toString();
		if (!current_.isEmpty() && current_ != old_value) {
			return;	//Typed by the user, keep it
		}
		if (!new_value.isEmpty()) {
			informations.addValue(key, new_value);
		} else if (!current_.isEmpty()) {
			informations.addValue(key, QString());
		}
	};

	update_(QETInformation::ELMT_MANUFACTURER,
			old_entry ? QString::fromUtf8(old_entry->manufacturer) : QString(),
			new_entry ? QString::fromUtf8(new_entry->manufacturer) : QString());
	update_(QETInformation::ELMT_MANUFACTURER_REF,
			old_entry ? TerminalCatalog::modelName(*old_entry) : QString(),
			new_entry ? TerminalCatalog::modelName(*new_entry) : QString());
}

/**
 * @brief RealTerminal::hasSeparatorAfter
 * @return true if a separator plate (partition plate) is mounted
 * after this terminal, between this terminal and the next one of the strip.
 * Used to separate groups of terminals (voltages, functions...).
 */
bool RealTerminal::hasSeparatorAfter() const {
	if (hasData()) {
		return QET::infoFlagIsTrue(
			terminalData().m_informations.value(separatorInfoKey()).toString());
	}
	return false;
}

/**
 * @brief RealTerminal::conductor
 * @return
 */
QString RealTerminal::conductor() const {
	if (m_element)
	{
		const auto conductors_{m_element->conductors()};
		if (conductors_.size()) {
			return conductors_.first()->properties().text;
		}
	}
	return QString();
}

/**
 * @brief RealTerminal::type
 * @return
 */
ElementData::TerminalType RealTerminal::type() const {
	if (hasData()) {
		return terminalData().terminalType();
	} else {
		return ElementData::TTGeneric;
	}
}

/**
 * @brief RealTerminal::function
 * @return
 */
ElementData::TerminalFunction RealTerminal::function() const {
	if (hasData()) {
		return terminalData().terminalFunction();
	} else {
		return ElementData::TFGeneric;
	}
}

/**
 * @brief RealTerminal::isLed
 * @return
 */
bool RealTerminal::isLed() const {
	if (hasData()) {
		return terminalData().terminalLed();
	} else {
		return false;
	}
}

/**
 * @brief isElement
 * @return true if this real terminal is linked to a terminal element
 */
bool RealTerminal::isElement() const {
	return m_element.isNull() ? false : true;
}

/**
 * @brief RealTerminal::isBridged
 * @return true if is bridged.
 * @sa TerminalStrip::isBridged
 */
bool RealTerminal::isBridged() const
{
	if (parentStrip()) {
		return !parentStrip()->isBridged(m_this_weak.toStrongRef()).isNull();
	} else {
		return false;
	}
}

/**
 * @brief RealTerminal::bridge
 * @return
 */
QSharedPointer<TerminalStripBridge> RealTerminal::bridge() const
{
	if (parentStrip()) {
		return parentStrip()->isBridged(m_this_weak.toStrongRef());
	} else {
		return QSharedPointer<TerminalStripBridge>();
	}
}

/**
 * @brief element
 * @return the element linked to this real terminal
 * or nullptr if not linked to an Element.
 */
Element *RealTerminal::element() const {
	return m_element.data();
}

/**
 * @brief elementUuid
 * @return if this real terminal is an element
 * in a folio, return the uuid of the element
 * else return a null uuid.
 */
QUuid RealTerminal::elementUuid() const {
	if (m_spare) {
		return m_spare_uuid;	//Used to find the terminal (bridges...) like an element uuid
	}
	if (!m_element.isNull()) {
		return m_element->uuid();
	} else {
		return QUuid();
	}
}

/**
 * @brief uuid
 * @return the identity to address this terminal with: the uuid of the
 * element of a drawn terminal (nothing changes for the bridges and the
 * commands of the terminal strip editor), or the uuid of the terminal
 * itself when it is not drawn yet, since elementUuid() is a null uuid
 * in that case.
 */
QUuid RealTerminal::uuid() const {
	if (!m_element.isNull()) {
		return m_element->uuid();
	} else {
		return m_uuid;
	}
}

/**
 * @brief RealTerminal::RealTerminal::xmlTagName
 * @return
 */
QString RealTerminal::RealTerminal::xmlTagName() {
	return QStringLiteral("real_terminal");
}

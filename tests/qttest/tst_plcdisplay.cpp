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
#include "qetapptest.h"

#include <QDomDocument>
#include <QtTest>

#include "properties/elementdata.h"

/**
	The PLC display modes: a PLC master's IO table is drawn as the
	table it always was, as a module box with the terminals on its
	sides, or as a one-line rack overview. The choice and its layouts
	travel beside the IO data and must never change a symbol which
	does not use them.
*/
class tst_plcdisplay : public QObject
{
	Q_OBJECT

	private slots:
		void theDisplayModeDefaultsToTheTable();
		void aTableSymbolSavesWithoutADisplayElement();
		void anOldFileWithoutADisplayElementStaysTheTable();
		void anUnknownDisplayModeFallsBackToTheTable();
		void theBoxDisplaySurvivesASaveAndAReload();
		void theRackDisplaySurvivesASaveAndAReload();
		void oneRowsOwnLayoutFieldsSurviveASaveAndAReload();
		void moduleLabelsSurviveASaveAndAReload();
		void theInstanceCopyKeepsTheDisplayMode();
		void theNewFieldsTakePartInTheEqualityCheck();
};

namespace {

ElementData::PlcMasterData twoRows()
{
	ElementData::PlcMasterData plc;
	ElementData::PlcIO in;
	in.type = ElementData::EntreeDigitale;
	in.address = QStringLiteral("I0.0");
	in.functionText = QStringLiteral("Start");
	ElementData::PlcIO out;
	out.type = ElementData::SortieDigitale;
	out.address = QStringLiteral("Q0.0");
	out.functionText = QStringLiteral("Motor K1");
	plc.ios << in << out;
	return plc;
}

/**
	Save \p data as a PLC master's definition and read it back.
	The reloaded data lands in \p reloaded.
*/
bool roundTripDefinition(const ElementData::PlcMasterData &plc,
						 ElementData::PlcMasterData &reloaded)
{
	ElementData data;
	data.m_type = ElementData::Master;
	data.m_master_type = ElementData::PLC;
	data.setPlcMasterData(plc);

	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("definition"));
	root.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
	root.setAttribute(QStringLiteral("link_type"), QStringLiteral("master"));
	root.appendChild(data.kindInfoToXml(document));
	document.appendChild(root);

	ElementData read_back;
	if (!read_back.fromXml(root)) {
		return false;
	}
	reloaded = read_back.plcMasterData();
	return true;
}

}

/**
	A PLC master which was never given a display mode is drawn as the
	table.
*/
void tst_plcdisplay::theDisplayModeDefaultsToTheTable()
{
	ElementData::PlcMasterData plc;
	QCOMPARE(plc.displayMode, ElementData::PlcDisplayTable);
	QCOMPARE(ElementData::plcDisplayModeFromString(QString()),
			 ElementData::PlcDisplayTable);
	QCOMPARE(ElementData::plcDisplayModeToString(ElementData::PlcDisplayTable),
			 QStringLiteral("table"));
}

/**
	A symbol which uses the plain table saves exactly as it did
	before: no display element, no module labels and no layout fields
	on the IO rows.
*/
void tst_plcdisplay::aTableSymbolSavesWithoutADisplayElement()
{
	ElementData data;
	data.m_type = ElementData::Master;
	data.m_master_type = ElementData::PLC;
	data.setPlcMasterData(twoRows());

	QDomDocument document;
	QDomElement saved = data.kindInfoToXml(document);
	auto xml_plc = saved.firstChildElement(QStringLiteral("plcMasterData"));
	QVERIFY(xml_plc.firstChildElement(QStringLiteral("display")).isNull());
	QVERIFY(xml_plc.firstChildElement(QStringLiteral("moduleLabels")).isNull());

	auto xml_ios = xml_plc.firstChildElement(QStringLiteral("plcIOs"));
	auto xml_io = xml_ios.firstChildElement(QStringLiteral("plcIO"));
	QVERIFY(!xml_io.isNull());
	QVERIFY(!xml_io.hasAttribute(QStringLiteral("module")));
	QVERIFY(!xml_io.hasAttribute(QStringLiteral("side")));
	QVERIFY(!xml_io.hasAttribute(QStringLiteral("hidden")));
}

/**
	A symbol written by an older QElectroTech, which has no display
	element, reads back as the table it always was.
*/
void tst_plcdisplay::anOldFileWithoutADisplayElementStaysTheTable()
{
	QDomDocument document;
	QVERIFY(document.setContent(QString(
		R"(<definition type="element" link_type="master">
<kindInformations>
<kindInformation name="type">plc</kindInformation>
<plcMasterData rowHeight="8.00">
<plcIOs>
<plcIO type="entree_digitale" address="I0.0" functionText="Start" terminalCount="1"/>
</plcIOs>
</plcMasterData>
</kindInformations>
</definition>)")));

	ElementData data;
	QVERIFY(data.fromXml(document.documentElement()));
	QCOMPARE(data.plcMasterData().displayMode, ElementData::PlcDisplayTable);
	QCOMPARE(data.plcMasterData().ios.size(), 1);
	QCOMPARE(data.plcMasterData().ios.first().moduleIndex, -1);
	QVERIFY(data.plcMasterData().ios.first().side.isEmpty());
	QVERIFY(!data.plcMasterData().ios.first().hidden);
}

/**
	An unknown display mode is drawn as the table, like one which was
	never chosen.
*/
void tst_plcdisplay::anUnknownDisplayModeFallsBackToTheTable()
{
	QCOMPARE(ElementData::plcDisplayModeFromString(QStringLiteral("banana")),
			 ElementData::PlcDisplayTable);
}

/**
	The module box and its layout survive a save and a reload through
	the definition of a symbol.
*/
void tst_plcdisplay::theBoxDisplaySurvivesASaveAndAReload()
{
	ElementData::PlcMasterData plc = twoRows();
	plc.displayMode = ElementData::PlcDisplayBox;
	plc.box_settings.width = 90.0;
	plc.box_settings.headerHeight = 12.0;
	plc.box_settings.showSupplyRow = false;
	plc.box_settings.splitSides = false;
	plc.box_settings.titleTemplate = QStringLiteral("%module (%type)");
	plc.box_settings.rowTemplate = QStringLiteral("%address %function");

	ElementData::PlcMasterData reloaded;
	QVERIFY(roundTripDefinition(plc, reloaded));
	QCOMPARE(reloaded.displayMode, ElementData::PlcDisplayBox);
	QVERIFY(reloaded.box_settings == plc.box_settings);
	QCOMPARE(reloaded.rack_settings, ElementData::PlcRackSettings());
}

/**
	The rack overview and its layout survive a save and a reload.
*/
void tst_plcdisplay::theRackDisplaySurvivesASaveAndAReload()
{
	ElementData::PlcMasterData plc = twoRows();
	plc.displayMode = ElementData::PlcDisplayRack;
	plc.rack_settings.slotWidth = 40.0;
	plc.rack_settings.slotHeight = 60.0;
	plc.rack_settings.showAddressRange = false;
	plc.rack_settings.titleTemplate = QStringLiteral("Rack %slot");
	plc.rack_settings.plateTemplate = QStringLiteral("%module %count");

	ElementData::PlcMasterData reloaded;
	QVERIFY(roundTripDefinition(plc, reloaded));
	QCOMPARE(reloaded.displayMode, ElementData::PlcDisplayRack);
	QVERIFY(reloaded.rack_settings == plc.rack_settings);
	QCOMPARE(reloaded.box_settings, ElementData::PlcBoxSettings());
}

/**
	One IO row's own layout fields -- the rack module it belongs to,
	the side its terminal sits on in the module box, and an unused
	channel which is kept but not drawn -- survive a save and a
	reload. A row which has none of them reads back with none.
*/
void tst_plcdisplay::oneRowsOwnLayoutFieldsSurviveASaveAndAReload()
{
	ElementData::PlcMasterData plc = twoRows();
	plc.ios[0].moduleIndex = 2;
	plc.ios[0].side = QStringLiteral("right");
	plc.ios[0].hidden = true;

	ElementData::PlcMasterData reloaded;
	QVERIFY(roundTripDefinition(plc, reloaded));
	QCOMPARE(reloaded.ios.size(), 2);
	QCOMPARE(reloaded.ios.at(0).moduleIndex, 2);
	QCOMPARE(reloaded.ios.at(0).side, QStringLiteral("right"));
	QVERIFY(reloaded.ios.at(0).hidden);
	QCOMPARE(reloaded.ios.at(1).moduleIndex, -1);
	QVERIFY(reloaded.ios.at(1).side.isEmpty());
	QVERIFY(!reloaded.ios.at(1).hidden);
	QVERIFY(reloaded.ios.at(0) == plc.ios.at(0));
}

/**
	The module labels, the names the box and the rack plates are
	drawn with, survive a save and a reload.
*/
void tst_plcdisplay::moduleLabelsSurviveASaveAndAReload()
{
	ElementData::PlcMasterData plc = twoRows();
	plc.moduleLabels.insert(0, QStringLiteral("PS 307"));
	plc.moduleLabels.insert(2, QStringLiteral("SM 321"));

	ElementData::PlcMasterData reloaded;
	QVERIFY(roundTripDefinition(plc, reloaded));
	QCOMPARE(reloaded.moduleLabels.size(), 2);
	QCOMPARE(reloaded.moduleLabels.value(0), QStringLiteral("PS 307"));
	QCOMPARE(reloaded.moduleLabels.value(2), QStringLiteral("SM 321"));
}

/**
	The copy a placed instance keeps in the project carries the
	display mode and its layouts too.
*/
void tst_plcdisplay::theInstanceCopyKeepsTheDisplayMode()
{
	ElementData::PlcMasterData plc = twoRows();
	plc.displayMode = ElementData::PlcDisplayBox;
	plc.box_settings.width = 77.0;
	plc.ios[1].moduleIndex = 3;

	ElementData data;
	data.m_type = ElementData::Master;
	data.m_master_type = ElementData::PLC;
	data.setPlcMasterData(plc);

	QDomDocument document;
	QDomElement saved = data.plcMasterDataToXml(document);

	ElementData read_back;
	read_back.plcMasterDataFromXml(saved);
	QCOMPARE(read_back.plcMasterData().displayMode, ElementData::PlcDisplayBox);
	QCOMPARE(read_back.plcMasterData().box_settings.width, 77.0);
	QCOMPARE(read_back.plcMasterData().ios.at(1).moduleIndex, 3);
}

/**
	The new fields take part in the equality check, which is what
	tells the editor that a change was made at all.
*/
void tst_plcdisplay::theNewFieldsTakePartInTheEqualityCheck()
{
	ElementData::PlcMasterData plc = twoRows();
	ElementData::PlcMasterData other = plc;
	QVERIFY(plc == other);

	other.displayMode = ElementData::PlcDisplayRack;
	QVERIFY(plc != other);
	other = plc;

	other.box_settings.width = 55.0;
	QVERIFY(plc != other);
	other = plc;

	other.moduleLabels.insert(0, QStringLiteral("CPU"));
	QVERIFY(plc != other);
	other = plc;

	other.ios[0].moduleIndex = 1;
	QVERIFY(plc != other);
	other = plc;

	other.ios[0].side = QStringLiteral("left");
	QVERIFY(plc != other);
	other = plc;

	other.ios[0].hidden = true;
	QVERIFY(plc != other);
}

QET_APP_TEST_MAIN(tst_plcdisplay)
#include "tst_plcdisplay.moc"

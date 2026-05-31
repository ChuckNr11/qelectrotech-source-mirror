#include "cablepropertieswidget.h"

#include "cablepropertiesdialog.h"
#include "qglobal.h"
#include "qlocale.h"
#include "sources/cable_assist/cabledefinitions.h"
#include "sources/cable_assist/ui/choosecabledialog.h"
#include "sources/qetinformation.h"
#include "sources/utils/qetutils.h"
#include "ui_cablepropertieswidget.h"
#include "qtextstream.h"
#include "../../elementprovider.h"

//#include "qdebug.h"
//#include <QMap>

/***************************************************************************
	DEFINITIONEN
	------------

PropertiesDialog Simple Elmt				m_cable_data

Label											|	[0]		Kennzeichnung 			ELMT_LABEL
Plant											|	[1]		Anlage 					ELMT_PLANT
Location										|	[2]		Ort 					ELMT_LOCATION
Annotation										|	[3]		Kommentar				ELMT_COMMENT
Function										|	[4]		Funktion				ELMT_FUNCTION
Textual description	(Artikelbeschreibung)		|	[5]		Voltage
Articel number									|	[6]		Kabeltyp
Manuactuerer									|	[7]		Leiterzahl
Order number									|	[8]		Marker (G, x)
Internal number									|	[9]		Querschnitt
Supplier										|	[10]	Einheit (mm,mm²,AWG)
Quantity								|
Auxilary block1							|
Auxilary 1 text description				|
Auxilary 1 article number				|
Manufacturer of Auxilary 1				|
Order number of Auxilary 1				|
Internal number of Auxilary 1			|
Supplier of Auxilary 1					|
Quantity of Auxilary 1					|
Unity of Auxilary 1						|
ELMT_AUX2								|
ELMT_DESCRIPTION_AUX2					|
ELMT_DESIGNATION_AUX2					|
ELMT_MANUFACTURER _AUX2					|
ELMT_MANUFACTURER_REF_AUX2				|
ELMT_MACHINE_MANUFACTURER_REF_AUX2		|
ELMT_SUPPLIER_AUX2						|
ELMT_QUANTITY_AUX2						|
ELMT_UNITY_AUX2							|



	# m_cable_data #											# korrespondierende QET Element Information #
	index	value												information			Bezeichnung Englisch	Bezeichnung Deutsch

	[0]		Type (Kabeltyp)										ELMT_DESCRIPTION
	[1]		Anzahl Adern (wirecount)							ELMT_DESIGNATION
	[2]		Querschnitt (Cross-section)							ELMT_QUANTITY
	[3]		Einheit	Querschnitt	(Unit)							ELMT_UNITY



	[7]		Aderreihenfolge nach Norm (indexStandard)
	[8]		Aderreihenfolge in der Zeichnung (indexDiagram)		ELMT_AUX1
			Aderkennzeichnung




ELMT_DESCRIPTION
ELMT_DESIGNATION
ELMT_MANUFACTURER
ELMT_MANUFACTURER_REF
ELMT_MACHINE_MANUFACTURER_REF
ELMT_SUPPLIER
ELMT_QUANTITY
ELMT_UNITY
ELMT_AUX1
ELMT_DESCRIPTION_AUX1
ELMT_DESIGNATION_AUX1
ELMT_MANUFACTURER _AUX1
ELMT_MANUFACTURER_REF_AUX1
ELMT_MACHINE_MANUFACTURER_REF_AUX1



****************************************************************************/

CablePropertiesWidget::CablePropertiesWidget(int wireCount, Diagram *diagram, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::CablePropertiesWidget)
{
	m_diagram = diagram;
	m_wire_count = wireCount;
	ui->setupUi(this);
	for(int i=0; i<=20; i++)
		m_cable_data.append("");

	init();
}


/**
	@brief CablePropertiesWidget::~CablePropertiesWidget
	Destructor
*/
CablePropertiesWidget::~CablePropertiesWidget()
{
	delete ui;
}

QStringList CablePropertiesWidget::cableData() const
{
	return m_cable_data;
}


/**
   @brief CablePropertiesWidget::init
*/
void CablePropertiesWidget::init()
{
	emit (disableOkButton(true));
	//setConnections(false);

	// plant
	ui->m_plant_le->setText("=");
	//m_cable_data.replace(cableData::PLANT, ui->m_plant_le->text());

	// location
	ui->m_location_le->setText("+");
	//m_cable_data.replace(cableData::LOCATION, ui->m_location_le->text());

	// label
	ui->m_label_le->setText("-W");
	ui->m_label_le->end(false);
	m_cable_data.replace(cableData::LABEL, ui->m_label_le->text());

	//  function
	ui->m_function_le->setText("");
	//m_cable_data.replace(cableData::FUNCTION, ui->m_function_le->text());

	// PE checkbox
	ui->m_pe_chb->setChecked(true);

	// type
	ui->m_cable_type_cb->insertItems(0,QETCableStandard::cableType());

	// marker
	m_cable_data.replace(cableData::MARKER, "x");

	// conductor count
	ui->m_conductor_count_le->clear();
	ui->m_conductor_count_le->setText(QString::number(m_wire_count)); // +m_cable_data.value(cableData::MARKER));
	m_cable_data.replace(cableData::COND_COUNT, QString::number(m_wire_count));


	// shield checkbox
	ui->m_schirm_chb->setChecked(false);	// CheckBox Schirm deaktivieren

	//Cross section
	ui->m_cross_section_cb->clear();
	ui->m_cross_section_cb->insertItems(0,QETCableStandard::condCrossSectionUnit(QETCableStandard::qmm));
	ui->m_cross_section_cb->setCurrentIndex(6);

	// core identification standards
	ui->m_core_identification_cb->clear();
	if(ui->m_pe_chb->isChecked()){
		for(int i=0;i< QETCableStandard::condIdentificationStandards().count();i++){
				// Standard
			ui->m_core_identification_cb->addItem(QETCableStandard::condIdentificationStandards().value(i).at(0));
				// Tooltip Standard
			ui->m_core_identification_cb->setItemData(i, QETCableStandard::condIdentificationStandards().value(i).at(1), Qt::ToolTipRole);
		}
	}
	ui->m_core_identification_cb->setCurrentIndex(2);

	  // wire marking without wire PE
	//else{
		//for(int i=0;i<m_core_identification_list_O.count();i++){
		//	ui->m_core_identification_cb->addItem(m_core_identification_list_O.value(i).at(0));
		//	ui->m_core_identification_cb->setToolTip(m_core_identification_list_O.value(i).at(1));
		//}
	//}

	parseProjectForCable();
	for(int i=0;i<m_cable_list.count();i++){
		addCableToTreewidget(m_cable_list[i]);
	}

	//addNewCable(m_cable_data);
	updateNewCable();
	updateNewCable();
//	setConnections(true);
}


/**
	@brief CablePropertiesWidget::parseProjectForCable
*/
void CablePropertiesWidget::parseProjectForCable()
{
	QList<QPointer<Element>>		aderelemente_list,	// Liste mit allen Aderelementen
									cable;	        // ein Kabel (Liste der Adern)
	QStringList						label_list;		// liste nur mit den Kabel Labels weiß noch nicht ob ich die brauche

		// Ein Kabel besteht aus einzelnen Aderelementen
		// Das Aderelement ist ein simple Element, deshalb erst einmal alle simple Elements aus dem Projekt holen
	ElementProvider ep(m_diagram->project());
	auto simpleElmt = ep.find(ElementData::Simple); // alle simple Elements aus dem Projekt

	if(simpleElmt.isEmpty())
		return;

		/*#####################
			Alle Aderelemente aus simpleElmt extrahieren.
			Gesucht wird nach BMK's desssen erster Buchstabe ein W oder w ist,
			vor dem W dürfen sich andere Zeichen oder Zahlen befinden, zB -W101 oder 3w200
		######################*/
	for(int i=0; i<simpleElmt.count();i++){
		QString str{""};
			//
		str = simpleElmt[i]->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString();
		if(!str.isEmpty()){
				// prüfen ob erster Buchstabe ein 'w' oder 'W' ist
			for(int k=0;k<str.count();k++){
				QChar zeichen = str.at(k);
				// eigentlich müssten hier auch auf die Variablenkennung '%'
				// geprüft werden. Beim Testen kam der Fall vor, und
				// die betroffenen Kabel wurden nicht erfasst
				if(zeichen.isLetter()){
					if(zeichen=="w" || zeichen=="W"){
						aderelemente_list.append(simpleElmt[i]);
					}
				break;
				}
			}

		}
	}

	if(aderelemente_list.isEmpty())
		return;

		// Alle Adern in aderelemente_list nach label sortieren
	std::sort(aderelemente_list.begin(), aderelemente_list.end(),
			  [](Element *a, Element *b)
			  {
				  return QETUtils::sortBeginIntString(a->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString(),
													  b->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString());
			  });

		// Liste der Kabel erstellen.
		// Ein Kabel ist die Liste von Aderelementen mit gleichen Label
	QString cable_label, old_cable_label{""};
	old_cable_label = aderelemente_list[0]->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString();
	for(int i=0; i<aderelemente_list.count(); i++){
		bool last_wire=false;

		if(i==aderelemente_list.count()-1)
			last_wire=true;

			// label der Ader holen
		cable_label = aderelemente_list[i]->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString();

			// Alle Aderelemente mit gleichen label gehören zu einem Kabel
		if(cable_label==old_cable_label && !last_wire){
			cable.append(aderelemente_list[i]);
		}
		else{		
				// Wenn es ist das Letzte Element aus aderelemente_list ist
				// muss es noch zur liste cable hinzugefügt werden.
				//
			if(last_wire){
				cable.append(aderelemente_list[i]);
			}

				// fertiges Kabel in die Kabelliste übernehmen
			m_cable_list << cable;

				// Kabel leeren
			cable.clear();
			if(!last_wire){
				// neues Kabel beginnen
				cable.append(aderelemente_list[i]);
				old_cable_label = cable_label;
			}
		}
	}
}


void CablePropertiesWidget::addCableToTreewidget(QList<QPointer<Element> > cable)
{
  // fertiges Kabel in das TreeWidget einfügen
  // erstmal nach der Reihenfolge der Adern im Diagram sortiern
  // Indikator dafür ist der index aus dem Eigenschaftsfeld 'interne Nummer'
  // interne Nummer = m_informations:ELMT_MACHINE_MANUFACTURER_REF
	std::sort(cable.begin(), cable.end(),
			  [](Element *a, Element *b)
			  {
				  return QETUtils::sortBeginIntString(a->elementData().m_informations.value(QETInformation::ELMT_AUX1).toString(),
													  b->elementData().m_informations.value(QETInformation::ELMT_AUX1).toString());
			  });

		// Die TopLevel Zeile mit den Kabeldaten füllen
	QTreeWidgetItem *cable_item = new QTreeWidgetItem(
		ui->m_treeWidget,
		QStringList() <<	  cable[0]->elementData().m_informations.value(QETInformation::ELMT_PLANT).toString()
							+ cable[0]->elementData().m_informations.value(QETInformation::ELMT_LOCATION).toString()
							+ cable[0]->elementData().m_informations.value(QETInformation::ELMT_LABEL).toString()
							+ " "
							+ cable[0]->elementData().m_informations.value(QETInformation::ELMT_FUNCTION).toString()
							+ " "
							+ cable[0]->elementData().m_informations.value(QETInformation::ELMT_DESCRIPTION).toString());

	cable_item->setFirstColumnSpanned(true);
	cable_item->setDisabled(true);

		   // Die Adern als Kind vom cable_item in das Treewidget übernehmen
	for(int i=0; i < cable.count(); i++){
		QTreeWidgetItem *ader = new QTreeWidgetItem(cable_item);
		ader->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		ader->setCheckState(1, Qt::Checked );
		ader->setText(0,cable[i]->elementData().m_informations.value(QETInformation::ELMT_MACHINE_MANUFACTURER_REF).toString());
		ader->setText(1,cable[i]->elementData().m_informations.value(QETInformation::ELMT_DESIGNATION).toString());
	}
}

/*void CablePropertiesWidget::addNewCable(QStringList cable_data)
{
	if(cable_data.value(cableData::condCount).isEmpty() || cable_data.value(cableData::condCount)==0)
		return;

		// Die TopLevel Zeile mit den Kabeldaten füllen
	m_new_cable = new QTreeWidgetItem(
		ui->m_treeWidget,
		QStringList() << cable_data.value(cableData::condCount));

	m_new_cable->setFirstColumnSpanned(true);
	m_new_cable->setExpanded(true);

		   // Die Adern als Kind vom cable_item in das Treewidget übernehmen
	for(int i=0; i < cable_data.value(cableData::condCount).toInt(); i++){
		QTreeWidgetItem *ader = new QTreeWidgetItem(m_new_cable);
		ader->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		ader->setText(0,QString::number(i+1));
		ader->setCheckState(1, Qt::Checked );
	}
}*/

void CablePropertiesWidget::updateNewCable()
{
	if(m_cable_data.value(cableData::COND_COUNT).isEmpty() || m_cable_data.value(cableData::COND_COUNT)==0)
		return;

		   // Falls Kabel nicht existiert, neues Kabel erstellen
	if(!m_new_cable){
		m_new_cable = new QTreeWidgetItem(ui->m_treeWidget);
		m_new_cable->setFirstColumnSpanned(true);
		m_new_cable->setExpanded(true);
	}

		// Die Adern als Kind vom cable_item in das Treewidget übernehmen
	if(m_new_cable->childCount()!= m_cable_data.value(cableData::COND_COUNT).toInt()){
		for(int i=0; i < m_new_cable->childCount(); i++){
			m_new_cable->removeChild(m_new_cable->child(i));
		}
		for(int i=0; i < m_cable_data.value(cableData::COND_COUNT).toInt(); i++){
			QTreeWidgetItem *ader = new QTreeWidgetItem(m_new_cable);
			ader->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
			ader->setText(0,QString::number(i+1));
			ader->setCheckState(1, Qt::Checked );
		}
	}

	m_new_cable->setText(0,
						  m_cable_data.value(cableData::PLANT)
						+ " "
						+ m_cable_data.value(cableData::LOCATION)
						+ " "
						+ m_cable_data.value(cableData::LABEL)
						+ "    "
						+ m_cable_data.value(cableData::TYPE)
						+ " "
						+ m_cable_data.value(cableData::COND_COUNT)
						+ m_cable_data.value(cableData::MARKER)
						+ m_cable_data.value(cableData::CROSS_SECTION)
						+ m_cable_data.value(cableData::UNIT));
}

void CablePropertiesWidget::textHasChanged()//const QString &text)
{
	//Q_UNUSED(text)
	m_cable_data.replace(cableData::PLANT, ui->m_plant_le->text());
	m_cable_data.replace(cableData::LOCATION, ui->m_location_le->text());
	m_cable_data.replace(cableData::LABEL, ui->m_label_le->text());
	m_cable_data.replace(cableData::FUNCTION, ui->m_function_le->text());
	m_cable_data.replace(cableData::VOLTAGE, ui->m_voltage_cb->currentText());
	updateNewCable();
}


void CablePropertiesWidget::on_m_conductor_count_le_textChanged(const QString &text)
{
		// Catching incorrect characters during manual input
		// Ensure correct syntax: 5x or 5G or 4x2x .
	QString str = text;
	QList <QChar>validCharacters={'0','1','2','3','4','5','6','7','8','9','x','G'};
	for (qsizetype i = 0; i < str.size(); ++i) {
		// Delete invalid characters
		if (!validCharacters.contains(str.at(i))){
			str.remove(i,1);
			ui->m_conductor_count_le->setText(str);
			//continue;
		}
	}
			// Buchstabe an erster Stelle unterdrücken
	if(str.startsWith('x') || str.startsWith('G')){
		ui->m_conductor_count_le->setText(str.remove(0,1));
	}
			// delete a second 'x' or 'G'
		else{
			if (str.count('x')>1 || str.count('G')>1){
				str.remove(ui->m_conductor_count_le->cursorPosition()-1,1);
				ui->m_conductor_count_le->setText(str);
			}
		}

	  // The cable must contain at least 'm_wire_count' or more  conductors
	if(str.count()==1 && str == "0")
		str="1";

		   // if text contains an x,
		   // calculate conductors count
	//if(str.count('x')==1){
	if((str.endsWith('x') && str.count('x')==1 )|| str.endsWith('G')){
		QString number;
		qsizetype x = str.indexOf('x');
		for(qsizetype i=0; i<x; i++)
			number.append(str.at(i));
		m_wire_count = number.toInt();
	}
	//else if(){

	//}
	else if(ui->m_conductor_count_le->text().isEmpty())
		m_wire_count = 0;
	else
		m_wire_count = ui->m_conductor_count_le->text().toInt();

	m_cable_data.replace(1 ,QString::number(m_wire_count));

}
/*void CablePropertiesWidget::setConnections(bool activate)
{
	if(activate)
	{
		connect(ui->m_plant_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_plant_le_textChanged);
		connect(ui->m_location_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_location_le_textChanged);
		connect(ui->m_label_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_label_le_textChanged);
		connect(ui->m_function_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_function_le_textChanged);
		connect(ui->m_cable_type_cb, &QComboBox::editTextChanged, this, &CablePropertiesWidget::on_m_cable_type_cb_editTextChanged);
		connect(ui->m_pe_chb, &::QCheckBox::checkState, this, &CablePropertiesWidget::on_m_pe_chb_checkStateChanged);
		//connect(ui->m_cable_type_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			//	this, &CablePropertiesWidget::on_m_cable_type_cb_editTextChanged);
		connect(ui->m_conductor_count_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_conductor_count_le_textChanged);
		connect(ui->m_core_identification_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				this, &CablePropertiesWidget::on_m_core_identification_cb_currentIndexChanged);

	}
	else
	{
		disconnect(ui->m_plant_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_plant_le_textChanged);
		disconnect(ui->m_location_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_location_le_textChanged);
		disconnect(ui->m_label_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_label_le_textChanged);
		disconnect(ui->m_function_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_function_le_textChanged);
		disconnect(ui->m_cable_type_cb, &QComboBox::editTextChanged, this, &CablePropertiesWidget::on_m_cable_type_cb_editTextChanged);
		disconnect(ui->m_pe_chb, &QCheckBox::stateChanged, this, &CablePropertiesWidget::on_m_pe_chb_stateChanged);
		//disconnect(ui->m_cable_type_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			//	this, &CablePropertiesWidget::on_m_cable_type_cb_editTextChanged);
		disconnect(ui->m_conductor_count_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::on_m_conductor_count_le_textChanged);
		disconnect(ui->m_core_identification_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				   this, &CablePropertiesWidget::on_m_core_identification_cb_currentIndexChanged);
	}
}


void CablePropertiesWidget::on_m_plant_le_textChanged(const QString &text)
{
	m_cable_data.replace(cableData::PLANT, text);
	updateNewCable();
}


void CablePropertiesWidget::on_m_location_le_textChanged(const QString &text)
{
	m_cable_data.replace(cableData::LOCATION, text);
	updateNewCable();
}


void CablePropertiesWidget::on_m_label_le_textChanged(const QString &text)
{
	m_cable_data.replace(cableData::LABEL, text);
	updateNewCable();
}


void CablePropertiesWidget::on_m_function_le_textChanged(const QString &text)
{
	m_cable_data.replace(cableData::FUNCTION, text);
	updateNewCable();
}


void CablePropertiesWidget::on_m_cable_type_cb_editTextChanged(const QString &text)
{

}



	   // QT6 on_m_pe_chb_checkStateChanged(int arg1)
void CablePropertiesWidget::on_m_pe_chb_checkStateChanged(const Qt::CheckState &arg1)
{

}


void CablePropertiesWidget::on_m_cross_section_cb_currentIndexChanged(int index)
{

}



void CablePropertiesWidget::on_m_mm_rb_toggled(bool checked)
{

}


void CablePropertiesWidget::on_m_qmm_rb_toggled(bool checked)
{

}


void CablePropertiesWidget::on_m_awg_rb_toggled(bool checked)
{

}


void CablePropertiesWidget::on_m_core_identification_cb_currentIndexChanged(int index)
{

}



void CablePropertiesWidget::on_m_schirm_chb_checkStateChanged(const Qt::CheckState &arg1)
{

}*/





void CablePropertiesWidget::on_m_newcable__tb_clicked()
{
	ChooseCableDialog ccd (m_wire_count,this);
}


#include "cablepropertieswidget.h"

#include "cablepropertiesdialog.h"
#include "qglobal.h"
#include "qlocale.h"
#include "ui_cablepropertieswidget.h"

//#include "qdebug.h"
//#include <QMap>


CablePropertiesWidget::CablePropertiesWidget(int wireCount, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::CablePropertiesWidget)
{
	m_wire_count = wireCount;
	ui->setupUi(this);
	for(int i=0; i<=9; i++)
		m_cable_data.insert(i,"");

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
   @brief CablePropertiesWidget::cable_type_currentTextChanged
	Slot comboBox "m_cable_type"
   @param text
*/
void CablePropertiesWidget::cable_type_editTextChanged(const QString &text)
{
	disconnect(ui->m_core_identification_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			   this, &CablePropertiesWidget::core_identification_currentIndexChanged);

	NORM norm = m_cable_type_map[ui->m_cable_type_cb->currentText()];

	switch (norm){

	case NORM::DIN_47100:
		ui->m_core_identification_cb->setCurrentIndex(NORM::DIN_47100);
		break;

		// VDE0293-308
	case NORM::VDE_0293_308_S2:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0293_308_S2);
		break;

	case NORM::VDE_0293_308_S1_massive:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0293_308_S1_massive);
		break;

	case NORM::VDE_0293_308_S1_flex:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0293_308_S1_flex);
		break;

		// VDE0293
	case NORM::VDE_0293_color:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0293_color);
		break;

	case NORM::VDE_0815:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0815);
		break;

	case NORM::VDE_0815_2x2:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0815);
		break;

	case NORM::VDE_0816_star_quad:
		ui->m_core_identification_cb->setCurrentIndex(NORM::VDE_0816_star_quad);
		break;

	case NORM::Zahlen:
		ui->m_core_identification_cb->setCurrentIndex(NORM::Zahlen);
		break;

	default:
		ui->m_core_identification_cb->setCurrentIndex(NORM::keine);
	}

	m_cable_data.replace(0 ,text);
	updateTreeWidget();

	connect(ui->m_core_identification_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			this, &CablePropertiesWidget::core_identification_currentIndexChanged);

}

/**
   @brief CablePropertiesWidget::on_m_pe_chb_stateChanged
   @param arg1
*/
void CablePropertiesWidget::on_m_pe_chb_stateChanged(int arg1)
{
	Q_UNUSED(arg1)

	updateTreeWidget();
}

void CablePropertiesWidget::wire_count_textChanged(const QString &text)
{

		// Catching incorrect characters during manual input
		// Ensure correct syntax: a single number or 2 × n (m × n).
	QString str = text;
	QList <QChar>validCharacters={'0','1','2','3','4','5','6','7','8','9','x'};
	for (qsizetype i = 0; i < str.size(); ++i) {
			// Delete invalid characters
		if (!validCharacters.contains(str.at(i))){
			str.remove(i,1);
			ui->m_wire_count_le->setText(str);
			continue;
		}
		else if(str.startsWith('x')){
			ui->m_wire_count_le->setText("");
			continue;
		}
			// delete a second 'x'
		else{
			if (str.count('x')>1){
				str.remove(ui->m_wire_count_le->cursorPosition()-1,1);
				ui->m_wire_count_le->setText(str);
			}
		}
	}
		// The cable must contain at least one conductor
	if(str.count()==1 && str == "0")
		str="1";

		// if text contains an x,
		// calculate conductors count
	if(str.count('x')==1){
		QString num1,num2;
		qsizetype x = str.indexOf('x');

		for(qsizetype i=0; i<x; i++)
			num1.append(str.at(i));

		for(qsizetype i=x+1; i<str.size(); i++)
			num2.append(str.at(i));

		m_wire_count = num1.toInt()*num2.toInt();
	}
	else if(ui->m_wire_count_le->text().isEmpty())
		m_wire_count = 0;
	else
		m_wire_count = ui->m_wire_count_le->text().toInt();

	m_cable_data.replace(1 ,QString::number(m_wire_count));

	updateTreeWidget();
}

void CablePropertiesWidget::cross_section_currentIndexChanged(int index)
{
	if(ui->m_mm_rb->isChecked()){
		m_cable_data.replace(2 ,m_mm_list.value(index));
		m_cable_data.replace(3, "mm");
	}
	else if(ui->m_qmm_rb->isChecked()){
		m_cable_data.replace(2 ,m_qmm_list.value(index));
		m_cable_data.replace(3, "mm²");
	}
	else{
		m_cable_data.replace(2 ,m_awg_list.value(index));
		m_cable_data.replace(3, "AWG");
	}
	updateTreeWidget();
}

void CablePropertiesWidget::unit_mm_toggled(bool checked)
{
	if(checked){
		ui->m_cross_section_cb->clear();
		ui->m_cross_section_cb->insertItems(0,m_mm_list);
		ui->m_cross_section_cb->setCurrentIndex(0);
		m_cable_data.replace(3, "mm");
	}
	updateTreeWidget();
}

void CablePropertiesWidget::unit_qmm_toggled(bool checked)
{
	if(checked){
		ui->m_cross_section_cb->clear();
		ui->m_cross_section_cb->insertItems(0,m_qmm_list);
		ui->m_cross_section_cb->setCurrentIndex(0);
		m_cable_data.replace(3, "mm²");
	}
	updateTreeWidget();
}

void CablePropertiesWidget::unit_awg_toggled(bool checked)
{
	if(checked){
		ui->m_cross_section_cb->clear();
		ui->m_cross_section_cb->insertItems(0,m_awg_list);
		ui->m_cross_section_cb->setCurrentIndex(0);
		m_cable_data.replace(3, "mm²");
	}
	m_cable_data.replace(3, "AWG");
	updateTreeWidget();
}


void CablePropertiesWidget::core_identification_currentIndexChanged(int index)
{
	Q_UNUSED(index)

	updateTreeWidget();
}

void CablePropertiesWidget::plant_textChanged(const QString &text)
{
	m_cable_data.replace(4, text);
	updateTreeWidget();
}

void CablePropertiesWidget::location_textChanged(const QString &text)
{
	m_cable_data.replace(5, text);
	updateTreeWidget();
}

void CablePropertiesWidget::label_textChanged(const QString &text)
{
	m_cable_data.replace(6, text);
	updateTreeWidget();
}


/**
   @brief CablePropertiesWidget::updateCableData
*/
void CablePropertiesWidget::updateCableData()
{
	// cableData:
	// ([0]Typ, [1]Wire, [2]Cross-section, [3]Unit, [4]Plant, [5]Location, [6]Label, [7] index, [8])

	qsizetype i = 0; //cabletyp
	m_cable_data.replace(i ,ui->m_cable_type_cb->currentText());

	i = 1; //wire count
	m_cable_data.replace(i ,QString::number(m_wire_count));

	i = 2; // Cross-section
	m_cable_data.replace(i ,ui->m_cross_section_cb->currentText());

	i = 3; // Cross-section unit
	if(ui->m_cross_section_cb->currentText().isEmpty())
			// if no unit is set, unit is empty
		m_cable_data.replace(i ,"");
	else{
		if (ui->m_mm_rb->isChecked())
			m_cable_data.replace(i ,"mm");
		else if (ui->m_qmm_rb->isChecked())
			m_cable_data.replace(i ,"mm²");
		else
			m_cable_data.replace(i ,"AWG");
	}

	i=6; // BMK
	m_cable_data.replace(i,ui->m_label_le->text());


}

/**
   @brief CablePropertiesWidget::updateTreeWidget
*/
void CablePropertiesWidget::updateTreeWidget()
{
		//qsizetype i = 0;
	ui->m_treeWidget->clear();

	QString plant,location;
	if(ui->m_plant_le->text().right(1)== "=" && ui->m_plant_le->text().count() == 1 )
		plant="";
	else
		plant=ui->m_plant_le->text();
	if(ui->m_location_le->text().right(1)== "+" && ui->m_location_le->text().count() == 1 )
		location="";
	else
		location=ui->m_location_le->text();

		// create TopLevel Item
		// insert Label to TopLevelItem
	kabel = new QTreeWidgetItem(
		ui->m_treeWidget,
		QStringList() << plant
							 + " "
							 + location
							 + " "
							 + m_cable_data.value(6)
							 + "    "
							 + m_cable_data.value(0)
							 + " "
							 + m_cable_data.value((1))
							 + "x"
							 + m_cable_data.value(2)
							 + m_cable_data.value(3)
		);
		// TopLevelItem expansieren damit der Kabelname komplett hineinpasst
		// expand to fit label
	kabel->setFirstColumnSpanned(true);

		//
	if(m_wire_count == 0)
		return;

	QList<QString> kennzeichnung;

		   // Entsprechend der Norm aus der Normen ComboBox werden die Adern beschriftet,
		   // oder wenn die Normen ComboBox leer ist, wird die Anzahl der Adern mit laufender
		   // Nummer dargestellt und man kann die Bezeichnung der Adern manuell vornehmen
	//if(!ui->m_core_identification_cb->currentText().isEmpty()){
	QList<QString> vde0815_from_4x2 = {"ws","bl","ws","ge","ws","gn","ws","bn","ws","sw"};
	QList<QString> vde0816_star_quad_color_ab = {"rt-1a","rt-1b","rt-2a","rt-2b",
												 "gn-1a","gn-1b","gn-2a","gn-2b",
												 "gr-1a","gr-1b","gr-2a","gr-2b",
												 "ge-1a","ge-1b","ge-2a","ge-2b",
												 "ws-1a","ws-1b","ws-2a","ws-2b"};
	QList<QString> vde0816_star_quad_ring =		{"rt-0","rt-1","rt-2w","rt-2d",
												 "gn-0","gn-1","gn-2w","gn-2d",
												 "gr-0","gr-1","gr-2w","gr-2d",
												 "ge-0","ge-1","ge-2w","ge-2d",
												 "ws-0","ws-1","ws-2w","ws-2d"};
	int index = ui->m_core_identification_cb->currentIndex();
	int k = 0;
	int l = 1;
	switch(index){

		// DIN47100
	case NORM::DIN_47100:
		kennzeichnung = {"ws","bn","gn","ge","gr","rs","bl","rt","sw","vio",
						 "gr-rs","rt-bl","ws-gn","bn-gn","ws-ge","ge-bn","ws-gr","gr-bn","ws-rs","rs-bn",
						 "ws-bl","bn-bl","ws-rt","bn-rt","ws-sw","bn-sw","gr-gn","ge-gn","rs-gn","ge-rs",
						 "gn-bl","ge-bl","gn-rt","ge-rt","gn-sw","ge-sw","gr-bl","rs-bl","gr-rt","rs-rt",
						 "gr-sw","rs-sw","bl-sw","rt-sw"};
		erstelleAdern(kennzeichnung);
		break;

		// VDE0293-308
	case NORM::VDE_0293_308_S2:
		if(ui->m_pe_chb->isChecked()){
			QList<QList<QString>> vde0293_308_s2_j = {{"gnge"},{},{"bn","bl","gnge"},{"bn","sw","gr","gnge"},{"bn","sw","gr","bl","gnge"},{"Zahlen"}};
			if(m_wire_count<=5){
				kennzeichnung=vde0293_308_s2_j.at(m_wire_count-1);
				erstelleAdern(kennzeichnung);
			}
			else{
				for(qsizetype i=0;i<m_wire_count;++i){
					if(i<m_wire_count-2)
						kennzeichnung.append(QString::number(i));
					else
						kennzeichnung.append("gnge");
				}
				erstelleAdern(kennzeichnung);
			}
		}
		else{
			QList<QList<QString>> vde0293_308_s2_o = {{"gnge"},{"bn","bl"},{"bn","sw","gr"},{"bn","sw","gr","bl"},{"bn","sw","gr","bl","sw"},{"Zahlen"}};
			if(m_wire_count<=5){
				kennzeichnung=vde0293_308_s2_o.at(m_wire_count-1);
				erstelleAdern(kennzeichnung);
			}
			else{
				for(qsizetype i=0;i<m_wire_count;++i){
					kennzeichnung.append(QString::number(i));
				}
				erstelleAdern(kennzeichnung);
			}
		}
		break;

	case NORM::VDE_0293_308_S1_massive:
		if(ui->m_pe_chb->isChecked()){
			QList<QList<QString>> vde0293_308_s1_massive_j	= {{"gnge"},{"sw","gnge"},{"sw","bl","gnge"},{"sw","bn","bl","gnge"},{"sw","bn","sw","bl","gnge"},{"Zahlen"}};
			if(m_wire_count<=5){
				kennzeichnung=vde0293_308_s1_massive_j.at(m_wire_count-1);
				erstelleAdern(kennzeichnung);
			}
			else{
				for(qsizetype i=0;i<m_wire_count;++i){
					if(i<m_wire_count-2)
						kennzeichnung.append(QString::number(i));
					else
						kennzeichnung.append("gnge");
				}
				erstelleAdern(kennzeichnung);
			}
		}
		else{
			QList<QList<QString>> vde0293_308_s1_massive_o = {{"gnge"},{"sw","bl"},{"sw","bn","bl"},{"sw","bn","sw","bl"},{"sw","bn","sw","bl","sw"},{"Zahlen"}};
			if(m_wire_count<=5){
				kennzeichnung=vde0293_308_s1_massive_o.at(m_wire_count-1);
				erstelleAdern(kennzeichnung);
			}
			else{
				for(qsizetype i=0;i<m_wire_count;++i){
					kennzeichnung.append(QString::number(i+1));
				}
				erstelleAdern(kennzeichnung);
			}
		}
		break;

	case NORM::VDE_0293_308_S1_flex:
		if(ui->m_pe_chb->isChecked()){
			QList<QList<QString>> vde0293_308_s1_flex_j	= {{"gnge"},{},{"bn","bl","gnge"},{"bn","sw","bl","gnge"},{"sw","bn","sw","bl","gnge"},{"Zahlen"}};
			if(m_wire_count<=5){
				kennzeichnung=vde0293_308_s1_flex_j.at(m_wire_count-1);
				erstelleAdern(kennzeichnung);
			}
			else{
				for(qsizetype i=0;i<m_wire_count;++i){
					if(i<m_wire_count-2)
						kennzeichnung.append(QString::number(i));
					else
						kennzeichnung.append("gnge");
				}
				erstelleAdern(kennzeichnung);
			}
		}
		else{
			QList<QList<QString>> vde0293_308_s1_flex_o = {{"gnge"},{"bn","bl"},{"sw","bn","bl"},{"sw","bn","sw","bl"},{"sw","bn","sw","bl","sw"},{"Zahlen"}};
			if(m_wire_count<=5){
				kennzeichnung=vde0293_308_s1_flex_o.at(m_wire_count-1);
				erstelleAdern(kennzeichnung);
			}
			else{
				for(qsizetype i=0;i<m_wire_count;++i){
					kennzeichnung.append(QString::number(i));
				}
				erstelleAdern(kennzeichnung);
			}
		}
		break;

			   // VDE0293
	case NORM::VDE_0293_color:
		if(ui->m_pe_chb->isChecked()){
			kennzeichnung={"gnge","ws","sw","bl","bn","gr","rt","vio","rs","tr","bg",
							 "sw-ws","bn-ws","gr-ws","rt-ws","vio-ws","rs-ws","org-ws","tr-ws","bg-ws",
							 "bl-sw","bn-sw","gr-sw","rt-sw","vio-sw","rs-sw","org-sw","tr-sw","bg-sw",
							 "bl-bl","bn-bl","gr-bl","rt-bl","vio-bl","rs-bl","org-bl","tr-bl","bg-bl",
							 "gr-bn","rt-bn","vio-bn","rs-bn","org-bn","tr-bn","bg-bn",
							 "rt-gr","vio-gr","rs-gr","org-gr","tr-gr","bg-gr",
							 "org-rt","tr-rt","bg-rt","rs-vio","org-vio","tr-vio","bg-vio",
							 "tr-rs","bg-rs","tr-org","bg-org"};
			erstelleAdern(kennzeichnung);
		}
		else{
			kennzeichnung={"ws","sw","bl","bn","gr","rt","vio","rs","tr","bg",
							 "sw-ws","bn-ws","gr-ws","rt-ws","vio-ws","rs-ws","org-ws","tr-ws","bg-ws",
							 "bl-sw","bn-sw","gr-sw","rt-sw","vio-sw","rs-sw","org-sw","tr-sw","bg-sw",
							 "bl-bl","bn-bl","gr-bl","rt-bl","vio-bl","rs-bl","org-bl","tr-bl","bg-bl",
							 "gr-bn","rt-bn","vio-bn","rs-bn","org-bn","tr-bn","bg-bn",
							 "rt-gr","vio-gr","rs-gr","org-gr","tr-gr","bg-gr",
							 "org-rt","tr-rt","bg-rt","rs-vio","org-vio","tr-vio","bg-vio",
							 "tr-rs","bg-rs","tr-org","bg-org"};
			for(qsizetype i=0;i<m_wire_count;++i){
				if(i<m_wire_count-2)
					kennzeichnung.append(QString::number(i));
				else
					kennzeichnung.append("gnge");
			}
			erstelleAdern(kennzeichnung);				erstelleAdern(kennzeichnung);
		}
		break;


	case NORM::VDE_0815_2x2:
		kennzeichnung = {"rt","sw","ws","ge"};
		erstelleAdern(kennzeichnung);
		break;

	case NORM::VDE_0815:
		for(int i=0; i<m_wire_count;++i){
			if(i==0){
				kennzeichnung.append("rt");
				k++;
				continue;
			}
			else if(m_wire_count==20 && i==16){
				kennzeichnung.append("rt");
				++k;
				continue;
			}
			else if(m_wire_count==32 && i==23){
				kennzeichnung.append("rt");
				++k;
				continue;
			}
			else if(m_wire_count==40 && i==26){
				kennzeichnung.append("rt");
				++k;
				continue;
			}
			else if(m_wire_count==48 && (i==28 || i==45)){
				kennzeichnung.append("rt");
				++k;
				continue;
			}
			else if(m_wire_count==100 && (i==42 || i==72 || i==92)){
				kennzeichnung.append("rt");
				++k;
				continue;
			}
			else{
				kennzeichnung.append(vde0815_from_4x2.at(k));
				++k;
				if(k==10)
					k=0;
			}
		}
		erstelleAdern(kennzeichnung);
		break;

	case NORM::VDE_0816_star_quad_ab:
		for(int i=0;i<m_wire_count;++i){
			if(k==0){
				kennzeichnung.append(QString::number(l)+"a");
				k=1;
			}
			else{
				kennzeichnung.append(QString::number(l)+"b");
				k=0;
			}
		}
		erstelleAdern(kennzeichnung);
		break;

	case NORM::VDE_0816_star_quad_color_ab:
		for(int i=0; i<m_wire_count;++i){
			kennzeichnung.append(vde0816_star_quad_color_ab.at(k));
			++k;
			if(k==19)
				k=0;
		}
		erstelleAdern(kennzeichnung);
		break;

	case NORM::VDE_0816_star_quad:
		for(int i=0; i<m_wire_count;++i){
			kennzeichnung.append(vde0816_star_quad_ring.at(k));
			++k;
			if(k==19)
				k=0;
		}
		erstelleAdern(kennzeichnung);
		break;

	case NORM::Zahlen:
		for(qsizetype i=0;i<m_wire_count;++i){
			if(i<m_wire_count-1)
				kennzeichnung.append(QString::number(i+1));
			else
				if(ui->m_pe_chb->isChecked())
					kennzeichnung.append("gnge");
				else
					kennzeichnung.append(QString::number(i+1));
		}
		erstelleAdern(kennzeichnung);
		break;
	case NORM::keine:
		for(qsizetype i=0;i<m_wire_count;++i){
			kennzeichnung.append("");
		}
		erstelleAdern(kennzeichnung);
		break;

	default:
		for(qsizetype i=0;i<m_wire_count;++i){
			kennzeichnung.append("");
		}
		erstelleAdern(kennzeichnung);
		break;
	}

		// expand TopLevelItem[0]
	ui->m_treeWidget->expandItem(ui->m_treeWidget->topLevelItem(0));

		   //
	bool type, cross, core;
	type = !ui->m_cable_type_cb->currentText().isEmpty();
	cross= !ui->m_cross_section_cb->currentText().isEmpty();
	core = !ui->m_core_identification_cb->currentText().isEmpty();

	if(type && cross && core){
		emit (disableOkButton(false));
	}
	else{
		emit (disableOkButton(true));
	}

}

void CablePropertiesWidget::activeConnections(bool active)
{
	if (active)
	{
		connect(ui->m_cable_type_cb, &QComboBox::editTextChanged, this, &CablePropertiesWidget::cable_type_editTextChanged);
		connect(ui->m_wire_count_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::wire_count_textChanged);
		connect(ui->m_cross_section_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				this, &CablePropertiesWidget::cross_section_currentIndexChanged);
		connect(ui->m_mm_rb, &QRadioButton::toggled, this, &CablePropertiesWidget::unit_mm_toggled);
		connect(ui->m_qmm_rb, &QRadioButton::toggled, this, &CablePropertiesWidget::unit_qmm_toggled);
		connect(ui->m_awg_rb, &QRadioButton::toggled, this, &CablePropertiesWidget::unit_awg_toggled);
		connect(ui->m_core_identification_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				this, &CablePropertiesWidget::core_identification_currentIndexChanged);
		connect(ui->m_plant_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::plant_textChanged);
		connect(ui->m_location_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::location_textChanged);
		connect(ui->m_label_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::label_textChanged);
	}
	else
	{
		disconnect(ui->m_cable_type_cb, &QComboBox::editTextChanged, this, &CablePropertiesWidget::cable_type_editTextChanged);
		disconnect(ui->m_wire_count_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::wire_count_textChanged);
		disconnect(ui->m_cross_section_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				   this, &CablePropertiesWidget::cross_section_currentIndexChanged);
		disconnect(ui->m_mm_rb, &QRadioButton::toggled, this, &CablePropertiesWidget::unit_mm_toggled);
		connect(ui->m_qmm_rb, &QRadioButton::toggled, this, &CablePropertiesWidget::unit_qmm_toggled);
		connect(ui->m_awg_rb, &QRadioButton::toggled, this, &CablePropertiesWidget::unit_awg_toggled);
		disconnect(ui->m_core_identification_cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				   this, &CablePropertiesWidget::core_identification_currentIndexChanged);
		disconnect(ui->m_plant_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::plant_textChanged);
		disconnect(ui->m_location_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::location_textChanged);
		disconnect(ui->m_label_le, &QLineEdit::textChanged, this, &CablePropertiesWidget::label_textChanged);

	}
}

/**
   @brief CablePropertiesWidget::erstelleAdern
   @param list
*/
void CablePropertiesWidget::erstelleAdern(QList<QString> list)
{
	QList<QString> liste;
	if(list.isEmpty()){
		for(int i = 0;i<=m_wire_count-1;++i){
			liste.insert(i,"Norm und Aderzahl passen nicht");
		}
	}
	else
		liste=list;

	if(m_cable_data.count()>10)
		while(m_cable_data.count()>10)
			m_cable_data.removeLast();

	for(int i=0; i <= m_wire_count-1; i++){
		QTreeWidgetItem *ader = new QTreeWidgetItem(kabel);
		ader->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		ader->setCheckState(0, Qt::Checked );
		ader->setText(0,QString(QString::number(i+1)));
		ader->setText(1,liste.at(i));

		m_cable_data.append(liste.at(i));
	}
}

/**
   @brief CablePropertiesWidget::init
*/
void CablePropertiesWidget::init()
{
		//deactivate slots during initialization
	activeConnections(false);

		// Contents of the "Cable Type" ComboBox as a QMap
		// The key of the QMap (the string) is the content of the ComboBox,
		// the value is an enum and the connection to the norm
	m_cable_type_map[""]=NORM::keine;
	m_cable_type_map["A2Y(L)2Y"]=NORM::VDE_0816_star_quad;
	m_cable_type_map["H07RN-F"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["J-H(St)H"]=NORM::VDE_0815;
	m_cable_type_map["J-Y(St)Y"]=NORM::VDE_0815;
	m_cable_type_map["JE-Y(ST)Y"]=NORM::VDE_0815;
	m_cable_type_map["NAYY"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["NYCY"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["NYCWY"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["NAYCWY"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["NHXMH"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["NYM"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["(N)YM-(St)"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["NYY"]=NORM::VDE_0293_308_S2;
	m_cable_type_map["LanCat5"]=NORM::keine;
	m_cable_type_map["LanCat6"]=NORM::keine;
	m_cable_type_map["LanCat7"]=NORM::keine;
	m_cable_type_map["LiYY"]=NORM::keine;
	m_cable_type_map["LiYCY"]=NORM::keine;
	m_cable_type_map["LiYCY_paarig"]=NORM::keine;
	m_cable_type_map["Oelflex_Classic_100_CY"]=NORM::Zahlen;
	m_cable_type_map["Oelflex_Classic_100_H"]=NORM::Zahlen;
	m_cable_type_map["Oelflex_Classic_110_CH"]=NORM::Zahlen;
	m_cable_type_map["Oelflex_Classic_110_CY"]=NORM::Zahlen;
	m_cable_type_map["Oelflex_Classic_110_LT"]=NORM::Zahlen;
	m_cable_type_map["Oelflex_Classic_110_SY"]=NORM::Zahlen;
	m_cable_type_map["Oelflex_Classic_115_CY"]=NORM::Zahlen;
	m_cable_type_map["YSLY-JB"]=NORM::VDE_0293_color;


		// Content for the 'Standards' ComboBox
		// 1.String in the list:   Name of the standard
		// 2.String in the list:   ToolTip
		// Every entry in the enum must also have a corresponding entry here,
		// in the same order as in the enum.
	m_core_identification_list = {{"",""},
								  {"DIN47100",
								   "Electronic control cables \n "
								   "Elektronik Steuerleitungen \n "
								   "LIYCY, LIYY (ws,bn,gn,gegr,rs,bl,rt,sw,vio)"},
								  {"VDE0293_308_S2",
								   "Multicore cables, actual standard \n"
								   "Mehradrige Kabel und Leitungen, aktuelle Norm \n"
								   "NYM, NYY, NAYY, H07RN-F (bn, sw, gr, bl, gnge)"},
								  {"VDE0293_308_S1_massive",
								   "Massive multicore cables old standard \n"
								   "Massive mehradrige Kabel alte Norm \n"
								   "NYM, NYY, NAYY, (sw, bn, sw, bl, gnge)"},
								  {"VDE0293_308_S1_flex",
								   "Flexible multicore cables old standard \n"
								   "Flexible mehradrige Leitungen alte Norm \n"
								   "H07RN-F (sw, bn, sw, bl, gnge)"},
								  {"VDE0293_color",
								   "PVC control cables with more than 6 colored wires\n"
								   "PVC Steuerleitung mit mehr wie 6 farbigen Adern \n"
								   "YSLY-JB, (gnge, ws, sw, bl, bn, gr, rt, ...)"},
								  {"VDE0815_2x2",
								   ""},
								  {"VDE0815_from_4x2",
								   ""},
								  {"VDE0816_star_quad_ab",
								   ""},
								  {"VDE0816_star_quad_color_ab",
								   ""},
								  {"VDE0816_star_quad_ring",
								   ""},
								  {"Numbers",
								   ""}};


		// Content of ComboBox 'cross section' for unit 'mm'
	m_mm_list.clear();
	m_mm_list+={"","0,6","0,8"};

		// Content of ComboBox 'cross section' for unit 'mm²'
	m_qmm_list.clear();
	m_qmm_list+={"","0,14","0,25","0,5","0,75","1","1,5","2,5","4","6","10","16","25","35",
		"50","70","95","120","150","185","240","300","400","500","630","800","1000",};

		// Content of ComboBox 'cross section' for unit 'AWG'
	m_awg_list.clear();
	m_awg_list+={"","32","30","28","26","24","22","20","18","16","14","12","10","8",
				   "6","4","2","1","0","00","000","0000"};


		// cable typ
	ui->m_cable_type_cb->clear();
								  // Die key's aus der map 'm_cable_type_map' bilden die Liste für die ComboBox
	ui->m_cable_type_cb->addItems(m_cable_type_map.keys());
	ui->m_cable_type_cb->setCurrentIndex(0); // Eintag NYM aktivieren
	ui->m_pe_chb->setChecked(true); // CheckBox PE aktivieren

		// wire count
	ui->m_wire_count_le->clear();
	ui->m_wire_count_le->setText(QString::number(m_wire_count));
	ui->m_schirm_chb->setChecked(false);	// CheckBox Schirm deaktivieren

		//Unit
	ui->m_qmm_rb->setChecked(true);


		//Cross section
	ui->m_cross_section_cb->clear();
	if (ui->m_mm_rb->isChecked())
		ui->m_cross_section_cb->insertItems(0,m_mm_list);
	else if (ui->m_qmm_rb->isChecked())
		ui->m_cross_section_cb->insertItems(0,m_qmm_list);
	else if(ui->m_awg_rb->isChecked())
		ui->m_cross_section_cb->insertItems(0,m_awg_list);
	ui->m_cross_section_cb->setCurrentIndex(0);

		// protection earth
	ui->m_core_identification_cb->clear();
	if(ui->m_pe_chb->isChecked()){
		for(int i=0;i<m_core_identification_list.count();i++){
			QString str1 = m_core_identification_list.value(i).at(0);
			ui->m_core_identification_cb->addItem(m_core_identification_list.value(i).at(0));
			QString str2 = m_core_identification_list.value(i).at(1);
			ui->m_core_identification_cb->setItemData(i, str2, Qt::ToolTipRole);

		}
	}
	else{
		for(int i=0;i<m_core_identification_list_O.count();i++){
			ui->m_core_identification_cb->addItem(m_core_identification_list_O.value(i).at(0));
			ui->m_core_identification_cb->setToolTip(m_core_identification_list_O.value(i).at(1));
		}
	}
		// Plant
	ui->m_plant_le->setText("=");
	ui->m_plant_le->end(false);

		// Location
	ui->m_location_le->setText("+");
	ui->m_location_le->end(false);

		// Label
	ui->m_label_le->setText("-W1");
	ui->m_label_le->end(false);

		// Function

		// Spannung

		// Since wire_count and the RadioButtons are already
		// pre-populated, the RadioButtons and the wire_count ComboBox
		// might not be interacted with at all, and thus would not be
		// written to the data list. Therefore, these values ​​are written
		// into the data list here at this stage.
	updateCableData();
	updateTreeWidget();

		// activate connections
	activeConnections(true);
}

void CablePropertiesWidget::on_m_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
	if (item->childCount()>0)
		return;

	Qt::ItemFlags flags = item->flags();
	if(column == 0)
	{
		item->setFlags(flags & (~Qt::ItemIsEditable));
	}
	else
	{
		item->setFlags(flags | Qt::ItemIsEditable);
	}
}


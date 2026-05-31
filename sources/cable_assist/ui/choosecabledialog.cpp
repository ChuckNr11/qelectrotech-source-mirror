#include "choosecabledialog.h"

#include "qtextstream.h"
#include "qfileinfo.h"
#include "ui_choosecabledialog.h"


ChooseCableDialog::ChooseCableDialog(int wireCount, QWidget* parent) :
	QDialog(parent), ui(new Ui::ChooseCableDialog)
{
	ui->setupUi(this);
	m_wire_count = wireCount;
	init();
}

ChooseCableDialog::~ChooseCableDialog() { delete ui; }

void ChooseCableDialog::init()
{
	m_table=ui->m_cable_database_tw;;
	parseFile();
		// Einstellungen für das TableWidget
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	//m_table->visib
	m_table->resizeColumnsToContents();
	ui->buttonBox->button(QDialogButtonBox::Ok)->setDisabled(true);   //  setDefault(false);
	ui->m_filter_cb->setCurrentIndex(1);
	ui->m_edit_le->setText(QString::number(m_wire_count));

	//filterTabelle(m_table,QString::number(m_wire_count), 1);
	//QStringList list;
	//disconnect(m_table, &QTableWidget::textChanged,);
}

void ChooseCableDialog::parseFile()
{
	QStringList cable;
	QFile f1("/home/achim/QET_Development/entwickler_version/qelectrotech-cable/Dateien/Kabel.txt");
	f1.open(QIODevice::ReadOnly);
	QTextStream s1(&f1);

	int i{0};
			   // bis zum eof
	while (!s1.atEnd()){
		QString s;
			// die ersten 30 Zeilen der Datei beinhalten keine Kabel
		if(i<29){
			s1.readLine();
			i++;
			continue;
		}
			// reads line from file
		s=s1.readLine();

		cable.append(s.split(";")); // appends column to list, ',' is separator
		m_table->setRowCount(m_table->rowCount()+1);
		for(int k=0;k<cable.count();k++){
			QTableWidgetItem *twi = new QTableWidgetItem(cable[k]);
			twi->setFlags(Qt::ItemIsSelectable);
			Qt::ItemFlags flag =twi->flags();
			m_table->setItem(i-29,k,twi);
		}
		cable.clear();
			//m_cable_list_from_csv.append(cable);
		i++;
		m_table->selectRow(0);
	}
	f1.close();
}

void ChooseCableDialog::filterTabelle(QTableWidget* table, const QString& searchtext, int col)
{
		// Suchtext normieren
	QString filter = searchtext.toLower();

		// Wenn es nichts zu filtern gibt, alle Zeilen wieder sichtbar schalten
	if(filter.isEmpty()){
		for(int row=0; row < table->rowCount(); ++row){
			table->setRowHidden(row, false);
		}
		return;
	}
		// Tabelle parsen
	for (int row = 0; row < table->rowCount(); ++row) {
		bool treffer = false;

		QTableWidgetItem* item = table->item(row, col);

			// Item Text normieren und beginnend mit dem ersten Zeichen
			// mit dem Filtertext vergleichen
		if (item && item->text().toLower().startsWith(filter)) {
			treffer = true;
			//table->setRowHidden(row,false);
			//continue;
		}
			// Zeile Ein- oder Ausschalten
		table->setRowHidden(row, !treffer);
	}
}

void ChooseCableDialog::resetFilter()
{
	ui->m_edit_le->setText("");
	for(int row=0;row<m_table->rowCount();row++){
		if(m_table->isRowHidden(row))
			m_table->setRowHidden(row,false);
	}

}

/*
	suche über 2 Spalten
	aufpassen -> der code kommt von phyton
*/

/*
search_text = search_text.lower() # Groß-/Kleinschreibung ignorieren

	for(nt row = 0; row < table->rowCount(); ++row){
		# Spaltenindizes anpassen (hier Spalte 0 und 1)
		item_col1 = table_widget.item(row, 0)
		item_col2 = table_widget.item(row, 1)

		match = False

		# Prüfen, ob der Suchtext in einer der beiden Spalten vorkommt
		if item_col1 and search_text in item_col1.text().lower():
		match = True
		if item_col2 and search_text in item_col2.text().lower():
		match = True

		# Zeile ausblenden, wenn kein Treffer erzielt wurde
		table_widget.setRowHidden(row, not match)
*/

void ChooseCableDialog::on_m_edit_le_textChanged(const QString &text)
{
	if(text.isEmpty()){
		resetFilter();
		return;
	}

	QString str = text;

		//
		// Catching incorrect characters during manual input
		// Ensure correct syntax: a single number or x×n (zB. 2x2, 4x2).
	if(ui->m_filter_cb->currentText()=="Aderzahl" || ui->m_filter_cb->currentText()=="Querschnitt"){
		QList<QChar>validCharacters;
		QList<QChar> char_list;

		if(ui->m_filter_cb->currentText()=="Aderzahl"){
			validCharacters={'0','1','2','3','4','5','6','7','8','9','x','G'};
			char_list = {'x','G'};
		}
		else{
			validCharacters={'0','1','2','3','4','5','6','7','8','9',',','.'};
			char_list = {',','.'};
		}

		for (qsizetype i = 0; i < str.size(); ++i) {
				// Delete invalid characters
			if (!validCharacters.contains(str.at(i))){
				str.remove(i,1);
				ui->m_edit_le->setText(str); // ??? Signal currentTextChanged unterbinden
				continue;
			}
		}

			// ignore a starting 'x', 'G', '.' oder ','
		if(char_list.contains(str.at(0))){
			ui->m_edit_le->setText("");
		}

			// delete a second 'x', 'G', '.' oder ','
		for(int i = 0;i<char_list.count();i++){
			if (str.count(char_list[i])>1){
				str.remove(ui->m_edit_le->cursorPosition()-1,1);
				ui->m_edit_le->setText(str);
			}
		}

	}
	int index =0;

	if(ui->m_filter_cb->currentIndex()==1)
		index=1;
	else if(ui->m_filter_cb->currentIndex()==2)
		index=3;
	else if(ui->m_filter_cb->currentIndex()==3)
		index=4;
	filterTabelle(m_table,str,index);

}


void ChooseCableDialog::on_m_filter_cb_currentIndexChanged(int index)
{
	filterTabelle(m_table,ui->m_edit_le->text(),index);
}


void ChooseCableDialog::on_m_new_cable_pb_clicked()
{
	/*
	   Das war der Versuch ein neues Kabel im Edito zu erstellen
	*/
	QString filter_text = ui->m_edit_le->text();

	m_table->setRowCount(0);
	//resetFilter();
	m_table->setRowCount(m_table->rowCount()+1);

	int row = m_table->rowCount()-1;
	for(int i=0;i<m_table->columnCount();i++){
		QTableWidgetItem *twi = new QTableWidgetItem("");
		twi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable);
		Qt::ItemFlags flag =twi->flags();
		m_table->setItem(row,i,twi);
	}
	m_table->item(0,1)->setText(filter_text);

	//m_table->setCurrentItem(m_table->item(row,0));
	m_table->item(row,0)->setSelected(true);
	m_table->setFocus();
	m_table->scrollToBottom();
	QTableWidgetItem twi = *m_table->item(row,0);
	Qt::ItemFlags flag =twi.flags();
	Qt::ItemFlags flags =twi.flags();
}


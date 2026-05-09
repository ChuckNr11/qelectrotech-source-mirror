#include "cablepropertiesdialog.h"

#include "ui_cablepropertiesdialog.h"
#include "qpushbutton.h"

CablePropertiesDialog::CablePropertiesDialog(QStringList *list, int wireCount, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::CablePropertiesDialog),
	m_list{list}
{
	ui->setupUi(this);
	m_cpw = new CablePropertiesWidget(wireCount);
	ui -> m_main_layout -> insertWidget(0, m_cpw);
	m_ok_button = ui->buttonBox->button(QDialogButtonBox::Ok);
	m_ok_button->setDisabled(true);
	connect(m_cpw, &CablePropertiesWidget::disableOkButton, this, &CablePropertiesDialog::enableOkButton);
}


CablePropertiesDialog::~CablePropertiesDialog()
{
	delete ui;
}

QStringList CablePropertiesDialog::cableDialog(QStringList *list, QWidget *parent, int wireCount)
{
	CablePropertiesDialog cpd (list, wireCount, parent);
	if (cpd.exec() == QDialog::Rejected){
		//cpd.cableData().clear();
		cpd.m_list_ = cpd.m_cpw->cableData();
		cpd.m_list_.clear();
		return (cpd.m_list_);
	}
	return cpd.cableData();

}


QStringList CablePropertiesDialog::cableData()
{
	m_list_ = m_cpw->cableData();
	return m_list_;
}

void CablePropertiesDialog::enableOkButton(bool disable)
{
	bool set = disable;
	m_ok_button->setDisabled(set);
}

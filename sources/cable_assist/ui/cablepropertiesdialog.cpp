#include "cablepropertiesdialog.h"

#include "sources/qetdiagrameditor.h"
#include "ui_cablepropertiesdialog.h"
#include "qpushbutton.h"

CablePropertiesDialog::CablePropertiesDialog(QStringList *list, int wireCount, Diagram *diagram, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::CablePropertiesDialog),
	m_list{list}
{
	ui->setupUi(this);
	m_cpw = new CablePropertiesWidget(wireCount, diagram, parent);
	ui -> m_main_layout -> insertWidget(0, m_cpw);
	m_ok_button = ui->buttonBox->button(QDialogButtonBox::Ok);
	m_ok_button->setDisabled(true);
	connect(m_cpw, &CablePropertiesWidget::disableOkButton, this, &CablePropertiesDialog::enableOkButton);
}


CablePropertiesDialog::~CablePropertiesDialog()
{
	delete ui;
}

QStringList CablePropertiesDialog::cableDialog(QStringList *list, int wireCount,Diagram *diagram, QWidget *parent)
{
	CablePropertiesDialog cpd (list, wireCount, diagram, parent);
	if (cpd.exec() == QDialog::Rejected){
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
	//bool set = disable;
	m_ok_button->setDisabled(disable);
}

QETDiagramEditor* CablePropertiesDialog::diagramEditor(Diagram *diagram) const
{
	if (!diagram)                     return nullptr;
	if (diagram -> views().isEmpty()) return nullptr;

	QWidget *w = const_cast<QGraphicsView *>(diagram -> views().at(0));
	while (w -> parentWidget() && !w -> isWindow()) {
		w = w -> parentWidget();
	}
	return(qobject_cast<QETDiagramEditor *>(w));
}

#include "compositetexteditdialog.h"

#include "../qetapp.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"
#include "ui_compositetexteditdialog.h"

#include <utility>

CompositeTextEditDialog::CompositeTextEditDialog(DynamicElementTextItem *text, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::CompositeTextEditDialog),
	m_text(text)
{
	ui->setupUi(this);
	m_default_text = m_text->compositeText();
	ui->m_plain_text_edit->setPlainText(m_default_text);
	ui->m_plain_text_edit->setPlaceholderText(tr("Entrée votre texte composé ici, en vous aidant des variables disponible"));
	setUpComboBox();
}

CompositeTextEditDialog::CompositeTextEditDialog(QString text, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::CompositeTextEditDialog)
{
	ui->setupUi(this);
	m_default_text = std::move(text);
	ui->m_plain_text_edit->setPlainText(m_default_text);
	ui->m_plain_text_edit->setPlaceholderText(tr("Entrée votre texte composé ici, en vous aidant des variables disponible"));
	setUpComboBox();
}

CompositeTextEditDialog::~CompositeTextEditDialog()
{
	delete ui;
}

/**
	@brief CompositeTextEditDialog::plainText
	@return The edited text
*/
QString CompositeTextEditDialog::plainText() const
{
	return ui->m_plain_text_edit->toPlainText();
}

/**
	@brief CompositeTextEditDialog::setUpComboBox
	Add the available element information in the combo box
*/
void CompositeTextEditDialog::setUpComboBox()
{
	QStringList qstrl;
	bool is_report = false;
	if (m_text && m_text->parentElement()->linkType() & Element::AllReport) {
		is_report = true;
	}

	if(is_report) //Special treatment for text owned by a folio report
	{
		qstrl = QETInformation::folioReportInfoKeys();
	}
	else
	{
		qstrl = QETInformation::elementInfoKeys();
		qstrl.removeAll("formula");
	}
	
	for (int i=0; i<qstrl.size();++i) {
		ui -> m_info_cb -> addItem(qstrl[i], QETInformation::translatedInfoKey(qstrl[i]));
	}
}

void CompositeTextEditDialog::on_m_info_cb_activated(const QString &arg1)
{
	Q_UNUSED(arg1)
	ui->m_plain_text_edit->insertPlainText(ui->m_info_cb->currentData().toString());
}

/**
	@brief CompositeTextEditDialog::focusInEvent
	Reimplemented from QWidget::focusInEvent
	@param event
*/
void CompositeTextEditDialog::focusInEvent(QFocusEvent *event)
{
	ui->m_plain_text_edit->setFocus();
	QDialog::focusInEvent(event);
}

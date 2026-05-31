#include "newcabledialog.h"

#include "ui_newcabledialog.h"

NewCableDialog::NewCableDialog(QWidget* parent) :
	QDialog(parent), ui(new Ui::NewCableDialog)
{
	ui->setupUi(this);
}

NewCableDialog::~NewCableDialog() { delete ui; }

#ifndef CABLEPROPERTIESDIALOG_H
#define CABLEPROPERTIESDIALOG_H

#include "cablepropertieswidget.h"
#include <QDialog>

class CablePropertiesWidget;

namespace Ui
{
	class CablePropertiesDialog;
}

class CablePropertiesDialog : public QDialog
{
	friend CablePropertiesWidget;

	Q_OBJECT

  public:
	explicit CablePropertiesDialog(QStringList *list, int wireCount, QWidget* parent = nullptr);
	~CablePropertiesDialog();

	static QStringList cableDialog (
		QStringList *list = nullptr, QWidget *parent = nullptr, int wireCount=0);

	QStringList cableData();

  private:
	Ui::CablePropertiesDialog* ui;
	static CablePropertiesWidget m_cpw_;
	CablePropertiesWidget *m_cpw;
	QStringList *m_list;
	QStringList m_list_;
	QPushButton* m_ok_button;
	void enableOkButton(bool disable);
};

#endif // CABLEPROPERTIESDIALOG_H


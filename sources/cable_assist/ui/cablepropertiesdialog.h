#ifndef CABLEPROPERTIESDIALOG_H
#define CABLEPROPERTIESDIALOG_H

#include "cablepropertieswidget.h"
#include <QDialog>
#include "../../diagram.h"
#include "sources/qetdiagrameditor.h"

class CablePropertiesWidget;
class Diagram;

namespace Ui
{
	class CablePropertiesDialog;
}

class CablePropertiesDialog : public QDialog
{
	friend CablePropertiesWidget;

	Q_OBJECT

  public:
	explicit CablePropertiesDialog(QStringList *list, int wireCount, Diagram *diagram, QWidget* parent);
	~CablePropertiesDialog();

	static QStringList cableDialog (
		//QStringList *list = nullptr, QWidget *parent = nullptr, int wireCount=0);
		QStringList *list = nullptr, int wireCount=0, Diagram *diagram=nullptr, QWidget *parent = nullptr);

	QStringList cableData();

  private:
	//QETDiagramEditor* diagramEditor() const;
	Ui::CablePropertiesDialog* ui;
	static CablePropertiesWidget m_cpw_;
	CablePropertiesWidget *m_cpw;
	QStringList *m_list;
	QStringList m_list_;
	QPushButton* m_ok_button;
	void enableOkButton(bool disable);
	QETDiagramEditor *diagramEditor(Diagram  *diagram) const;
	Diagram *m_diagram;
};

#endif // CABLEPROPERTIESDIALOG_H


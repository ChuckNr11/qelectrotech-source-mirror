#ifndef CHOOSECABLEDIALOG_H
#define CHOOSECABLEDIALOG_H

#include "qtablewidget.h"
#include <QDialog>

namespace Ui
{
	class ChooseCableDialog;
}

class ChooseCableDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit ChooseCableDialog(int wireCount, QWidget* parent = nullptr);
	~ChooseCableDialog();

	void cableType() const;

  private slots:
	void on_m_filter_cb_currentIndexChanged(int index);
	void on_m_edit_le_textChanged(const QString &arg1);
	void on_m_new_cable_pb_clicked();

  private:
	Ui::ChooseCableDialog* ui;

	void init();
	void parseFile();
	void filterTabelle(QTableWidget* , const QString&, int);
	void resetFilter();

	int m_wire_count;
	QTableWidget *m_table;

};

#endif // CHOOSECABLEDIALOG_H

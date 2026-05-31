#ifndef CABLEPROPERTIESWIDGET_H
#define CABLEPROPERTIESWIDGET_H

#include "sources/qetproject.h"
#include <QWidget>
#include <QPair>
#include <QList>
#include <QMap>
#include <QTreeWidgetItem>

namespace Ui
{
	class CablePropertiesWidget;
}
class CablePropertiesDialog;

class CablePropertiesWidget : public QWidget
{
	//friend CablePropertiesDialog;


	Q_OBJECT

  public:
	enum cableData{
		LABEL = 0,		// ELMT_LABEL
		PLANT,			// ELMT_PLANT
		LOCATION,		// ELMT_LOCATION
		ANNOTATON,		// ELMT_COMMENT
		FUNCTION,		// ELMT_FUNCTION
		VOLTAGE,		//
		TYPE,
		COND_COUNT,
		MARKER,
		CROSS_SECTION,
		UNIT,
	};

	explicit CablePropertiesWidget(int wireCount, Diagram *diagram, QWidget* parent = nullptr);
	~CablePropertiesWidget();

	QStringList cableData() const;

  private:
	Ui::CablePropertiesWidget* ui;

	void init();
	void parseProjectForCable();
	void addCableToTreewidget(QList<QPointer<Element>> cable);
	void addNewCable(QStringList cable_data);
	void updateNewCable();
	void setConnections(bool);

  signals:
	void disableOkButton(bool disable);

  private slots:
	void textHasChanged();//const QString &text);
	void on_m_conductor_count_le_textChanged(const QString &text);
/*	void on_m_plant_le_textChanged(const QString &text);
	void on_m_location_le_textChanged(const QString &text);
	void on_m_label_le_textChanged(const QString &text);
	void on_m_function_le_textChanged(const QString &text);
	void on_m_cable_type_cb_editTextChanged(const QString &text);

	void on_m_cross_section_cb_currentIndexChanged(int index);
	void on_m_mm_rb_toggled(bool checked);
	void on_m_qmm_rb_toggled(bool checked);
	void on_m_awg_rb_toggled(bool checked);
	void on_m_core_identification_cb_currentIndexChanged(int index);

	void on_m_pe_chb_stateChanged(int arg1);

	void on_m_schirm_chb_checkStateChanged(const Qt::CheckState &arg1);

	void on_m_pe_chb_checkStateChanged(const Qt::CheckState &arg1);*/

	void on_m_newcable__tb_clicked();

  private:
	Diagram *m_diagram;
	int m_wire_count;

	QStringList
		m_cable_data;

	QList<QList<QPointer<Element>>> m_cable_list;		/// Liste aller Kabel aus dem Projekt
	QTreeWidgetItem *m_new_cable=nullptr;
};

#endif // CABLEPROPERTIESWIDGET_H


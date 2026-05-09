#ifndef CABLEPROPERTIESWIDGET_H
#define CABLEPROPERTIESWIDGET_H

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
	friend CablePropertiesDialog;


	Q_OBJECT

  public:

	enum NORM {
		keine =0,
		DIN_47100,
		VDE_0293_308_S2,
		VDE_0293_308_S1_massive,
		VDE_0293_308_S1_flex,
		VDE_0293_color,
		VDE_0815_2x2,
		VDE_0815,
		VDE_0816_star_quad_ab,
		VDE_0816_star_quad_color_ab,
		VDE_0816_star_quad,
		Zahlen
	};
	Q_ENUM (NORM)


		   // void extracted();
	explicit CablePropertiesWidget(int wireCount, QWidget* parent = nullptr);
	~CablePropertiesWidget();

	QStringList cableData() const;


  private slots:

	void cable_type_editTextChanged(const QString &text);
	void on_m_pe_chb_stateChanged(int arg1);
	void wire_count_textChanged(const QString &text);
	void cross_section_currentIndexChanged(int index);
	void unit_mm_toggled(bool checked);
	void unit_qmm_toggled(bool checked);
	void unit_awg_toggled(bool checked);
	void core_identification_currentIndexChanged(int index);
	void plant_textChanged(const QString &text);
	void location_textChanged(const QString &text);
	void label_textChanged(const QString &text);
	void on_m_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);

  signals:
	void disableOkButton(bool disable);

  private:
	Ui::CablePropertiesWidget* ui;

	using TStandardList = QList<QString>;
	using TVDE0293List = QList<QList<QString>>;
	using TCbWidgetList = QList<QList<QString>>;

	void init();
	void updateCableData();
	void updateTreeWidget();
	void activeConnections(bool active);
	void erstelleAdern(QList<QString> list);

	QTreeWidgetItem *kabel;

	int m_wire_count;
	QStringList
		m_qmm_list,
		m_awg_list,
		m_mm_list;
	QList<QString>
		m_cable_data;

	TStandardList
		m_actual_core_list,
		m_din47100_core_stranding,
		m_din47100_pair_stranding,
		m_zahlen;

	TCbWidgetList
		m_core_identification_list,
		m_core_identification_list_O;

	TVDE0293List
		m_vde0293_308_s2_j,
		m_vde0293_308_s1_massive_j,
		m_vde0293_308_s1_flex_j,
		m_vde0293_308_s2_o,
		m_vde0293_308_s1_massive_o,
		m_vde0293_308_s1_flex_o;


	QMap<QString,NORM> m_cable_type_map;

};

#endif // CABLEPROPERTIESWIDGET_H


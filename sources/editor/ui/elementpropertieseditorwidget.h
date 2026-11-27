/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef ELEMENTPROPERTIESEDITORWIDGET_H
#define ELEMENTPROPERTIESEDITORWIDGET_H

#include <diagramcontext.h>
#include <properties/elementdata.h>

#include <QAbstractButton>
#include <QDialog>
#include <QItemDelegate>
#include <QPointF>
#include <QUuid>

class QTableWidget;
class QTreeWidgetItem;
class QSpinBox;
class QCheckBox;
class QGroupBox;
class QPushButton;
class QLineEdit;
class QScrollBar;
class QComboBox;
class QStackedWidget;
class PartTerminal;

namespace Ui {
	class ElementPropertiesEditorWidget;
}

/**
	@brief The EditorDelegate class
	This delegate is only use for disable the edition of the first
	column of the information tree widget
*/
class EditorDelegate : public QItemDelegate
{
	Q_OBJECT
	public:
		EditorDelegate(QObject *parent) :
			QItemDelegate(parent)
		{}

		QWidget* createEditor(QWidget *parent,
			      const QStyleOptionViewItem &option,
				  const QModelIndex &index) const override;
};

/**
	@brief The ElementPropertiesEditorWidget class
	This class provide a dialog for edit various property of element, like
	the type (master, slave, report etc....) and kind info.
*/
class ElementPropertiesEditorWidget : public QDialog
{
	Q_OBJECT

	//METHODS
	public:
		explicit ElementPropertiesEditorWidget(ElementData data, QWidget *parent = nullptr);
		~ElementPropertiesEditorWidget() override;

		ElementData editedData() {return m_data;}

			/// One connection of the symbol being edited: the uuid saved on
			/// <terminal>, which is what a role is stored against; the name it
			/// is drawn with and the side it sits on, which are only there so
			/// the row reads as something; and the part itself, so a row can
			/// point at the connection in the drawing
		struct Connection
		{
			QUuid uuid;
			QString name;
			QPointF pos;
				/// The side of the symbol it sits on: "n", "e", "s", "w"
			QString side;
			PartTerminal *item = nullptr;
		};
			/// The symbol's connections, so that a junction can ask which of
			/// them the wire comes in at (@see populateJunctionRoles)
		void setConnections(const QList<Connection> &connections);

	private:
		void upDateInterface();
		void setUpInterface();
		void updateTree();
		void populateTree();
		void populateJunctionRoles();
		void setUpCustomInfo();
		QTreeWidgetItem *addCustomInfoRow(const QString &key, const QString &value);
		void addCustomInfo();
		void populateSlaveGroupsTable();
		void readSlaveGroupsFromTable();
		void createPlcConfigWidgets();
		void populatePlcTable();
		void readPlcTable();
		void plcSetupExtraColumns(int row, const ElementData::PlcIO &io);
		void populateTerminalSizeCombos(double section, double width, const QString &catalog_entry);
		void refreshTerminalWidthCombo();
		void populateTerminalColorCombo(const QString &spec);

		//SLOTS
	private slots:
		void on_m_buttonBox_accepted();
		void on_m_base_type_cb_currentIndexChanged(int index);
		void junctionRoleChanged(int row);
		void junctionConnectionChosen(int row);
		void on_m_slave_groups_checkbox_toggled(bool checked);
		void on_max_slaves_checkbox_toggled(bool checked);
		void plcAddRow();
		void plcRemoveRow();
		void plcMoveRowUp();
		void plcMoveRowDown();
		void plcPasteFromClipboard();
		void plcTerminalCountChanged(int row, int count);
		void plcDisplayModeChanged(int index);
		void plcSelectHeaderFont();
		void plcSelectCellFont();
		void plcSetCellFromValue(int row, int col, const QString &val);

		//ATTRIBUTES
	private:
		Ui::ElementPropertiesEditorWidget *ui;
		ElementData m_data;
			/// The symbol's connections, while the dialog is open
		QList<Connection> m_connections;

		// PLC configuration widgets (created programmatically)
		QGroupBox *m_plc_gb = nullptr;
		QTableWidget *m_plc_table = nullptr;
		QTableWidget *m_plc_terminal_table = nullptr;
		QCheckBox *m_plc_break_checkboxes[4] = {nullptr, nullptr, nullptr, nullptr};
		QSpinBox *m_plc_break_spinboxes[4] = {nullptr, nullptr, nullptr, nullptr};
		QSpinBox *m_plc_row_height_spinbox = nullptr;
		QWidget *m_custom_info_w = nullptr;
		QPushButton *m_plc_header_font_btn = nullptr;
		QPushButton *m_plc_cell_font_btn = nullptr;
		QCheckBox   *m_plc_show_headers_cb = nullptr;
		QFont m_plc_header_font;
		QFont m_plc_cell_font;
	QList<QCheckBox *> m_plc_col_visibility_checkboxes;
	QList<QSpinBox *>  m_plc_col_width_spinboxes;
	QList<QLineEdit *> m_plc_col_name_edits;
	QScrollBar *m_plc_shared_scrollbar = nullptr;
	bool m_plc_scroll_sync = false;
		/// Display mode of the IO table and the layouts of the
		/// module box and the rack overview
	QComboBox *m_plc_display_cb = nullptr;
	QStackedWidget *m_plc_display_stack = nullptr;
	QSpinBox *m_plc_box_width_spinbox = nullptr;
	QSpinBox *m_plc_box_header_spinbox = nullptr;
	QCheckBox *m_plc_box_supply_cb = nullptr;
	QCheckBox *m_plc_box_split_cb = nullptr;
	QLineEdit *m_plc_box_title_edit = nullptr;
	QLineEdit *m_plc_box_row_edit = nullptr;
	QLineEdit *m_plc_box_supply_edit = nullptr;
	QSpinBox *m_plc_rack_slot_w_spinbox = nullptr;
	QSpinBox *m_plc_rack_slot_h_spinbox = nullptr;
	QCheckBox *m_plc_rack_range_cb = nullptr;
	QLineEdit *m_plc_rack_title_edit = nullptr;
	QLineEdit *m_plc_rack_plate_edit = nullptr;
	QTableWidget *m_plc_module_labels = nullptr;
};

#endif // ELEMENTPROPERTIESEDITORWIDGET_H

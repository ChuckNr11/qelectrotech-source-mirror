#ifndef NEWCABLEDIALOG_H
#define NEWCABLEDIALOG_H

#include <QDialog>

namespace Ui
{
	class NewCableDialog;
}

class NewCableDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit NewCableDialog(QWidget* parent = nullptr);
	~NewCableDialog();

  private:
	Ui::NewCableDialog* ui;
};

#endif // NEWCABLEDIALOG_H

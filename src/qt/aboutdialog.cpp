#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "clientmodel.h"

#include "version.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    this->setStyleSheet("QWidget                      { background: transparent; }"
                        "QPushButton                  { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); border: none; padding-left: 12px; padding-right: 12px; padding-top: 8px; padding-bottom: 8px; height: 16px; border-radius: 3px; min-width: 64px; }"
                        "QPushButton:enabled:hover    { background: rgba(252, 252, 252, 120); color: rgb(118, 134, 131); }"
                        "QPushButton:enabled:pressed  { background: rgba(252, 252, 252, 40); color: rgb(220, 220, 220); }"
                        "QPushButton:disabled         { background: rgb(0, 0, 0, 20); color: rgb(150, 150, 150); }"
    );
}

void AboutDialog::setModel(ClientModel *model)
{
    if(model)
    {
        ui->versionLabel->setText(model->formatFullVersion());
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_buttonBox_accepted()
{
    close();
}

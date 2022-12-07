#include "appchooserdialog.h"

AppChooserDialog::AppChooserDialog(QWidget *parent)
    : QDialog(parent)
{

}

const QStringList &AppChooserDialog::selectChoices() const
{
    return m_selectedChoices;
}

void AppChooserDialog::updateChoices(const QStringList &choices)
{

}

void AppChooserDialog::setCurrentChoice(const QString &choice)
{

}

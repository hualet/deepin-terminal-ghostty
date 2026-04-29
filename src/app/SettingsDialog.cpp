#include "SettingsDialog.h"

#include "AppSettings.h"

#include <DKeySequenceEdit>
#include <DSettingsOption>
#include <DSettingsWidgetFactory>

static QPair<QWidget *, QWidget *> createShortcutEditHandle(QObject *opt) {
    auto *option = qobject_cast<Dtk::Core::DSettingsOption *>(opt);
    auto *edit = new DKeySequenceEdit();
    edit->setKeySequence(QKeySequence(option->value().toString()));

    QObject::connect(edit, &DKeySequenceEdit::editingFinished, edit, [edit, option](const QKeySequence &seq) {
        QString str = seq.toString();
        if (str == "Esc" || str == "Backspace") {
            edit->setKeySequence(QKeySequence(option->value().toString()));
            return;
        }
        option->setValue(str);
    });

    QObject::connect(option, &Dtk::Core::DSettingsOption::valueChanged, edit,
                     [edit](const QVariant &value) { edit->setKeySequence(QKeySequence(value.toString())); });

    return DSettingsWidgetFactory::createStandardItem(QByteArray(), option, edit);
}

SettingsDialog::SettingsDialog(QWidget *parent) : DSettingsDialog(parent) {
    setWindowTitle(tr("Settings"));
    setAccessibleName(tr("Settings"));
    setAccessibleDescription(tr("Configure terminal appearance, behavior, and shortcuts."));
    widgetFactory()->registerWidget("shortcut", createShortcutEditHandle);
    updateSettings(AppSettings::instance()->dsettings());
}

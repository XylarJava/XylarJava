#include "ElyLoginDialog.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "DesktopServices.h"
#include "ui/widgets/Skin3DPreview.h"

MinecraftAccountPtr ElyAccountDialog::login(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Add Ely.by account"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* intro =
        new QLabel(QObject::tr("Sign in to your Ely.by account. Your password is sent directly to Ely.by over HTTPS and is never saved."));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* form = new QFormLayout;
    auto* user = new QLineEdit(&dialog);
    auto* password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    auto* code = new QLineEdit(&dialog);
    code->setMaxLength(6);
    code->setPlaceholderText(QObject::tr("If enabled on your account"));
    form->addRow(QObject::tr("Email or username"), user);
    form->addRow(QObject::tr("Password"), password);
    form->addRow(QObject::tr("Two-factor code"), code);
    layout->addLayout(form);
    auto* status = new QLabel(&dialog);
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Sign in"));
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    MinecraftAccountPtr result;
    shared_qobject_ptr<AuthFlow> task;
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (task && task->isRunning())
            return;
        if (user->text().trimmed().isEmpty() || password->text().isEmpty()) {
            status->setText(QObject::tr("Enter your email and password."));
            return;
        }
        auto candidate = MinecraftAccount::createBlankEly();
        candidate->accountData()->elyLoginName = user->text().trimmed();
        candidate->accountData()->elyPassword = password->text() + (code->text().isEmpty() ? QString() : ":" + code->text());
        task = candidate->login();
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
        status->setText(QObject::tr("Contacting Ely.by…"));
        QObject::connect(task.get(), &Task::succeeded, &dialog, [&, candidate] {
            result = candidate;
            dialog.accept();
        });
        QObject::connect(task.get(), &Task::failed, &dialog, [&, candidate](const QString& reason) {
            Q_UNUSED(candidate);
            status->setText(reason);
            buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
        });
        task->start();
    });
    const auto accepted = dialog.exec() == QDialog::Accepted;
    if (task && task->isRunning())
        task->abort();
    password->clear();
    code->clear();
    return accepted ? result : nullptr;
}

void ElyAccountDialog::showSkin(QWidget* parent, const MinecraftAccountPtr& account)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Ely.by skin — %1").arg(account->profileName()));
    auto* layout = new QVBoxLayout(&dialog);
    auto* preview = new Skin3DPreview(&dialog);
    layout->addWidget(preview);
    auto update = [&] {
        const auto& skin = account->accountData()->minecraftProfile.skin;
        preview->setSkin(QImage::fromData(skin.data), skin.variant == "slim");
    };
    update();
    auto* text = new QLabel(QObject::tr(
        "This account uses Ely.by's skin system. Manage your skin on Ely.by, then refresh the account. Ely.by does not provide capes."));
    text->setWordWrap(true);
    layout->addWidget(text);
    auto* website = new QPushButton(QObject::tr("Manage skin on Ely.by"));
    layout->addWidget(website);
    QObject::connect(website, &QPushButton::clicked, &dialog, [] { DesktopServices::openUrl(QUrl("https://ely.by/skins")); });
    auto* refresh = new QPushButton(QObject::tr("Refresh"));
    layout->addWidget(refresh);
    shared_qobject_ptr<AuthFlow> task;
    QObject::connect(refresh, &QPushButton::clicked, &dialog, [&] {
        refresh->setEnabled(false);
        task = account->refresh();
        QObject::connect(task.get(), &Task::succeeded, &dialog, [&] {
            update();
            refresh->setEnabled(true);
        });
        QObject::connect(task.get(), &Task::failed, &dialog, [&](const QString& reason) {
            text->setText(reason);
            refresh->setEnabled(true);
        });
        if (!task->isRunning())
            task->start();
    });
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(close);
    QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
    if (task && task->isRunning())
        task->abort();
}

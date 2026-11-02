#include "MSALoginDialog.h"
#include "Application.h"

#include "ui_MSALoginDialog.h"

#include "DesktopServices.h"
#include "minecraft/auth/AuthFlow.h"

#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QUrl>
#include <QUrlQuery>
#include <QtWidgets/QPushButton>

#include <qrencode.h>

namespace {

/// A QR code for the sign-in link, so a phone can be used to approve it.
QPixmap qrCodeFor(const QString& text, int size)
{
    QRcode* code = QRcode_encodeString(text.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!code)
        return {};
    const int modules = code->width;
    const int quiet = 2;
    const int total = modules + 2 * quiet;
    QImage image(total, total, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int y = 0; y < modules; y++) {
        for (int x = 0; x < modules; x++) {
            if (code->data[y * modules + x] & 1)
                image.setPixel(x + quiet, y + quiet, qRgb(0, 0, 0));
        }
    }
    QRcode_free(code);
    return QPixmap::fromImage(image.scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
}

}  // namespace

MSALoginDialog::MSALoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::MSALoginDialog)
{
    ui->setupUi(this);

    // Sign-in works like XylarJava for Xbox: the Windows token broker when there is one, a device code
    // (microsoft.com/link) everywhere else. No browser redirect back to the launcher is needed.
    ui->stackedWidget->setCurrentIndex(0);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->loadingLabel->setText(tr("Sign in with Microsoft"));
    ui->status->setText(tr("Contacting Microsoft..."));

    ui->minecraftOwnershipWarningQr->setStyleSheet(QString());
    ui->minecraftOwnershipWarningQr->setText(tr("Sign in with the Microsoft account that owns Minecraft: Java Edition."));
    ui->openLinkButton->setText(tr("Open microsoft.com/link"));
    ui->copyCode->setToolTip(tr("Copy the code"));

    connect(ui->copyCode, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(ui->code->text());
        ui->copyCode->setToolTip(tr("Copied"));
    });
    connect(ui->openLinkButton, &QPushButton::clicked, this, [this] {
        if (m_linkUrl.isValid())
            DesktopServices::openUrl(m_linkUrl);
    });
}

int MSALoginDialog::exec()
{
    if (APPLICATION->getMSAClientID().isEmpty()) {
        QMessageBox::warning(this, tr("Microsoft login not configured"),
                             tr("No Azure client ID was found. Create auth-settings.ini next to the launcher executable."));
        return QDialog::Rejected;
    }

    m_account = MinecraftAccount::createBlankMSA();
    m_login_task = m_account->login(false);
    connect(m_login_task.get(), &Task::failed, this, &MSALoginDialog::onTaskFailed);
    connect(m_login_task.get(), &Task::succeeded, this, &QDialog::accept);
    connect(m_login_task.get(), &Task::aborted, this, &MSALoginDialog::reject);
    connect(m_login_task.get(), &Task::status, this, &MSALoginDialog::onFlowStatus);
    connect(m_login_task.get(), &AuthFlow::authorizeWithBrowserWithExtra, this, &MSALoginDialog::showDeviceCode);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, m_login_task.get(), &Task::abort);
    QMetaObject::invokeMethod(m_login_task.get(), &Task::start, Qt::QueuedConnection);

    return QDialog::exec();
}

MSALoginDialog::~MSALoginDialog()
{
    delete ui;
}

void MSALoginDialog::onTaskFailed(QString reason)
{
    m_login_task->disconnect();
    m_waitingForCode = false;
    ui->stackedWidget->setCurrentIndex(0);
    auto lines = reason.split('\n');
    QString processed;
    for (auto line : lines) {
        if (line.size()) {
            processed += "<font color='red'>" + line.toHtmlEscaped() + "</font><br />";
        } else {
            processed += "<br />";
        }
    }
    ui->status->setText(processed);
    if (m_login_task) {
        ui->loadingLabel->setText(m_login_task->getStatus());
    }
    disconnect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, m_login_task.get(), &Task::abort);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &MSALoginDialog::reject);
}

void MSALoginDialog::showDeviceCode(QString url, QString code, int expiresIn)
{
    Q_UNUSED(expiresIn)
    m_waitingForCode = true;

    // microsoft.com/link fills the code in by itself with ?otc=
    QUrl link(url);
    QUrlQuery query(link);
    query.removeAllQueryItems(QStringLiteral("otc"));
    query.addQueryItem(QStringLiteral("otc"), code);
    link.setQuery(query);
    const bool firstCode = !m_linkUrl.isValid();
    m_linkUrl = link;

    ui->code->setText(code);
    ui->qr->setPixmap(qrCodeFor(link.toString(QUrl::FullyEncoded), ui->qr->maximumWidth() * devicePixelRatio()));
    ui->qrMessage->setText(tr("Go to <a href=\"%1\">%2</a> and enter this code, or scan the QR code with your phone. "
                              "This window continues by itself once you've approved the sign-in.")
                               .arg(link.toString(QUrl::FullyEncoded).toHtmlEscaped(), QUrl(url).host() + QUrl(url).path()));
    ui->stackedWidget->setCurrentIndex(1);

    // Open the page once; a refreshed code (after it expired) just updates the dialog.
    if (firstCode)
        DesktopServices::openUrl(link);
}

void MSALoginDialog::onFlowStatus(QString status)
{
    // A new step only starts once the code was approved, so the code page can go.
    m_waitingForCode = false;
    ui->stackedWidget->setCurrentIndex(0);
    ui->status->setText(status);
}

MinecraftAccountPtr MSALoginDialog::newAccount(QWidget* parent)
{
    MSALoginDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}

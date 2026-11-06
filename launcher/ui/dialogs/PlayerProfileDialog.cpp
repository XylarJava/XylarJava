#include "PlayerProfileDialog.h"

#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "social/ProfileClient.h"
#include "ui/widgets/SkinPreview.h"

namespace {
QString playtimeText(qint64 seconds)
{
    if (seconds < 60)
        return PlayerProfileDialog::tr("Under a minute");
    const qint64 minutes = seconds / 60;
    if (minutes < 60)
        return PlayerProfileDialog::tr("%1 min").arg(minutes);
    const qint64 hours = minutes / 60;
    if (hours >= 100)
        return PlayerProfileDialog::tr("%1 h").arg(QLocale().toString(hours));
    return PlayerProfileDialog::tr("%1 h %2 min").arg(hours).arg(minutes % 60);
}

QString agoText(qint64 at)
{
    if (at <= 0)
        return PlayerProfileDialog::tr("Never");
    const qint64 seconds = qMax<qint64>(0, QDateTime::currentSecsSinceEpoch() - at);
    if (seconds < 120)
        return PlayerProfileDialog::tr("Just now");
    if (seconds < 3600)
        return PlayerProfileDialog::tr("%1 minutes ago").arg(seconds / 60);
    if (seconds < 7200)
        return PlayerProfileDialog::tr("1 hour ago");
    if (seconds < 86400)
        return PlayerProfileDialog::tr("%1 hours ago").arg(seconds / 3600);
    if (seconds < 86400 * 2)
        return PlayerProfileDialog::tr("Yesterday");
    if (seconds < 86400 * 30)
        return PlayerProfileDialog::tr("%1 days ago").arg(seconds / 86400);
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(at).date(), QLocale::LongFormat);
}

QLabel* caption(const QString& text, QWidget* parent)
{
    auto label = new QLabel(text, parent);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
    label->setPalette(palette);
    return label;
}
}  // namespace

PlayerProfileDialog::PlayerProfileDialog(ProfileClient* client, const QString& name, const QImage& skin, QWidget* parent)
    : QDialog(parent), m_client(client), m_name(name)
{
    setWindowTitle(tr("%1's profile").arg(name));
    setAttribute(Qt::WA_DeleteOnClose);

    setMinimumSize(560, 380);
    auto outer = new QVBoxLayout(this);
    auto top = new QHBoxLayout;
    top->setSpacing(24);
    outer->addLayout(top, 1);

    m_skin = new SkinPreview(this);
    m_skin->setFixedSize(150, 260);
    top->addWidget(m_skin, 0, Qt::AlignTop);
    setSkin(skin);

    auto right = new QVBoxLayout;
    right->setSpacing(4);
    top->addLayout(right, 1);

    m_title = new QLabel(name, this);
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    m_title->setTextFormat(Qt::PlainText);
    m_title->setFont(titleFont);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    right->addWidget(m_title);
    auto copyName = new QPushButton(tr("Copy player name"), this);
    connect(copyName, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_title->text()); });
    right->addWidget(copyName, 0, Qt::AlignLeft);
    m_since = caption(QString(), this);
    right->addWidget(m_since);
    right->addSpacing(12);

    m_message = new QLabel(tr("Loading the profile..."), this);
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    right->addWidget(m_message);
    m_retry = new QPushButton(tr("Retry"), this);
    m_retry->hide();
    connect(m_retry, &QPushButton::clicked, this, &PlayerProfileDialog::load);
    right->addWidget(m_retry, 0, Qt::AlignLeft);

    m_details = new QWidget(this);
    auto details = new QVBoxLayout(m_details);
    details->setContentsMargins(0, 0, 0, 0);
    m_stats = new QGridLayout;
    m_stats->setHorizontalSpacing(18);
    m_stats->setVerticalSpacing(8);
    details->addLayout(m_stats);
    details->addSpacing(14);
    auto versionsTitle = new QLabel(tr("Most played versions"), m_details);
    QFont bold = versionsTitle->font();
    bold.setBold(true);
    versionsTitle->setFont(bold);
    details->addWidget(versionsTitle);
    m_versions = new QVBoxLayout;
    m_versions->setSpacing(6);
    details->addLayout(m_versions);
    m_details->hide();
    auto scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(m_details);
    right->addWidget(scroll, 1);
    right->addStretch(1);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    resize(560, 360);
    load();
}

void PlayerProfileDialog::setSkin(const QImage& skin)
{
    m_skin->setSkin(skin, !skin.isNull() && SkinPreview::looksSlim(skin));
}

void PlayerProfileDialog::showMessage(const QString& text)
{
    m_details->hide();
    m_message->setText(text);
    m_message->show();
    m_retry->show();
}

void PlayerProfileDialog::load()
{
    m_retry->hide();
    m_message->setText(tr("Loading the profile..."));
    m_message->show();
    m_client->fetch(m_name, this, [this](bool ok, const ProfileClient::Profile& profile, const QString& error) {
        if (!ok)
            return showMessage(error);
        m_title->setText(profile.name);
        setWindowTitle(tr("%1's profile").arg(profile.name));
        if (profile.memberSince > 0)
            m_since->setText(tr("On XylarJava since %1")
                                 .arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(profile.memberSince).date(), QLocale::LongFormat)));

        const QString none = tr("Not yet");
        const QList<QPair<QString, QString>> rows = {
            { tr("Time played"), playtimeText(profile.playtime) },
            { tr("Last played"), agoText(profile.lastPlayedAt) },
            { tr("Last version"), profile.lastVersion.isEmpty() ? none : tr("Minecraft %1").arg(profile.lastVersion) },
            { tr("Favourite version"), profile.favoriteVersion.isEmpty() ? none : tr("Minecraft %1").arg(profile.favoriteVersion) },
            { tr("Favourite server"), profile.favoriteServer.isEmpty() ? none : profile.favoriteServer },
        };
        for (int i = 0; i < rows.size(); i++) {
            m_stats->addWidget(caption(rows[i].first, m_details), i, 0);
            auto value = new QLabel(rows[i].second, m_details);
            value->setTextFormat(Qt::PlainText);
            value->setWordWrap(true);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_stats->addWidget(value, i, 1);
        }
        m_stats->setColumnStretch(1, 1);

        qint64 most = 0;
        for (const auto& version : profile.versions)
            most = qMax(most, version.second);
        for (const auto& [version, seconds] : profile.versions) {
            auto row = new QHBoxLayout;
            auto label = new QLabel(version, m_details);
            label->setTextFormat(Qt::PlainText);
            label->setMinimumWidth(90);
            row->addWidget(label);
            auto bar = new QProgressBar(m_details);
            bar->setTextVisible(false);
            bar->setFixedHeight(8);
            bar->setRange(0, 1000);
            bar->setValue(most > 0 ? qBound(0, int(1000.0 * qMax<qint64>(0, seconds) / most), 1000) : 0);
            row->addWidget(bar, 1);
            row->addWidget(caption(playtimeText(seconds), m_details));
            m_versions->addLayout(row);
        }
        if (profile.versions.isEmpty())
            m_versions->addWidget(caption(tr("Nothing played on XylarJava yet."), m_details));

        m_message->hide();
        m_details->show();
    });
}

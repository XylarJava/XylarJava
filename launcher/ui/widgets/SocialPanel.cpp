#include "SocialPanel.h"

#include <QApplication>
#include <QDateTime>
#include <QEnterEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QClipboard>

#include "Application.h"
#include "HomePage.h"
#include "minecraft/skins/DefaultSkin.h"
#include "minecraft/skins/MojangSkinFetcher.h"
#include "minecraft/skins/XylarSkinClient.h"
#include "settings/SettingsObject.h"
#include "social/SocialClient.h"

namespace {
constexpr int kPanelWidth = 304;
constexpr int kAvatar = 36;
// Friends change skins: faces are fetched again after this long, while the old one stays on screen.
constexpr qint64 kSkinMaxAgeMs = 2 * 60 * 1000;

QString rgba(const QColor& c, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

bool isOnline(const QString& state)
{
    return state == QLatin1String("launcher") || state == QLatin1String("playing");
}

QColor stateColor(const QString& state)
{
    if (isOnline(state))
        return QColor(0x3f, 0xb9, 0x50);
    if (state == QLatin1String("away"))
        return QColor(0xd2, 0x99, 0x22);
    return QColor(0x80, 0x80, 0x80);
}

QString lastSeen(qint64 seenAt)
{
    if (seenAt <= 0)
        return SocialPanel::tr("Offline");
    const qint64 ago = QDateTime::currentSecsSinceEpoch() - seenAt;
    if (ago < 60 * 60)
        return SocialPanel::tr("Last seen %n min ago", nullptr, int(qMax<qint64>(1, ago / 60)));
    if (ago < 24 * 60 * 60)
        return SocialPanel::tr("Last seen %n h ago", nullptr, int(ago / 3600));
    if (ago < 30 * 24 * 60 * 60)
        return SocialPanel::tr("Last seen %n days ago", nullptr, int(ago / 86400));
    return SocialPanel::tr("Offline");
}

QString statusLine(const QString& state, const QString& detail, qint64 seenAt)
{
    if (state == QLatin1String("playing"))
        return detail.isEmpty() ? SocialPanel::tr("Playing Minecraft") : SocialPanel::tr("Playing %1").arg(detail);
    if (state == QLatin1String("launcher"))
        return SocialPanel::tr("In the launcher");
    if (state == QLatin1String("away"))
        return SocialPanel::tr("Away");
    return lastSeen(seenAt);
}

/// Old 64x32 skins often fill the hat layer with an opaque colour; the game ignores it then, and so do we.
bool hatIsDrawn(const QImage& skin, const QRect& hat)
{
    if (skin.height() * 2 != skin.width())
        return true;
    for (int y = hat.top(); y <= hat.bottom(); y++)
        for (int x = hat.left(); x <= hat.right(); x++)
            if (qAlpha(skin.pixel(x, y)) < 128)
                return true;
    return false;
}

void clearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        else if (QLayout* child = item->layout())
            clearLayout(child);
        delete item;
    }
}
}  // namespace

SocialPanel::SocialPanel(SocialClient* client, QNetworkAccessManager* network, QWidget* parent)
    : QFrame(parent), m_client(client), m_network(network)
{
    setObjectName(QStringLiteral("socialPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(0);
    hide();

    // The content keeps its width while the panel slides, so nothing reflows mid-animation.
    m_body = new QWidget(this);
    m_body->setObjectName(QStringLiteral("socialBody"));
    m_body->setFixedWidth(kPanelWidth);
    auto outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_body, 0, Qt::AlignLeft);

    auto column = new QVBoxLayout(m_body);
    column->setContentsMargins(14, 18, 14, 12);
    column->setSpacing(10);

    auto header = new QHBoxLayout;
    auto title = new QLabel(tr("Social"), m_body);
    title->setObjectName(QStringLiteral("socialTitle"));
    auto close = new QToolButton(m_body);
    close->setObjectName(QStringLiteral("socialClose"));
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Close"));
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QToolButton::clicked, this, &SocialPanel::closeRequested);
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(close);
    column->addLayout(header);

    auto addRow = new QHBoxLayout;
    addRow->setSpacing(6);
    m_addField = new QLineEdit(m_body);
    m_addField->setObjectName(QStringLiteral("socialAddField"));
    m_addField->setPlaceholderText(tr("Add a friend by name"));
    m_addField->setMaxLength(16);
    m_addField->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9_]{0,16}")), m_addField));
    m_addField->setClearButtonEnabled(true);
    m_addButton = new QPushButton(tr("Add"), m_body);
    m_addButton->setObjectName(QStringLiteral("socialAddButton"));
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setEnabled(false);
    connect(m_addField, &QLineEdit::textChanged, this, [this](const QString& text) { m_addButton->setEnabled(text.length() >= 3); });
    connect(m_addField, &QLineEdit::returnPressed, this, &SocialPanel::addFriend);
    connect(m_addButton, &QPushButton::clicked, this, &SocialPanel::addFriend);
    addRow->addWidget(m_addField, 1);
    addRow->addWidget(m_addButton);
    column->addLayout(addRow);

    m_note = new QLabel(m_body);
    m_note->setObjectName(QStringLiteral("socialNote"));
    m_note->setWordWrap(true);
    m_note->hide();
    column->addWidget(m_note);

    auto scroll = new QScrollArea(m_body);
    scroll->setObjectName(QStringLiteral("socialScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto listHost = new QWidget(scroll);
    listHost->setObjectName(QStringLiteral("socialList"));
    m_list = new QVBoxLayout(listHost);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(2);
    scroll->setWidget(listHost);
    column->addWidget(scroll, 1);

    m_slide = new QPropertyAnimation(this, "maximumWidth", this);
    m_slide->setDuration(180);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slide, &QPropertyAnimation::valueChanged, this, [this](const QVariant& value) { setMinimumWidth(value.toInt()); });
    connect(m_slide, &QPropertyAnimation::finished, this, [this] {
        if (!m_open)
            hide();
    });

    m_mojang = new MojangSkinFetcher(network, this);
    connect(m_mojang, &MojangSkinFetcher::finished, this, [this](const QString& key, const QImage& texture, bool) {
        for (const QString& name : QStringList(m_loading.values())) {
            if (MojangSkinFetcher::keyForName(name) != key)
                continue;
            if (!texture.isNull()) {
                skinLoaded(name, texture);
                continue;
            }
            // No Mojang skin either: the game's default skin for this name.
            skinLoaded(name, DefaultSkin::forUuid(XylarSkinClient::computeOfflineUUID(name)).texture);
        }
    });

    connect(m_client, &SocialClient::changed, this, [this] {
        if (m_rebuildPending)
            return;
        m_rebuildPending = true;
        QTimer::singleShot(0, this, [this] {
            m_rebuildPending = false;
            rebuild();
        });
    });
    connect(m_client, &SocialClient::actionFinished, this, [this](bool ok, const QString& message) {
        m_addField->setEnabled(true);
        if (ok)
            m_addField->clear();
        if (!message.isEmpty())
            showNote(message, !ok);
    });

    updateStyle();
    rebuild();
}

void SocialPanel::setOpen(bool open, bool animate)
{
    m_open = open;
    m_client->setWatching(open);
    m_slide->stop();
    if (open)
        show();
    if (!animate) {
        setMinimumWidth(open ? kPanelWidth : 0);
        setMaximumWidth(open ? kPanelWidth : 0);
        if (!open)
            hide();
        return;
    }
    m_slide->setStartValue(width());
    m_slide->setEndValue(open ? kPanelWidth : 0);
    m_slide->start();
}

void SocialPanel::setInviteTarget(const QString& serverTitle)
{
    if (serverTitle == m_inviteTitle)
        return;
    m_inviteTitle = serverTitle;
    if (!m_rebuildPending) {
        m_rebuildPending = true;
        QTimer::singleShot(0, this, [this] {
            m_rebuildPending = false;
            rebuild();
        });
    }
}

void SocialPanel::addFriend()
{
    const QString name = m_addField->text().trimmed();
    if (name.length() < 3)
        return;
    if (name.compare(m_client->player(), Qt::CaseInsensitive) == 0) {
        showNote(tr("That's you."), true);
        return;
    }
    m_addField->setEnabled(false);
    m_client->sendRequest(name);
}

void SocialPanel::showNote(const QString& text, bool bad)
{
    m_note->setText(text);
    m_note->setProperty("bad", bad);
    m_note->style()->unpolish(m_note);
    m_note->style()->polish(m_note);
    m_note->show();
    const QString shown = text;
    QTimer::singleShot(6000, m_note, [this, shown] {
        if (m_note->text() == shown)
            m_note->hide();
    });
}

void SocialPanel::rebuild()
{
    clearLayout(m_list);

    const QString me = m_client->player();
    const bool hasPlayer = !me.isEmpty();
    m_addField->setEnabled(hasPlayer && m_client->ready());

    if (!hasPlayer) {
        auto empty = new QLabel(tr("Pick an account to see your friends. Offline accounts work too."), m_list->parentWidget());
        empty->setObjectName(QStringLiteral("socialEmpty"));
        empty->setWordWrap(true);
        m_list->addWidget(empty);
        m_list->addStretch(1);
        return;
    }

    m_list->addWidget(makePersonRow(me, m_client->activity(), statusLine(m_client->activity(), m_client->activityDetail(), 0), true));

    if (!m_client->lastError().isEmpty()) {
        auto error = new QLabel(m_client->lastError(), m_list->parentWidget());
        error->setObjectName(QStringLiteral("socialError"));
        error->setWordWrap(true);
        m_list->addWidget(error);
        auto retry = new QPushButton(tr("Try again"), m_list->parentWidget());
        retry->setObjectName(QStringLiteral("socialQuietButton"));
        retry->setCursor(Qt::PointingHandCursor);
        connect(retry, &QPushButton::clicked, m_client, &SocialClient::refresh);
        m_list->addWidget(retry, 0, Qt::AlignLeft);
    }

    if (!m_client->invites().isEmpty()) {
        m_list->addWidget(makeSection(QStringLiteral("invites"), tr("Invites"), m_client->invites().size()));
        if (!m_collapsed.contains(QStringLiteral("invites")))
            for (const auto& invite : m_client->invites())
                m_list->addWidget(makeInviteRow(invite.name, invite.server, invite.version, invite.title));
    }

    if (!m_client->incoming().isEmpty()) {
        m_list->addWidget(makeSection(QStringLiteral("incoming"), tr("Requests"), m_client->incoming().size()));
        if (!m_collapsed.contains(QStringLiteral("incoming")))
            for (const auto& request : m_client->incoming())
                m_list->addWidget(makeRequestRow(request.name, true));
    }

    QList<SocialClient::Friend> online, away, offline;
    for (const auto& person : m_client->friends()) {
        if (isOnline(person.state))
            online.append(person);
        else if (person.state == QLatin1String("away"))
            away.append(person);
        else
            offline.append(person);
    }
    // Playing first, then the most recently seen offline friends.
    std::stable_sort(online.begin(), online.end(), [](const auto& a, const auto& b) {
        return (a.state == QLatin1String("playing")) > (b.state == QLatin1String("playing"));
    });
    std::stable_sort(offline.begin(), offline.end(), [](const auto& a, const auto& b) { return a.seenAt > b.seenAt; });

    const struct {
        const char* key;
        QString title;
        const QList<SocialClient::Friend>& people;
    } groups[] = { { "online", tr("Online"), online }, { "away", tr("Away"), away }, { "offline", tr("Offline"), offline } };
    for (const auto& group : groups) {
        const QString key = QString::fromLatin1(group.key);
        m_list->addWidget(makeSection(key, group.title, group.people.size()));
        if (m_collapsed.contains(key))
            continue;
        for (const auto& person : group.people)
            m_list->addWidget(makePersonRow(person.name, person.state, statusLine(person.state, person.detail, person.seenAt), false,
                                            person.state == QLatin1String("playing") ? person.server : QString(), person.version));
    }

    if (m_client->loaded() && m_client->friends().isEmpty() && m_client->incoming().isEmpty()) {
        auto hint = new QLabel(tr("No friends yet. Add someone by their Minecraft name, and they'll see your request here."),
                               m_list->parentWidget());
        hint->setObjectName(QStringLiteral("socialEmpty"));
        hint->setWordWrap(true);
        m_list->addWidget(hint);
    }

    if (!m_client->outgoing().isEmpty()) {
        m_list->addWidget(makeSection(QStringLiteral("outgoing"), tr("Sent"), m_client->outgoing().size()));
        if (!m_collapsed.contains(QStringLiteral("outgoing")))
            for (const auto& request : m_client->outgoing())
                m_list->addWidget(makeRequestRow(request.name, false));
    }

    m_list->addStretch(1);
}

QWidget* SocialPanel::makeSection(const QString& key, const QString& title, int count)
{
    auto button = new QPushButton(m_list->parentWidget());
    button->setObjectName(QStringLiteral("socialSection"));
    button->setCursor(Qt::PointingHandCursor);
    const bool collapsed = m_collapsed.contains(key);
    // Chevron, then the title and how many: "▾ Online 3".
    button->setText(QStringLiteral("%1  %2  %3").arg(collapsed ? QStringLiteral("▸") : QStringLiteral("▾"), title).arg(count));
    button->setAccessibleName(collapsed ? tr("Show %1").arg(title) : tr("Hide %1").arg(title));
    connect(button, &QPushButton::clicked, this, [this, key] {
        if (!m_collapsed.remove(key))
            m_collapsed.insert(key);
        rebuild();
    });
    return button;
}

QWidget* SocialPanel::makePersonRow(const QString& name, const QString& state, const QString& status, bool self, const QString& server,
                                    const QString& version)
{
    auto row = new QWidget(m_list->parentWidget());
    row->setObjectName(self ? QStringLiteral("socialSelf") : QStringLiteral("socialRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 6, 6);
    layout->setSpacing(10);

    auto avatar = new QLabel(row);
    avatar->setFixedSize(kAvatar + 4, kAvatar + 4);
    avatar->setPixmap(avatarFor(name, state, kAvatar));
    layout->addWidget(avatar);

    auto text = new QVBoxLayout;
    text->setSpacing(1);
    auto nameLabel = new QLabel(name, row);
    nameLabel->setObjectName(QStringLiteral("socialName"));
    auto statusLabel = new QLabel(status, row);
    statusLabel->setObjectName(isOnline(state) ? QStringLiteral("socialStatusOn") : QStringLiteral("socialStatus"));
    statusLabel->setToolTip(status);
    // Long instance names get an ellipsis instead of widening the panel.
    statusLabel->setText(statusLabel->fontMetrics().elidedText(status, Qt::ElideRight, kPanelWidth - kAvatar - 90));
    // Labels give way to the buttons instead of pushing the row wider than the panel.
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    text->addWidget(nameLabel);
    text->addWidget(statusLabel);
    layout->addLayout(text, 1);

    if (!self && !server.isEmpty()) {
        auto join = new QPushButton(tr("Join"), row);
        join->setObjectName(QStringLiteral("socialAccept"));
        join->setCursor(Qt::PointingHandCursor);
        join->setToolTip(tr("Play on %1 with %2").arg(server, name));
        connect(join, &QPushButton::clicked, this, [this, server, version, name] { emit joinRequested(server, version, name); });
        layout->addWidget(join);
    } else if (!self && !m_inviteTitle.isEmpty() && isOnline(state)) {
        auto invite = new QPushButton(tr("Invite"), row);
        invite->setObjectName(QStringLiteral("socialQuietButton"));
        invite->setCursor(Qt::PointingHandCursor);
        invite->setToolTip(tr("Invite %1 to %2").arg(name, m_inviteTitle));
        connect(invite, &QPushButton::clicked, this, [this, name] { emit inviteRequested(name); });
        layout->addWidget(invite);
    }

    if (!self) {
        auto more = new QToolButton(row);
        more->setObjectName(QStringLiteral("socialMore"));
        more->setText(QStringLiteral("⋯"));
        more->setToolTip(tr("More"));
        more->setCursor(Qt::PointingHandCursor);
        more->setPopupMode(QToolButton::InstantPopup);
        auto menu = new QMenu(more);
        menu->addAction(tr("Copy name"), this, [name] { QGuiApplication::clipboard()->setText(name); });
        if (!m_inviteTitle.isEmpty())
            menu->addAction(tr("Invite to %1").arg(m_inviteTitle), this, [this, name] { emit inviteRequested(name); });
        menu->addSeparator();
        menu->addAction(tr("Remove friend"), this, [this, name] {
            const auto answer = QMessageBox::question(this, tr("Remove friend"), tr("Remove %1 from your friends?").arg(name),
                                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer == QMessageBox::Yes)
                m_client->remove(name);
        });
        more->setMenu(menu);
        layout->addWidget(more);
    }
    return row;
}

QWidget* SocialPanel::makeRequestRow(const QString& name, bool incoming)
{
    auto row = new QWidget(m_list->parentWidget());
    row->setObjectName(QStringLiteral("socialRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 6, 6);
    layout->setSpacing(10);

    auto avatar = new QLabel(row);
    avatar->setFixedSize(kAvatar + 4, kAvatar + 4);
    avatar->setPixmap(avatarFor(name, QString(), kAvatar));
    layout->addWidget(avatar);

    auto text = new QVBoxLayout;
    text->setSpacing(1);
    auto nameLabel = new QLabel(name, row);
    nameLabel->setObjectName(QStringLiteral("socialName"));
    auto statusLabel = new QLabel(incoming ? tr("Friend request") : tr("Waiting for them"), row);
    statusLabel->setObjectName(QStringLiteral("socialStatus"));
    // Labels give way to the buttons instead of pushing the row wider than the panel.
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    text->addWidget(nameLabel);
    text->addWidget(statusLabel);
    layout->addLayout(text, 1);

    layout->setSpacing(8);
    if (incoming) {
        auto accept = new QPushButton(tr("Accept"), row);
        accept->setObjectName(QStringLiteral("socialAccept"));
        accept->setCursor(Qt::PointingHandCursor);
        connect(accept, &QPushButton::clicked, this, [this, name] { m_client->accept(name); });
        layout->addWidget(accept);
    }
    auto decline = new QToolButton(row);
    decline->setObjectName(QStringLiteral("socialMore"));
    decline->setText(QStringLiteral("✕"));
    decline->setToolTip(incoming ? tr("Decline") : tr("Cancel request"));
    decline->setCursor(Qt::PointingHandCursor);
    connect(decline, &QToolButton::clicked, this, [this, name] { m_client->decline(name); });
    layout->addWidget(decline);
    return row;
}

QWidget* SocialPanel::makeInviteRow(const QString& name, const QString& server, const QString& version, const QString& title)
{
    auto row = new QWidget(m_list->parentWidget());
    row->setObjectName(QStringLiteral("socialRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 6, 6);
    layout->setSpacing(8);

    auto avatar = new QLabel(row);
    avatar->setFixedSize(kAvatar + 4, kAvatar + 4);
    avatar->setPixmap(avatarFor(name, QString(), kAvatar));
    layout->addWidget(avatar);

    auto text = new QVBoxLayout;
    text->setSpacing(1);
    auto nameLabel = new QLabel(name, row);
    nameLabel->setObjectName(QStringLiteral("socialName"));
    const QString where = title.isEmpty() ? server : title;
    const QString status = version.isEmpty() ? tr("Invites you to %1").arg(where) : tr("Invites you to %1 (%2)").arg(where, version);
    auto statusLabel = new QLabel(row);
    statusLabel->setObjectName(QStringLiteral("socialStatusOn"));
    statusLabel->setToolTip(QStringLiteral("%1\n%2").arg(status, server));
    statusLabel->setText(statusLabel->fontMetrics().elidedText(status, Qt::ElideRight, kPanelWidth - kAvatar - 150));
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    text->addWidget(nameLabel);
    text->addWidget(statusLabel);
    layout->addLayout(text, 1);

    auto join = new QPushButton(tr("Join"), row);
    join->setObjectName(QStringLiteral("socialAccept"));
    join->setCursor(Qt::PointingHandCursor);
    connect(join, &QPushButton::clicked, this, [this, server, version, name] {
        emit joinRequested(server, version, name);
        m_client->dismissInvite(name);
    });
    layout->addWidget(join);

    auto dismiss = new QToolButton(row);
    dismiss->setObjectName(QStringLiteral("socialMore"));
    dismiss->setText(QStringLiteral("✕"));
    dismiss->setToolTip(tr("Not now"));
    dismiss->setCursor(Qt::PointingHandCursor);
    connect(dismiss, &QToolButton::clicked, this, [this, name] { m_client->dismissInvite(name); });
    layout->addWidget(dismiss);
    return row;
}

QPixmap SocialPanel::faceFrom(const QImage& skin, int size, qreal dpr)
{
    const int px = qRound(size * dpr);
    QImage face(px, px, QImage::Format_ARGB32_Premultiplied);
    face.fill(Qt::transparent);
    if (skin.width() >= 64 && skin.height() >= 32) {
        const int scale = skin.width() / 64;
        QPainter painter(&face);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(QRect(0, 0, px, px), skin, QRect(8 * scale, 8 * scale, 8 * scale, 8 * scale));
        const QRect hat(40 * scale, 8 * scale, 8 * scale, 8 * scale);
        if (hatIsDrawn(skin, hat))
            painter.drawImage(QRect(0, 0, px, px), skin, hat);
    }
    QPixmap out = QPixmap::fromImage(face);
    out.setDevicePixelRatio(dpr);
    return out;
}

QPixmap SocialPanel::avatarFor(const QString& name, const QString& state, int size)
{
    const qreal dpr = devicePixelRatioF();
    const int box = size + 4;
    QPixmap canvas(qRound(box * dpr), qRound(box * dpr));
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);

    const QString key = name.toLower();
    if (!m_skins.contains(key) || QDateTime::currentMSecsSinceEpoch() - m_skinTimes.value(key) > kSkinMaxAgeMs)
        loadSkin(name);
    const QImage skin = m_skins.value(key);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF faceRect(0, 0, size, size);
    QPainterPath clip;
    clip.addRoundedRect(faceRect, 7, 7);
    painter.setClipPath(clip);
    if (!skin.isNull()) {
        painter.drawPixmap(faceRect.toRect(), faceFrom(skin, size, dpr));
    } else {
        // Still loading, or no skin anywhere: the first letter on a quiet tile.
        const QColor fg = palette().color(QPalette::WindowText);
        painter.fillRect(faceRect, QColor(fg.red(), fg.green(), fg.blue(), 30));
        QFont font = painter.font();
        font.setPixelSize(size / 2);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(fg);
        painter.drawText(faceRect, Qt::AlignCenter, name.left(1).toUpper());
    }
    painter.setClipping(false);

    if (!state.isEmpty()) {
        // Status dot on the corner, ringed in the panel colour so it reads on any face.
        const qreal dot = 12;
        const QRectF dotRect(size - dot + 3, size - dot + 3, dot, dot);
        painter.setPen(QPen(m_body->palette().color(QPalette::Window), 2.5));
        painter.setBrush(stateColor(state));
        painter.drawEllipse(dotRect);
    }
    return canvas;
}

void SocialPanel::loadSkin(const QString& name)
{
    const QString key = name.toLower();
    if (m_loading.contains(key))
        return;
    m_loading.insert(key);

    // XylarSkins first: it's what other XylarJava players see in game.
    const QString base = APPLICATION->settings()->get("XylarSkinBackendUrl").toString().trimmed();
    const QString uuid = XylarSkinClient::computeOfflineUUID(name);
    if (base.isEmpty()) {
        m_mojang->fetchByName(name);
        return;
    }
    QNetworkRequest request{ QUrl(base + QStringLiteral("/api/skin/") + uuid) };
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    const QString token = APPLICATION->settings()->get("XylarSkinToken").toString();
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
        reply->deleteLater();
        QImage skin;
        if (reply->error() == QNetworkReply::NoError && skin.loadFromData(reply->readAll(), "PNG") && skin.width() >= 64) {
            skinLoaded(name, skin);
            return;
        }
        MojangSkinFetcher::Skin cached;
        if (m_mojang->cached(MojangSkinFetcher::keyForName(name), &cached)) {
            skinLoaded(name, cached.texture.isNull() ? DefaultSkin::forUuid(XylarSkinClient::computeOfflineUUID(name)).texture
                                                     : cached.texture);
            return;
        }
        m_mojang->fetchByName(name);
    });
}

void SocialPanel::refreshSkin(const QString& name)
{
    if (name.isEmpty())
        return;
    m_skinTimes.remove(name.toLower());
    loadSkin(name);
}

void SocialPanel::skinLoaded(const QString& name, const QImage& skin)
{
    const QString key = name.toLower();
    m_loading.remove(key);
    m_skinTimes.insert(key, QDateTime::currentMSecsSinceEpoch());
    const bool same = m_skins.contains(key) && m_skins.value(key) == skin;
    m_skins.insert(key, skin);
    if (same)
        return;  // nothing new to draw
    if (!m_rebuildPending) {
        m_rebuildPending = true;
        QTimer::singleShot(0, this, [this] {
            m_rebuildPending = false;
            rebuild();
        });
    }
}

void SocialPanel::changeEvent(QEvent* event)
{
    switch (event->type()) {
        case QEvent::PaletteChange:
        case QEvent::ApplicationPaletteChange:
        case QEvent::StyleChange:
            QTimer::singleShot(0, this, &SocialPanel::updateStyle);
            break;
        default:
            break;
    }
    QFrame::changeEvent(event);
}

void SocialPanel::updateStyle()
{
    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = HomePage::accentFor(pal);
    const QColor accentInk = HomePage::textOn(accent);
    const bool dark = bg.lightnessF() < 0.5f;
    const QColor side = dark ? bg.darker(118) : bg.darker(104);
    const QColor field = dark ? bg.darker(135) : bg.lighter(104);

    const QString qss =
        QStringLiteral(
            "#socialPanel { background-color: %1; border: none; border-left: 1px solid %2; }"
            "#socialBody, #socialList, #socialScroll, #socialScroll > QWidget > QWidget { background: transparent; }"
            "#socialTitle { color: %3; font-size: 17px; font-weight: 800; }"
            "#socialClose, #socialMore { color: %4; background: transparent; border: none; border-radius: 8px;"
            " min-width: 26px; max-width: 26px; min-height: 26px; font-size: 14px; }"
            "#socialClose:hover, #socialMore:hover { color: %3; background-color: %5; }"
            "#socialMore::menu-indicator { image: none; width: 0; }"
            "#socialAddField { color: %3; background-color: %6; border: 1px solid %2; border-radius: 10px; padding: 7px 10px; }"
            "#socialAddField:focus { border-color: %7; }"
            "#socialAddButton, #socialAccept { color: %8; background-color: %7; border: none; border-radius: 10px;"
            " padding: 6px 12px; font-weight: 700; }"
            "#socialAddButton:hover, #socialAccept:hover { background-color: %9; }"
            "#socialAddButton:disabled { color: %4; background-color: %5; }"
            "#socialNote { color: %4; font-size: 12px; }"
            "#socialNote[bad=\"true\"] { color: #e5534b; }"
            "#socialError { color: #e5534b; font-size: 12px; padding: 4px 8px; }"
            "#socialEmpty { color: %4; font-size: 12px; padding: 8px; }"
            "#socialQuietButton { color: %3; background: transparent; border: 1px solid %2; border-radius: 9px; padding: 5px 12px; }"
            "#socialQuietButton:hover { background-color: %5; }"
            "#socialSection { color: %4; background: transparent; border: none; text-align: left; padding: 12px 6px 4px 6px;"
            " font-size: 12px; font-weight: 700; }"
            "#socialSection:hover { color: %3; }"
            "#socialRow, #socialSelf { background: transparent; border-radius: 12px; }"
            "#socialRow:hover { background-color: %5; }"
            "#socialSelf { background-color: %5; border: 1px solid %2; }"
            "#socialName { color: %3; font-size: 13px; font-weight: 700; }"
            "#socialStatus { color: %4; font-size: 12px; }"
            "#socialStatusOn { color: #3fb950; font-size: 12px; }")
            .arg(side.name(), rgba(fg, 28), fg.name(), rgba(fg, 160), rgba(fg, 18), field.name(), accent.name(), accentInk.name(),
                 accent.lighter(112).name());

    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
}

SocialButton::SocialButton(QWidget* host) : QToolButton(host)
{
    setObjectName(QStringLiteral("socialButton"));
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(44, 44);
    setToolTip(tr("Social"));
    setAccessibleName(tr("Social"));
    host->installEventFilter(this);
    place();
    raise();
}

void SocialButton::setBadge(int count)
{
    if (count == m_badge)
        return;
    m_badge = count;
    setToolTip(count ? tr("Social (%n friend request(s))", nullptr, count) : tr("Social"));
    update();
}

bool SocialButton::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize)
            place();
        else if (event->type() == QEvent::ChildAdded || event->type() == QEvent::LayoutRequest)
            raise();  // stay above pages that are added or shown later
    }
    return QToolButton::eventFilter(watched, event);
}

void SocialButton::place()
{
    if (auto host = parentWidget())
        move(host->width() - width() - 18, 14);
}

void SocialButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QToolButton::enterEvent(event);
}

void SocialButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QToolButton::leaveEvent(event);
}

void SocialButton::paintEvent(QPaintEvent*)
{
    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = HomePage::accentFor(pal);
    const bool dark = bg.lightnessF() < 0.5f;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QColor fill = dark ? bg.darker(118) : bg.darker(104);
    if (m_hovered || isDown())
        fill = dark ? fill.lighter(125) : fill.darker(106);
    painter.setPen(QPen(isChecked() ? accent : QColor(fg.red(), fg.green(), fg.blue(), 40), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(box, 14, 14);

    // Two friends side by side: drawn here so it follows the theme colours at any scale.
    const QColor ink = isChecked() ? accent : fg;
    const QPointF c = QRectF(rect()).center();
    auto person = [&painter](QPointF head, qreal r, const QColor& color, const QColor& gap) {
        QPainterPath shape;
        shape.addEllipse(head, r, r);
        const QRectF shoulders(head.x() - r * 2.05, head.y() + r * 1.45, r * 4.1, r * 4.2);
        QPainterPath body;
        body.moveTo(shoulders.left(), shoulders.center().y());
        body.arcTo(shoulders, 180, -180);
        body.closeSubpath();
        shape.addPath(body);
        painter.setPen(QPen(gap, 2.2));
        painter.setBrush(color);
        painter.drawPath(shape.simplified());
    };
    QColor back = ink;
    back.setAlphaF(0.55);
    painter.save();
    painter.setClipRect(QRectF(rect()).adjusted(4, 4, -4, -11));
    person(c + QPointF(4.5, -4.5), 3.4, back, Qt::transparent);
    person(c + QPointF(-2.5, -3), 4.2, ink, fill);
    painter.restore();

    if (m_badge > 0) {
        const QString text = m_badge > 9 ? QStringLiteral("9+") : QString::number(m_badge);
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        const qreal w = qMax<qreal>(16, QFontMetricsF(font).horizontalAdvance(text) + 8);
        const QRectF badge(width() - w + 2, -2, w, 16);
        painter.setPen(QPen(fill, 2));
        painter.setBrush(QColor(0xe5, 0x53, 0x4b));
        painter.drawRoundedRect(badge, 8, 8);
        painter.setPen(Qt::white);
        painter.drawText(badge, Qt::AlignCenter, text);
    }
}

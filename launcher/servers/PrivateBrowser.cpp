#include "PrivateBrowser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include "BuildConfig.h"
#include "DesktopServices.h"

namespace PrivateBrowser {

namespace {

// Mozilla's add-on site always serves the latest signed release here.
const char* UBLOCK_XPI = "https://addons.mozilla.org/firefox/downloads/latest/ublock-origin/latest.xpi";
// Firefox installs an add-on dropped in "<profile>/extensions/" when the file is named after its ID.
const char* UBLOCK_ID = "uBlock0@raymondhill.net";

const char* USER_JS = R"(// Written by XylarJava for its private browser.
// Turn on add-ons placed in this profile (uBlock Origin) without asking.
user_pref("extensions.autoDisableScopes", 0);
user_pref("extensions.enabledScopes", 15);
// No first-run pages, default-browser prompts, studies or telemetry.
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("browser.startup.homepage_override.mstone", "ignore");
user_pref("startup.homepage_welcome_url", "");
user_pref("startup.homepage_welcome_url.additional", "");
user_pref("trailhead.firstrun.didSeeAboutWelcome", true);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("app.shield.optoutstudies.enabled", false);
user_pref("browser.newtabpage.activity-stream.showSponsored", false);
user_pref("browser.newtabpage.activity-stream.showSponsoredTopSites", false);
)";

QString profileDir(const QString& dataRoot)
{
    return QDir(dataRoot).filePath(QStringLiteral("browser"));
}

void launch(const QUrl& url, const QString& profile)
{
    // With --profile, Firefox hands the URL to this profile's window if it's already open,
    // and starts a separate one next to the user's own Firefox otherwise.
    const bool started = QProcess::startDetached(firefox(), { QStringLiteral("--profile"), profile, QStringLiteral("--new-window"), url.toString() });
    if (!started)
        DesktopServices::openUrl(url);
}

}  // namespace

QString firefox()
{
    for (const char* name : { "firefox", "firefox-bin", "firefox-esr", "librewolf" }) {
        const QString found = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!found.isEmpty())
            return found;
    }
    return QString();
}

void open(const QUrl& url, const QString& dataRoot, QNetworkAccessManager* network)
{
    if (firefox().isEmpty()) {
        DesktopServices::openUrl(url);
        return;
    }
    const QString profile = profileDir(dataRoot);
    const QString extensions = QDir(profile).filePath(QStringLiteral("extensions"));
    QDir().mkpath(extensions);
    QFile::setPermissions(profile, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QSaveFile prefs(QDir(profile).filePath(QStringLiteral("user.js")));
    if (prefs.open(QIODevice::WriteOnly)) {
        prefs.write(USER_JS);
        prefs.commit();
    }

    const QString xpi = QDir(extensions).filePath(QString::fromLatin1(UBLOCK_ID) + QStringLiteral(".xpi"));
    if (QFileInfo(xpi).size() > 0) {
        launch(url, profile);
        return;
    }
    QNetworkRequest request(QUrl(QString::fromLatin1(UBLOCK_XPI)));
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, xpi, url, profile] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        // An .xpi is a zip; anything else (an error page) isn't saved. The browser opens either way.
        if (reply->error() == QNetworkReply::NoError && data.startsWith("PK")) {
            QSaveFile file(xpi);
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
                file.commit();
        }
        launch(url, profile);
    });
}

}  // namespace PrivateBrowser

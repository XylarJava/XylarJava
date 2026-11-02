#pragma once

#include <QString>
#include <QUrl>

class QNetworkAccessManager;

/**
 * A Firefox window with its own profile ("<data folder>/browser/") and uBlock Origin, for sites the
 * launcher sends people to (playit.gg sign-up and tunnels). Kept apart from the user's own Firefox:
 * no history, logins or extensions are shared either way.
 */
namespace PrivateBrowser {

/// The Firefox (or LibreWolf) executable, empty when none is installed.
QString firefox();

/// Opens the URL in the private Firefox, installing uBlock Origin there first if needed.
/// Falls back to the default browser when Firefox isn't installed.
void open(const QUrl& url, const QString& dataRoot, QNetworkAccessManager* network);

}  // namespace PrivateBrowser

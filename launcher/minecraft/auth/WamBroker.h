#pragma once

#include <string>

// Windows Web Account Manager (the Windows token broker). Signs in with the Microsoft accounts
// Windows already knows about, without a browser. The Azure app needs the redirect URI
// ms-appx-web://Microsoft.AAD.BrokerPlugin/<client id> under "Mobile and desktop applications".
//
// Kept free of Qt headers: C++/WinRT and Qt's keyword macros don't mix in one translation unit.
// All calls block, so run them on a worker thread.
namespace wambroker {

enum class Status { Success, UserInteractionRequired, Unavailable, Cancelled, Failed };

struct Result {
    Status status = Status::Failed;
    std::string accessToken;
    std::string accountId;    // WebAccount id, for later silent requests
    std::string accountName;  // e-mail shown by Windows
    long long expiresAt = 0;  // unix seconds, 0 when the broker did not say
    std::string error;
};

// false when this build has no broker support (non Windows, or not built with MSVC)
bool isSupported();

// Shows the Windows account picker over parentWindow (an HWND).
Result requestToken(const std::string& clientId, void* parentWindow);

// Token for an account picked earlier, without any UI.
Result getTokenSilently(const std::string& clientId, const std::string& accountId);

}  // namespace wambroker

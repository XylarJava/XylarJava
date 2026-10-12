#include "WamBroker.h"

#include <cstdio>
#include <cstdlib>

#include <windows.h>

#include <inspectable.h>
#include <webauthenticationcoremanagerinterop.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Authentication.Web.Core.h>
#include <winrt/Windows.Security.Credentials.h>

namespace wambroker {
namespace {

using namespace winrt::Windows::Security::Authentication::Web::Core;
using winrt::Windows::Security::Credentials::WebAccount;
using winrt::Windows::Security::Credentials::WebAccountProvider;

constexpr wchar_t kProviderId[] = L"https://login.microsoft.com";
constexpr wchar_t kAuthority[] = L"consumers";
constexpr wchar_t kScopes[] = L"XboxLive.signin offline_access";

// the calling thread may be a fresh std::thread, give it an apartment for the WinRT calls
struct Apartment {
    bool owned = false;
    Apartment()
    {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            owned = true;
        } catch (const winrt::hresult_error&) {
            // already initialised by someone else, which is fine
        }
    }
    ~Apartment()
    {
        if (owned)
            winrt::uninit_apartment();
    }
};

std::string hex(uint32_t value)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08X", value);
    return buffer;
}

WebAccountProvider findProvider(Result& out)
{
    WebAccountProvider provider = WebAuthenticationCoreManager::FindAccountProviderAsync(kProviderId, kAuthority).get();
    if (!provider) {
        out.status = Status::Unavailable;
        out.error = "Windows has no Microsoft account provider.";
    }
    return provider;
}

WebTokenRequest makeRequest(const WebAccountProvider& provider, const std::string& clientId)
{
    WebTokenRequest request(provider, kScopes, winrt::to_hstring(clientId));
    // v2 tokens for a converged (Azure) app from the Microsoft account provider, the way MSAL asks
    // for them; without it the provider reports the app as disabled or misconfigured (0x80860003)
    request.Properties().Insert(L"api-version", L"2.0");
    return request;
}

Result fromResult(const WebTokenRequestResult& result)
{
    Result out;
    const WebTokenRequestStatus status = result.ResponseStatus();
    if (status == WebTokenRequestStatus::Success && result.ResponseData().Size() > 0) {
        const WebTokenResponse response = result.ResponseData().GetAt(0);
        out.status = Status::Success;
        out.accessToken = winrt::to_string(response.Token());
        if (const WebAccount account = response.WebAccount()) {
            out.accountId = winrt::to_string(account.Id());
            out.accountName = winrt::to_string(account.UserName());
        }
        const auto properties = response.Properties();
        if (properties.HasKey(L"exp"))
            out.expiresAt = std::atoll(winrt::to_string(properties.Lookup(L"exp")).c_str());
        return out;
    }
    switch (status) {
        case WebTokenRequestStatus::UserInteractionRequired:
            out.status = Status::UserInteractionRequired;
            break;
        case WebTokenRequestStatus::UserCancel:
            out.status = Status::Cancelled;
            break;
        case WebTokenRequestStatus::AccountProviderNotAvailable:
            out.status = Status::Unavailable;
            break;
        default:
            out.status = Status::Failed;
            break;
    }
    if (const auto error = result.ResponseError())
        out.error = winrt::to_string(error.ErrorMessage()) + " (0x" + hex(error.ErrorCode()) + ")";
    else if (out.status == Status::Cancelled)
        out.error = "Sign-in was cancelled.";
    else
        out.error = "The Windows token broker did not return a token.";
    return out;
}

}  // namespace

bool isSupported()
{
    return true;
}

Result requestToken(const std::string& clientId, void* parentWindow)
{
    Apartment apartment;
    Result out;
    try {
        const WebAccountProvider provider = findProvider(out);
        if (!provider)
            return out;
        const WebTokenRequest request = makeRequest(provider, clientId);
        auto interop = winrt::get_activation_factory<WebAuthenticationCoreManager, ::IWebAuthenticationCoreManagerInterop>();
        winrt::Windows::Foundation::IAsyncOperation<WebTokenRequestResult> operation;
        winrt::check_hresult(interop->RequestTokenForWindowAsync(
            static_cast<HWND>(parentWindow), static_cast<::IInspectable*>(winrt::get_abi(request)),
            winrt::guid_of<winrt::Windows::Foundation::IAsyncOperation<WebTokenRequestResult>>(), winrt::put_abi(operation)));
        out = fromResult(operation.get());
    } catch (const winrt::hresult_error& ex) {
        out.status = Status::Failed;
        out.error = "Windows token broker sign-in failed: " + winrt::to_string(ex.message());
    }
    return out;
}

Result getTokenSilently(const std::string& clientId, const std::string& accountId)
{
    Apartment apartment;
    Result out;
    try {
        const WebAccountProvider provider = findProvider(out);
        if (!provider)
            return out;
        const WebAccount account = WebAuthenticationCoreManager::FindAccountAsync(provider, winrt::to_hstring(accountId)).get();
        if (!account) {
            out.status = Status::UserInteractionRequired;
            out.error = "Windows no longer has this Microsoft account.";
            return out;
        }
        out = fromResult(WebAuthenticationCoreManager::GetTokenSilentlyAsync(makeRequest(provider, clientId), account).get());
    } catch (const winrt::hresult_error& ex) {
        out.status = Status::Failed;
        out.error = "Windows token broker request failed: " + winrt::to_string(ex.message());
    }
    return out;
}

}  // namespace wambroker

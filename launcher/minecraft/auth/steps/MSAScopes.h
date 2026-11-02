#pragma once

// The scopes every Microsoft sign-in asks for, the same as XylarJava for Xbox and the Windows token
// broker: Xbox Live sign-in, plus offline_access for a refresh token.
inline constexpr char kMSAScopes[] = "XboxLive.signin offline_access";

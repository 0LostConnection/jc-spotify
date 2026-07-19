#pragma once

/** Spotify app constants (Client ID lives in secrets.hpp). */
namespace spotify {

constexpr char kRedirectUri[] = "https://jc-spotify.local/callback";
constexpr char kMdnsHostname[] = "jc-spotify";
constexpr char kAuthorizeUrl[] = "https://accounts.spotify.com/authorize";
constexpr char kTokenUrl[] = "https://accounts.spotify.com/api/token";
constexpr char kScopes[] =
    "user-read-playback-state "
    "user-modify-playback-state "
    "user-read-currently-playing "
    "user-library-read "
    "user-library-modify";

}  // namespace spotify

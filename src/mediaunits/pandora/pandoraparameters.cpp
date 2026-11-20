///
/// Pandora connection parameter implementation.
/// @file       pandoraparameters.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-03-17
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cassert>

#include "utility.h"
#include "pandora.h"

#define KEY_USERNAME "username"
#define KEY_PASSWORD "password"
#define KEY_PROXY "proxyUrl"
#define KEY_CONTROLPROXY "controlProxyUrl"
#define KEY_PAUSETIMEOUT "pauseTimeout"
#define KEY_PLAYLISTTIMEOUT "playlistTimeout"
#define KEY_CACHEMINIMUM "cacheMinimum"
#define KEY_CACHEMAXIMUM "cacheMaximum"

// Stuff for the JSON protocol
#define KEY_ACCOUNTTYPE "accountType"
#define KEY_RPCHOST "rpcHost"
#define KEY_PARTNER "partner"
#define KEY_PARTNERPASSWORD "partnerPassword"
#define KEY_DEVICE "deviceType"
#define KEY_ENCRYPTIONKEY "encryptionKey"
#define KEY_DECRYPTIONKEY "decryptionKey"

namespace Pandora {
    static const int MinimumCacheMargin = 100;

    /// Name to enum translation for Pandora connection parameters.
    const LookupTable<ProtocolNature> ProtocolNatureLookup ({ { "PandoraPlus", ProtocolNature::PANDORAPLUS },
                                                              { "PandoraOne", ProtocolNature::PANDORAPLUS },
                                                              { "PandoraPremium", ProtocolNature::PANDORAPREMIUM },
                                                              { "Standard", ProtocolNature::STANDARD },
                                                              { "Automatic", ProtocolNature::AUTOMATIC },
                                                              { "Custom", ProtocolNature::CUSTOM },
                                                              { "Plus", ProtocolNature::PANDORAPLUS },
                                                              { "Premium", ProtocolNature::PANDORAPREMIUM }});

    const JSONProtocolParameters JSONProtocolParameters::Standard{ .nature = ProtocolNature::STANDARD,
                                                                   .rpc_host = "tuner.pandora.com",
                                                                   .partner = "android",
                                                                   .partner_password
                                                                   = "AC7IBG09A3DTSYM4R41UJWL07VLN8JI7",
                                                                   .device = "android-generic",
                                                                   .encryption_key = "6#26FRL$ZWD",
                                                                   .decryption_key = "R=U!LH$O2B#" };

    const JSONProtocolParameters JSONProtocolParameters::PandoraPlus{ .nature = ProtocolNature::PANDORAPLUS,
                                                                      .rpc_host = "internal-tuner.pandora.com",
                                                                      .partner = "pandora one",
                                                                      .partner_password
                                                                      = "TVCKIBGS9AO9TSYLNNFUML0743LH82D",
                                                                      .device = "D01",
                                                                      .encryption_key = "2%3WCL*JU$MP]4",
                                                                      .decryption_key = "U#IO$RZPAB%VX2" };

    ConnectionParameters::ConnectionParameters (void) : SourceParameters (Ownership::Type::SHARED){};

    Parsnip::OptionParser::Definitions ConnectionParameters::parser_definitions() {
        Parsnip::OptionParser::Definitions parser{ Media::SourceParameters::parser_definitions() };
        static const Parsnip::OptionParser::Definitions pandora_options
                = { "account type <" KEY_ACCOUNTTYPE ":automatic|standard|plus|premium>", // Set all settings
                    "proxy {" KEY_PROXY "}", // Set proxy for all connections
                    "control proxy {" KEY_CONTROLPROXY "}",            // Set the proxy for control connection only
                    "pause timeout {#" KEY_PAUSETIMEOUT ":15-86400}",  // Timeout if paused too long
                    "playlist timeout {#" KEY_PLAYLISTTIMEOUT ":1800-86400}",  // Expire items queued too long
                    "cache minimum {#" KEY_CACHEMINIMUM ":800-989999} maximum {#" KEY_CACHEMAXIMUM ":1000-999999}",
                    // JSON protocol parameters
                    "rpc host {" KEY_RPCHOST "}",
                    "partner {" KEY_PARTNER "} {" KEY_PARTNERPASSWORD "}",
                    "pandora device {" KEY_DEVICE "}",
                    "encryption password {" KEY_ENCRYPTIONKEY "}",
                    "decryption password {" KEY_ENCRYPTIONKEY "}" };
        std::copy (pandora_options.begin(), pandora_options.end(), std::back_inserter (parser));
        return parser;
    }


    void ConnectionParameters::extractOptions (const Parsnip::Data &options) {
        SourceParameters::extractOptions (options);
        extract (options);
    }

    /** Verify that a proxy server contains a URL.
        If not, try decrypting the value and check again.
        @param proxy The proxy server URL.
        @param encryption_key The key used for encryption.
        @return The proxy server URL, unencrypted.
        @throw CommandError if the proxy is invalid. */
    std::string validate_and_decode_proxy (const std::string &proxy, std::string encryption_key) {
        if (strncasecmp (proxy.c_str(), "http:", 5) == 0 || strncasecmp (proxy.c_str(), "https:", 6) == 0) {
            return proxy;
        }
        std::string decrypted = lamer_cipher (encryption_key, proxy);
        if (strncasecmp (decrypted.c_str(), "http:", 5) == 0 || strncasecmp (decrypted.c_str(), "https:", 6) == 0) {
            return decrypted;
        }
        throw CommandError (E_INVALID, proxy);
    }
    
    void ConnectionParameters::extract (const Parsnip::Data &options) {
        if (options.contains (KEY_PROXY)) {
            proxy = validate_and_decode_proxy (options [KEY_PROXY].asString(), username);
        }
        if (options.contains (KEY_CONTROLPROXY)) {
            control_proxy = validate_and_decode_proxy (options [KEY_CONTROLPROXY].asString(), username);
        }
        if (options.contains (KEY_PAUSETIMEOUT)) {
            pause_timeout = options [KEY_PAUSETIMEOUT].asInteger();
        }
        if (options.contains (KEY_PLAYLISTTIMEOUT)) {
            playlist_expiration = options [KEY_PLAYLISTTIMEOUT].asInteger();
        }
        if (options.contains (KEY_CACHEMINIMUM)) {
            cache_minimum = options [KEY_CACHEMINIMUM].asInteger();
            cache_maximum = options [KEY_CACHEMAXIMUM].asInteger();
            if (cache_minimum + MinimumCacheMargin > cache_maximum) {
                throw CommandError (E_RANGE, "Cache margin must exceed " + std::to_string (MinimumCacheMargin));
            }
        }

        // Extract JSON protocol parameters
        ProtocolNature initial_setting;
        if (options.contains (KEY_ACCOUNTTYPE)) {
            initial_setting = ProtocolNatureLookup [options [KEY_ACCOUNTTYPE].asString()];
        } else {
            initial_setting = protocol.nature;
        }
        switch (initial_setting) {
            case ProtocolNature::CUSTOM:
                if (protocol.nature != ProtocolNature::CUSTOM) {
                    protocol = JSONProtocolParameters::Standard;
                }
                break;
            case ProtocolNature::AUTOMATIC:
            case ProtocolNature::STANDARD:
                protocol = JSONProtocolParameters::Standard;
                break;
            case ProtocolNature::PANDORAPLUS:
            case ProtocolNature::PANDORAPREMIUM:
                protocol = JSONProtocolParameters::PandoraPlus;
                break;
        }
        protocol.nature = initial_setting;

        if (options.contains (KEY_RPCHOST)) {
            protocol.rpc_host = options [KEY_RPCHOST].asString();
            protocol.nature = ProtocolNature::CUSTOM;
        }
        if (options.contains (KEY_PARTNER)) {
            protocol.partner = options [KEY_PARTNER].asString();
            protocol.nature = ProtocolNature::CUSTOM;
        }
        if (options.contains (KEY_PARTNERPASSWORD)) {
            protocol.partner_password = options [KEY_PARTNERPASSWORD].asString();
            protocol.nature = ProtocolNature::CUSTOM;
        }
        if (options.contains (KEY_DEVICE)) {
            protocol.device = options [KEY_DEVICE].asString();
            protocol.nature = ProtocolNature::CUSTOM;
        }
        if (options.contains (KEY_ENCRYPTIONKEY)) {
            protocol.encryption_key = options [KEY_ENCRYPTIONKEY].asString();
            protocol.nature = ProtocolNature::CUSTOM;
        }
        if (options.contains (KEY_DECRYPTIONKEY)) {
            protocol.decryption_key = options [KEY_DECRYPTIONKEY].asString();
            protocol.nature = ProtocolNature::CUSTOM;
        }
    }

    /** Create new connection parameters, retrieving values from user settings.
        @param src A user settings dictionary to retrieve settings from.
        @throw invalid_argument if values in the settings dictionary are invalid. */
    ConnectionParameters::ConnectionParameters (const UserData::JSONData &src) : SourceParameters (src) {
        username = src [KEY_USERNAME].asString();
        if (username.empty())
            throw std::invalid_argument ("username not recoverable");
        password = lamer_cipher (username, src [KEY_PASSWORD].asString());
        if (password.empty())
            throw std::invalid_argument ("user password missing");

        extract (src);
        if (cache_minimum < 800 || cache_minimum >= 1000000)
            throw std::invalid_argument ("Invalid cacheMinimum");
        if (cache_maximum < 1000 || cache_maximum >= 1000000)
            throw std::invalid_argument ("Invalid cacheMaximum");
    }

    /// Copy connection parameters into a user settings dictionary.
    bool ConnectionParameters::persist (UserData::JSONData &dest) const {
        dest [KEY_USERNAME] = username;
        dest [KEY_PASSWORD] = lamer_cipher (username, password);
        if (!proxy.empty()) {
            dest [KEY_PROXY] = lamer_cipher (username, proxy);
        }
        if (!control_proxy.empty()) {
            dest [KEY_CONTROLPROXY] = lamer_cipher (username, control_proxy);
        }
        dest [KEY_CACHEMINIMUM] = cache_minimum;
        dest [KEY_CACHEMAXIMUM] = cache_maximum;
        // Persist the JSON parameters
        dest [KEY_ACCOUNTTYPE] = ProtocolNatureLookup [protocol.nature];
        if (protocol.nature == ProtocolNature::CUSTOM) {
            dest [KEY_RPCHOST] = protocol.rpc_host;
            dest [KEY_PARTNER] = protocol.partner;
            dest [KEY_PARTNERPASSWORD] = protocol.partner_password;
            dest [KEY_DEVICE] = protocol.device;
            dest [KEY_ENCRYPTIONKEY] = protocol.encryption_key;
            dest [KEY_DECRYPTIONKEY] = protocol.decryption_key;
        }
        return SourceParameters::persist (dest);
    }
}  // namespace Pandora

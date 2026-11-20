///
/// Pandora communication.
/// @file       mediaunits/pandora/pandoracomm.h - pianod project
/// @author     Perette Barella
/// @date       2020-03-23
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>

#include <ctime>

#include <parsnip/parsnip.h>

#include "logging.h"
#include "httpclient.h"
#include "blowfish.h"

#include "pandoramessages.h"

namespace Pandora {
    /// Communication completion statuses
    enum class Status {
        Ok,
        CorruptMessage,
        MessageFormatUnknown,
        AllocationError,
        InternalError,
        CommunicationError,
        TooFrequentErrors,
        BadRequest = 400,
        Unauthorized = 401,
        StreamingViolation = 429,
        BadGateway = 502,
        PandoraError0 = 999,
        PandoraReadOnlyMode = 1000,
        InvalidAuthToken = 1001,
        InvalidPandoraCredentials = 1002,
        PandoraSubscriptionExpired = 1003,
        UserNotAuthorized = 1004,
        StationLimitReached = 1005,
        NoSuchStation = 1006,
        SharedStationNotMutable = 1008,
        DeviceNotAllowed = 1009,
        PartnerNotAuthorized = 1010,
        InvalidUsername = 1011,
        InvalidPassword = 1012,
        PlaylistRequestsExceeded = 1039
    };

    const std::string status_strerror (Status status);

    class UserFeatures {
    public:
        int inactivity_timeout{86400};
        int daily_skip_limit{60};
        int station_skip_limit{6};
        int max_stations{0};
        bool adverts{true};
        bool replays{false};
        bool hifi_audio_encoding{false};
        bool is_subscriber{false};
    };

    /** Pandora communication class.  Provided a request, assembles a JSON request
        and issues it to Pandora, then decodes the response.  This class manages
        the CSRF (cross-site request forgery) and authorization tokens, and
        reauthenticates if/when necessary. */
    class Communication {
    private:
        enum class State {
            Uninitialized,  /// Partner login not yet performed.
            Initialized,    /// Partner login complete, user not yet authenticated.
            Authenticated,  /// Ready to go...
            Failed          /// Bad credentials, refuse to work anymore.
        };

        JSONProtocolParameters protocol;
        const bool automatic_protocol_parameters {false};
        std::string username;
        std::string password;
        std::string proxy;
        
        std::string melodica_access;
        std::string partner_auth_token; ///< Partner authorization token, for JSON protocol.
        std::string partner_id;
        std::string user_auth_token; ///< Authorization token, retrieved during login.
        std::string listener_id;
        time_t synctime_offset;
        
        int sequential_failures{0};    ///< Number of failed transactions without successes.
        time_t lockout_until{0};       ///< Time until which communications are blocked/automatically fail.
        time_t session_expiration{0};  ///< Time at which session expires from inactivity.

        UserFeatures features;  ///< User capabilities, retrieved from login response.

        State state{State::Uninitialized};

        HttpClient http_client;
        BlowFish encryptor;
        BlowFish decryptor;

        Status performRequest (Request &request);
        Status authenticate();

    public:
        Communication (const std::string &name, const std::string &pass, const std::string &prox,
                        const JSONProtocolParameters &proto_params);
        Status execute (Request &request, bool retry_if_auth_required = true);
        Status partnerAuthenticate(); // Should be private, but used to check if able to initialize.
        void resetState ();
        
        Parsnip::Data persist () const;
        void restore (const Parsnip::Data &data);
        
        inline time_t retryTime() const {
            return lockout_until;
        }
        inline const UserFeatures &getFeatures() const {
            return features;
        }
        inline const time_t offlineUntil () const {
            return lockout_until;
        }
        inline const time_t sessionExpires () const {
            return session_expiration;
        }
        inline const bool isFailed() const {
            return state == State::Failed;
        }
    };

}  // namespace Pandora

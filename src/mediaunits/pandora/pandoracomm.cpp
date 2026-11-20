///
/// Pandora communication library.
/// @file       mediaunits/pandora/pandoratypes.h - pianod project
/// @author     Perette Barella
/// @date       2020-03-23
/// @copyright  Copyright 2020-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <string>

#include <cctype>

#include <curl/curl.h>

#include "logging.h"

#include "pandoraparameters.h"
#include "pandoramessages.h"
#include "pandoracomm.h"

namespace Pandora {
    static const std::unordered_map<Status, std::string> pandora_errors{
        { Status::Ok, "Ok" },
        { Status::CorruptMessage, "Corrupt message" },
        { Status::MessageFormatUnknown, "Unknown message format" },
        { Status::AllocationError, "Allocation error" },
        { Status::CommunicationError, "Communication error" },
        { Status::TooFrequentErrors, "Too many errors (temporary lockout)" },
        { Status::BadRequest, "HTTP 400/Bad request" },
        { Status::Unauthorized, "HTTP 401/Unauthorized" },
        { Status::StreamingViolation, "HTTP 429/Streaming Violation" },
        { Status::BadGateway, "HTTP 502/Bad Gateway" },
        { Status::PandoraError0, "Unexpected Pandora error" },
        { Status::PandoraReadOnlyMode, "Pandora is in read-only mode" },
        { Status::InvalidAuthToken, "Invalid or expired auth token" },
        { Status::InvalidPandoraCredentials, "Invalid Pandora credentials" },
        { Status::PandoraSubscriptionExpired, "Subscription or trial expired" },
        { Status::UserNotAuthorized, "User not authorized" },
        { Status::StationLimitReached, "Station limit reached" },
        { Status::NoSuchStation, "No such station" },
        { Status::SharedStationNotMutable, "Station not mutable (transformation required)" },
        { Status::DeviceNotAllowed, "Device not allowed" },
        { Status::PartnerNotAuthorized, "Partner not authorized" },
        { Status::InvalidUsername, "Invalid username" },
        { Status::InvalidPassword, "Invalid password" },
        { Status::PlaylistRequestsExceeded, "Excessive requests for playlist" },
    };

    /** Return a string corresponding to a Pandora communication status.
        @param status The communication status.
        @return The corresponding string. */
    const std::string status_strerror (Status status) {
        auto it = pandora_errors.find (status);
        if (it == pandora_errors.end()) {
            return "Error #" + std::to_string (int (status));
        }
        return it->second;
    }

    class PartnerLoginRequest : public Request {
        const BlowFish *decryptor{ nullptr };
        mutable std::string authorization_token;
        mutable std::string partner_id;
        mutable time_t sync_time;

    public:
        PartnerLoginRequest (const JSONProtocolParameters &params, BlowFish *decrypt)
        : Request (nullptr, "auth.partnerLogin", Option::TLS | Option::UNENCRYPTED),
          decryptor (decrypt) {
            request_message = Parsnip::Data::make_dictionary ({ { "username", params.partner },
                                                                { "password", params.partner_password },
                                                                { "deviceModel", params.device },
                                                                { "version", "5" } });
        }

        virtual void extractResponse (const Parsnip::Data &message) override {
            authorization_token = message ["partnerAuthToken"].asString();
            partner_id = message ["partnerId"].asString();
            std::string sync = decryptor->decryptFromHex (message ["syncTime"].asString());
            sync_time = std::stoi (sync.substr (4));
        }

        inline const std::string &getPartnerAuthToken() {
            return authorization_token;
        }
        inline const std::string &getPartnerId() {
            return partner_id;
        }
        inline const time_t &getSyncTime() {
            return sync_time;
        }
    };

    /// Pandora login request
    class AuthorizationRequest : public Request {
        mutable std::string authorization_token;
        mutable std::string listener_id;
        mutable std::string listening_timeout;
        mutable UserFeatures features;

    public:
        AuthorizationRequest (const std::string &username,
                              const std::string &password,
                              const std::string &partner_token)
        : Request (nullptr, "auth.userLogin", Option::TLS) {
            request_message = Parsnip::Data::make_dictionary ({ { "loginType", "user" },
                                                                { "username", username },
                                                                { "password", password },
                                                                { "partnerAuthToken", partner_token },
                                                                { "includePandoraOneInfo", true },
                                                                { "includeDailySkipLimit", true },
                                                                { "returnIsSubscriber", true } });
        }

        virtual void extractResponse (const Parsnip::Data &message) override {
            authorization_token = message ["userAuthToken"].asString();
            listener_id = message ["userId"].asString();

            if (message.contains ("listeningTimeoutMinutes"))
                features.inactivity_timeout = message ["listeningTimeoutMinutes"].makeFlexible().asInteger() * 60;
            else
                features.inactivity_timeout = 86400;
            features.daily_skip_limit = message ["dailySkipLimit"].asInteger();
            features.station_skip_limit = message ["stationHourlySkipLimit"].asInteger();
            features.max_stations = message ["maxStationsAllowed"].asInteger();
            features.adverts = message ["hasAudioAds"].asBoolean();
            features.replays = false;
            features.is_subscriber = message ["isSubscriber"].asBoolean();
            features.hifi_audio_encoding = features.is_subscriber;
        }
        inline const std::string &getUserAuthToken() {
            return authorization_token;
        }
        inline const std::string &getListenerId() {
            return listener_id;
        }

        inline const UserFeatures &getFeatures() {
            return features;
        }
    };

    /// Construct a new communicator given the user's name and password, and an optional proxy server.
    Communication::Communication (const std::string &name,
                                  const std::string &pass,
                                  const std::string &prox,
                                  const JSONProtocolParameters &proto_params)
    : protocol (proto_params),
      automatic_protocol_parameters (protocol.nature == ProtocolNature::AUTOMATIC),
      username (name),
      password (pass),
      proxy (prox),
      encryptor (proto_params.encryption_key),
      decryptor (proto_params.decryption_key) {
    }

    /** Perform an API request.
        @param request The request to perform.
        @return Status::Ok or an error value.
        @throws Exceptions thrown by message decoders. */
    Status Communication::performRequest (Request &request) {
        try {
            HttpClient::Request req;
            req.type = HttpClient::RequestType::Post;
            const bool relay = protocol.rpc_host == "tuner.pandora.com" && protocol.partner == "android";
            const bool login = request.url() == "auth.userLogin";
            req.URL = relay ? "https://spacegho.st/melodica/api/" :
                              (request.tlsEncrypt() ? "https://" : "http://") + protocol.rpc_host + "/services/json/";
            if (relay && login) melodica_access.clear();
            else if (relay && !melodica_access.empty()) req.headers["x-melodica-access"] = melodica_access;
            req.proxy = proxy;
            req.debug = !relay && request.debug();
            req.timeout = 10;

            req.user_agent = PACKAGE_NAME "-" PACKAGE_VERSION;
            req.headers ["Content-Type"] = "text/plain";
            req.parameters ["method"] = request.url();
            // It would be proper to make a copy of the request message
            // and modify that, but copies become expensive.
            auto &request_message = request.retrieveRequestMessage();
            if (!user_auth_token.empty()) {
                req.parameters ["auth_token"] = user_auth_token;
                req.parameters ["user_id"] = listener_id;
                request_message ["userAuthToken"] = user_auth_token;
            } else if (!partner_auth_token.empty()) {
                req.parameters ["auth_token"] = partner_auth_token;
            }
            if (!partner_id.empty()) {
                req.parameters ["partner_id"] = partner_id;
                request_message ["syncTime"] = synctime_offset + time (NULL);
            }
            std::string json = request_message.toJson();
            req.body = request.blowfishEncrypt() ? encryptor.encryptToHex (json) : json;

            // Set up for some logging controls
            bool detail = (logging_enabled (Log::PANDORA) && logging_enabled (Log::PROTOCOL)) || req.debug;
            LogType PANDORA_HTTP = LogType (detail ? (Log::PANDORA | Log::PROTOCOL) : Log::ERROR);

            flog (LOG_WHERE (PANDORA_HTTP), "Pandora transaction to ", request.url());
            if (detail && !relay) {
                request_message.dumpJson ("Request");
            }

            try {
                const HttpClient::Response response = http_client.performHttpRequest (req);
                if (response.http_status < 100 || response.http_status >= 300) {
                    flog (LOG_WHERE (Log::ERROR), "Failed HTTP request: ", request.url());
                    if (!relay) req.dump();
                    if (!relay) response.dump();
                    return Status (response.http_status);
                }
                if (relay && login) {
                    const auto access = response.headers.find("x-melodica-access");
                    if (access != response.headers.end()) melodica_access = access->second;
                }
                Parsnip::Data response_message;
                try {
                    if (response.body [0] == '{') {
                        response_message = Parsnip::parse_json (response.body);
                    } else {
                        response_message = Parsnip::parse_json (decryptor.decryptFromHex (response.body));
                    }
                    if (detail && !relay) {
                        response_message.dumpJson ("Response");
                    }
                    if (response_message ["stat"] == "ok") {
                        request.extractResponse (response_message ["result"]);
                        return Status::Ok;
                    }
                    Status code = Status (response_message ["code"].asInteger());
                    if (code == Status::Ok) {
                        code = Status::PandoraError0;
                    }
                    std::string message = response_message.getOr ("message", "An unexpected error occurred");
                    if (message == "An unexpected error occurred") {
                        message = status_strerror (code);
                    }
                    flog (LOG_WHERE (Log::ERROR), "Request failed: ", message);
                    if (!detail && !relay) {
                        request_message.dumpJson ("Request");
                        response_message.dumpJson ("Response");
                    }
                    if (!relay) req.dump();
                    return code;
                } catch (const Parsnip::Exception &err) {
                    flog (LOG_WHERE (Log::ERROR), "Unexpected HTTP response: ", err.what());
                    if (!detail && !relay) {
                        request_message.dumpJson ("Request");
                        response_message.dumpJson ("Response");
                    }
                    return Status::MessageFormatUnknown;
                } catch (const std::invalid_argument &err) {
                    flog (LOG_WHERE (Log::ERROR), "Message not in expected form: ", err.what());
                    if (!relay) response.dump();
                    return Status::CommunicationError;
                }
            } catch (const HttpClient::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), ex.what());
                if (!relay) req.dump();
                if (!detail && !relay) {
                    request_message.dumpJson ("Request");
                }
                return Status::CommunicationError;
            }
        } catch (const std::bad_alloc &err) {
            flog (LOG_WHERE (Log::ERROR), "Allocation error");
            return Status::AllocationError;
        }
        assert (!"Unreachable");
    }

    /// Authenticate with Pandora.
    Status Communication::partnerAuthenticate() {
        if (automatic_protocol_parameters) {
            protocol = features.is_subscriber ? JSONProtocolParameters::PandoraPlus : JSONProtocolParameters::Standard;
            encryptor = BlowFish (protocol.encryption_key);
            decryptor = BlowFish (protocol.decryption_key);
        }
        PartnerLoginRequest request (protocol, &decryptor);
        Status status = performRequest (request);
        if (status == Status::Ok) {
            partner_auth_token = request.getPartnerAuthToken();
            partner_id = request.getPartnerId();
            synctime_offset = request.getSyncTime() - time (NULL);
        }
        return status;
    }

    /// Authenticate with Pandora.
    Status Communication::authenticate() {
        AuthorizationRequest request (username, password, partner_auth_token);
        Status status = performRequest (request);
        if (status == Status::Ok) {
            user_auth_token = request.getUserAuthToken();
            features = request.getFeatures();
        }
        return status;
    }

    void Communication::resetState() {
        if (state == State::Authenticated) {
            state = State::Initialized;
        }
        partner_auth_token.clear();
        partner_id.clear();
        user_auth_token.clear();
        listener_id.clear();
    }

    /** Execute an HTTP request.  Acquire CSRF token and authenticate if necessary.
        - If an error indicates authentication has expired, log in again and retry request.
        - For other errors, back off for a period to prevent overloading Pandora servers.
        @param request The request to perform.
        @internal @param retry_if_auth_required Internal use (for managing recursion).
        @return Status::Ok, or an error. */
    Status Communication::execute (Request &request, bool retry_if_auth_required) {
        time_t now = time (nullptr);
        if (lockout_until && (lockout_until > now)) {
            return Status::TooFrequentErrors;
        }
        if (state == State::Authenticated && now >= session_expiration) {
            resetState();
        }
        Status stat;
        try {
            switch (state) {
                case State::Uninitialized:
                case State::Initialized:
                    if (partner_auth_token.empty()) {
                        stat = partnerAuthenticate();
                        if (stat != Status::Ok) {
                            flog (LOG_WHERE (Log::PANDORA), "Pandora partner authenticiation failed.");
                            break;
                        }
                        state = State::Initialized;
                    }

                    stat = authenticate();
                    if (stat != Status::Ok) {
                        flog (LOG_WHERE (Log::PANDORA), "Pandora authentication failed.");
                        if (stat == Status::InvalidPandoraCredentials) {
                            state = State::Failed;
                        }
                        break;
                    }
                    state = State::Authenticated;

                    if (automatic_protocol_parameters
                        && (features.is_subscriber != (protocol.nature == ProtocolNature::PANDORAPLUS))) {
                        flog (LOG_WHERE (Log::PANDORA), "Switching server settings based on subscriberness");
                        resetState();
                        return execute (request, false);
                    }
                    // Fall through
                case State::Authenticated:
                    stat = performRequest (request);
                    if (stat == Status::Unauthorized || stat == Status::InvalidAuthToken) {
                        resetState();
                        if (retry_if_auth_required) {
                            return execute (request, false);
                        }
                    }
                    break;
                case State::Failed:
                    lockout_until = FAR_FUTURE;
                    return Status::InvalidPandoraCredentials;
            }
        } catch (const HttpClient::Exception &ex) {
            flog (Log::ERROR, "HttpClient (", request.url(), "): ", ex.what());
            stat = Status::CommunicationError;
        }
        if (stat != Status::Ok) {
            if (stat == Status::PartnerNotAuthorized) {
                resetState();
            }
            // Gradually back off if we start getting errors.
            if (sequential_failures < 8) {
                sequential_failures++;
            }
            if (sequential_failures > 2) {
                int duration = (1 << (sequential_failures - 1));
                flog (LOG_WHERE (Log::ERROR),
                      "Multiple successive failures, disabling communication for ",
                      duration,
                      " seconds");
                lockout_until = time (nullptr) + duration;
            }
        } else {
            sequential_failures = 0;
            lockout_until = 0;
            session_expiration = now + features.inactivity_timeout;
        }
        return stat;
    }

    namespace Key {
        static const char *IsSubscriber = "isSubscriber";
    }

    /** Persist communication settings.
        @return A dictionary with any settings the communicator wants saved. */
    Parsnip::Data Communication::persist() const {
        return Parsnip::Data::make_dictionary ({ { Key::IsSubscriber, features.is_subscriber } });
    }

    /** Restore communication settings.
        @param data A dictionary of previous settings from which to restore. */
    void Communication::restore (const Parsnip::Data &data) {
        try {
            features.is_subscriber = data.getOr (Key::IsSubscriber, false);
        } catch (std::exception &ex) {
            flog (LOG_WHERE (Log::ERROR), ex.what());
        }
    }

}  // namespace Pandora

///
/// Pandora connection parameters.
/// @file       pandoraparameters.h - pianod2
/// @author     Perette Barella
/// @date       2015-03-17
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <string>

#include "mediaparameters.h"

namespace Pandora {
    enum class ProtocolNature { AUTOMATIC, STANDARD, PANDORAPLUS, PANDORAPREMIUM, CUSTOM };

   struct JSONProtocolParameters {
        // JSON protocol parameters
        ProtocolNature nature{ ProtocolNature::AUTOMATIC };
        std::string rpc_host;
        std::string partner;
        std::string partner_password;
        std::string device;
        std::string encryption_key;
        std::string decryption_key;
        
        static const JSONProtocolParameters Standard;
        static const JSONProtocolParameters PandoraPlus;
    };

    /// Pandora connection and source settings
    struct ConnectionParameters : public Media::SourceParameters {
        std::string proxy;
        std::string control_proxy;
        std::string username;
        std::string password;
        int pause_timeout{ 1800 };
        int playlist_expiration{ 3600 };
        int cache_minimum{ 4000 };
        int cache_maximum{ 5000 };

        JSONProtocolParameters protocol;

        ConnectionParameters();
        ConnectionParameters (const ConnectionParameters &params) = default;
        ConnectionParameters (ConnectionParameters &&params) = default;
        ConnectionParameters (const UserData::JSONData &src);
        virtual bool persist (UserData::JSONData &dest) const override;
        static Parsnip::OptionParser::Definitions parser_definitions();
        void extractOptions (const Parsnip::Data &options);
    private:
        void extract (const Parsnip::Data &options);
    };

}  // namespace Pandora

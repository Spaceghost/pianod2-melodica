///
/// Functions that deal with all the possible sources.
/// @file       sources.cpp - pianod2
/// @author     Perette Barella
/// @date       2016-03-22
/// @copyright  Copyright (c) 2016-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <iostream>
#include <string>

#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "utility.h"
#include "interpreter.h"
#include "datastore.h"
#include "connection.h"
#include "users.h"

#include "sources.h"

#ifdef WITH_PANDORA
#include <curl/curl.h>
#include "pandora/pandoracommand.h"
#endif
#ifdef WITH_TONEGENERATOR
#include "tonegenerator/tonegencommand.h"
#endif
#ifdef WITH_FILESYSTEM
#include "filesystem/filesystemcommand.h"
#endif

namespace SourceName {
    const char *Manager = "manager";

#ifdef WITH_PANDORA
    const char *Pandora = "pandora";
#endif

#ifdef WITH_TONEGENERATOR
    const char *ToneGenerator = "tonegenerator";
#endif

#ifdef WITH_FILESYSTEM
    const char *FileSystem = "filesystem";
#endif
}


/// Interface between the general pianod code and the individual sources,
/// which may or may not be present in any particular build.
namespace Sources {
   
#ifdef WITH_PANDORA
    PandoraCommands pandora_interpreter;
#endif
#ifdef WITH_TONEGENERATOR
    ToneGeneratorCommands tonegenerator_interpreter;
#endif
#ifdef WITH_FILESYSTEM
    FilesystemCommands filesystem_interpreter;
#endif


    /** Get a list of source type names.
        @return Vector with source names (except manager). */
    const std::vector<std::string> &sourceKindNames () {
        static std::vector <std::string> names;
        if (names.empty()) {
#ifdef WITH_PANDORA
            names.push_back (SourceName::Pandora);
#endif
#ifdef WITH_TONEGENERATOR
            names.push_back (SourceName::ToneGenerator);
#endif
#ifdef WITH_FILESYSTEM
            names.push_back (SourceName::FileSystem);
#endif
        }
        return names;
    }


    /** Check if a name is a source type name.
        @param name The name to check.
        @return True if name is a source type name other than the manager. */
    bool isSource (const std::string &name) {
        for (const auto &source : sourceKindNames()) {
            if (strcasecmp (name, source) == 0) {
                return true;
            }
        }
        return (false);
    };

    /** Register source parser definitions with the master parser.
        @param parser The master parser. */
    void registerCommands (PianodParser *parser) {
#ifdef WITH_PANDORA
        parser->addOptionParser (KEY_PANDORA_OPTIONS, retrieve_pandora_option_definitions ());
        parser->addStatements (PandoraCommands::parser_definitions());
#endif
#ifdef WITH_TONEGENERATOR
        parser->addOptionParser (KEY_TONEGENERATOR_OPTIONS, retrieve_tonegenerator_option_definitions ());
        parser->addStatements (ToneGeneratorCommands::parser_definitions ());
#endif
#ifdef WITH_FILESYSTEM
        parser->addStatements (FilesystemCommands::parser_definitions ());
#endif
    }
    
    void registerInterpreters (PianodDispatcher &dispatcher) {
#ifdef WITH_PANDORA
        pandora_interpreter.registerInterpreter (dispatcher);
#endif
#ifdef WITH_TONEGENERATOR
        tonegenerator_interpreter.registerInterpreter (dispatcher);
#endif
#ifdef WITH_FILESYSTEM
        filesystem_interpreter.registerInterpreter (dispatcher);
#endif
    }

    void addRequestNames (PianodSchema &schema) {
#ifdef WITH_PANDORA
        schema.addRequestNames (PandoraCommands::json_request_names());
#endif
#ifdef WITH_TONEGENERATOR
        schema.addRequestNames (ToneGeneratorCommands::json_request_names());
#endif
#ifdef WITH_FILESYSTEM
        schema.addRequestNames (FilesystemCommands::json_request_names());
#endif
    }

    /** Restore a source.
        @param source_data The parameters for the source.
        @param owner The user to whom the source belongs. */
    void restore (UserData::DataStore *source_data, User *owner) {
        const std::string &source_name = source_data->origin();
        
        auto *persisted = dynamic_cast<UserData::JSONData *> (source_data);
        assert (persisted);

#ifdef WITH_PANDORA
        if (strcasecmp (source_name, SourceName::Pandora) == 0)
            restore_pandora_source (persisted, owner);
#endif
#ifdef WITH_TONEGENERATOR
        if (strcasecmp (source_name, SourceName::ToneGenerator) == 0)
            restore_tonegenerator_source (persisted, owner);
#endif
#ifdef WITH_FILESYSTEM
        if (strcasecmp (source_name, SourceName::FileSystem) == 0)
            restore_filesystem_source (persisted, owner);
#endif
    }

    /** Report sources, libraries they use and their versions.
        The currently unused parameter represents a verbosity level. */
    void report (int) {
        std::cerr << "  Sources:\n";
#ifdef WITH_PANDORA
        std::cerr << "    pandora\n";
        {
            const curl_version_info_data *version = curl_version_info (CURLVERSION_NOW);
            std::cerr << "      curl compile version " << LIBCURL_VERSION
            << "\n      curl runtime version " << version->version << '\n';
        }
#endif
#ifdef WITH_TONEGENERATOR
        std::cerr << "    tonegenerator\n";
#endif
#ifdef WITH_FILESYSTEM
        std::cerr << "    filesystem\n";
#endif
    }
}

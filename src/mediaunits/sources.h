///
/// Functions that deal with all the possible sources.
/// @file       sources.h - pianod2
/// @author     Perette Barella
/// @date       2016-03-22
/// @copyright  Copyright (c) 2016-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <vector>

#include <parsnip/parsnip_command.h>

#include "interpreter.h"

namespace UserData {
    class JSONData;
    class DataStore;
}
class User;

namespace UserData {
    class DataStore;
}

namespace SourceName {
    extern const char *Manager;

#ifdef WITH_PANDORA
    extern const char *Pandora;
#endif

#ifdef WITH_TONEGENERATOR
    extern const char *ToneGenerator;
#endif

#ifdef WITH_FILESYSTEM
    extern const char *FileSystem;
#endif
}

namespace Sources {
    const std::vector<std::string> &sourceKindNames ();
    bool isSource (const std::string &origin);
    void registerCommands (PianodParser *parser);
    void registerInterpreters (PianodDispatcher &dispatcher);
    void addRequestNames (PianodSchema &schema);
    void restore (UserData::DataStore *source_data, User *owner);
    void report (int verbose);
}


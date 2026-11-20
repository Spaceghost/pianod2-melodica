///
/// Read/write JSON documents.
/// @file       fileio.h - pianod2
/// @author     Perette Barella
/// @date       2015-02-23
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <parsnip/parsnip.h>

#include "fileio.h"

// Functions for reading/writing files

const std::string &setupConfigDirectory (const char *location,
                                         const char *package,
                                         const char *nobody_name,
                                         const char *nobody_groups_list);
bool carefullyWriteFile (std::string filename, Parsnip::Data &data);

const Parsnip::Data retrieveJsonFile (std::string filename, int minimumVersion = 0);

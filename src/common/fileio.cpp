///
/// Read/write documents and do filesystem interaction.
/// @file       fileio.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-02-23
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include <iostream>
#include <fstream>
#include <stdexcept>

#include <parsnip/parsnip.h>

#ifdef HAVE_COMPRESSION
#include "gzstream.h"
#endif

#include "logging.h"

// Default sysconfdir default; overrides by defining at compile time.
// This is were all the config files will go when run as root.
#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

static const char *FileCreator = "author";
static const char *FileCreatorVersion = "version";

static std::string config_directory;
static bool have_nobody = false;
static struct passwd nobody;
static gid_t *nobody_groups;
static int nobody_groups_count;

// Determine if we're running as root.
static bool running_as_root (void) {
    return (geteuid() == 0);
}

/* When running as root, gather the user and group IDs pianod will run as.
   @return home directory of chosen user, unless chosen user is root, then nullptr. */
static char *select_nobody_user (const char *nobody_name, const char *group_names) {
    size_t nobody_groups_size = 0;
    assert (nobody_name);
    assert (running_as_root());

    // Gather the primary user & group IDs.
    struct passwd *user = getpwnam (nobody_name);
    if (!user) {
        flog (Log::ERROR, "user '", nobody_name, "' not found when invoking pianod as root.");
        flog (Log::ERROR, "Use -n <username> to indicate user to run as.");
        exit (1);
    }
    nobody = *user;
    have_nobody = true;
    char *home = nullptr;
    if (user->pw_uid) {
        home = strdup (user->pw_dir);
        if (!home) {
            perror ("select_nobody_user: setenv");
            exit (1);
        }
    }

    endpwent();

    // Gather supplementary groups we will be running as
    if (group_names) {
        // Set a user-provided list of supplementary groups
        char *groups = strdup (group_names);
        if (!groups) {
            perror ("select_nobody_user: strdup");
            exit (1);
        }
        // Count the groups and allocate memory for the list
        char *group = strtok (groups, ",");
        while (group) {
            nobody_groups_size++;
            group = strtok (NULL, ",");
        }
        nobody_groups = (gid_t *) calloc (nobody_groups_size, sizeof (*nobody_groups));
        if (!nobody_groups) {
            perror ("select_nobody_user: calloc");
            free (groups);
            exit (1);
        }
        // Gather the group list
        strcpy (groups, group_names);
        group = strtok (groups, ",");
        while (group) {
            struct group *group_info;
            if ((group_info = getgrnam (group))) {
                nobody_groups[nobody_groups_count++] = group_info->gr_gid;
            } else {
                flog (LOG_WHERE (Log::WARNING), group, ": getgrnam: ", strerror (errno));
            }
            group = strtok (NULL, ",");
        }
        free (groups);
    } else {
        // Use supplementary groups from /etc/groups
        do {
            nobody_groups_size = nobody_groups_size * 2 + 10;
            nobody_groups = (gid_t *) realloc (nobody_groups,
                                               nobody_groups_size * sizeof (*nobody_groups));
            if (!nobody_groups) {
                flog (LOG_FUNCTION (Log::ERROR), "realloc", strerror (errno));
                exit (1);
            }
            // Stupid apple/BSDism: nobody.pw_gid should be gid_t but is int.
            // Other varieties get this right.
        } while (getgrouplist (nobody_name,
                               nobody.pw_gid,
#if !defined(__FreeBSD__) && !defined(__APPLE__)
                               nobody_groups,
#else
                               (int *) nobody_groups,
#endif
                               &nobody_groups_count)
                 < 0);
    }
    return home;
}

// Drop root privileges by setting the effective user to the real user
static void drop_root_privs (void) {
    assert (running_as_root());
    if (setgid (nobody.pw_gid) < 0) {
        flog (LOG_FUNCTION (Log::ERROR), "setgid: ", strerror (errno));
        exit (1);
    }
    if (setgroups (nobody_groups_count, nobody_groups) < 0) {
        flog (LOG_FUNCTION (Log::ERROR), "setgroups: ", strerror (errno));
        exit (1);
    }
    if (setuid (nobody.pw_uid) < 0) {
        flog (LOG_FUNCTION (Log::ERROR), "setuid: ", strerror (errno));
        exit (1);
    }
}

/** Ensure the pianod configuration directory is owned by the user
    we become when dropping root privileges. */
static void precreateDirectory (const std::string &directory, bool own) {
    if (directory.empty())
        return;

    struct stat fileinfo;
    bool exists = (stat (directory.c_str(), &fileinfo) >= 0);
    bool mine = false;
    if (exists) {
        mine = fileinfo.st_uid == nobody.pw_gid;
    } else {
        // Make sure the parent directory exists, too.
        std::string parent = directory;
        parent.erase (parent.length() - 1);
        while (!parent.empty() && parent[parent.length() - 1] != '/')
            parent.erase (parent.length() - 1);
        // Make sure parent directories exist, but don't take ownership of them.
        precreateDirectory (parent, false);
        if (mkdir (directory.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            perror (directory.c_str());
            exit (1);
        }
    }
    // Reassign file ownership to the user pianod will run as
    if (!mine && own) {
        if (chmod (directory.c_str(), S_IRWXU) < 0) {
            perror (directory.c_str());
        }
        if (chown (directory.c_str(), nobody.pw_uid, nobody.pw_gid) < 0) {
            perror (directory.c_str());
        }
    }
}

/** Determine the configuration directory in accordance with the Free
    Desktop Base Directory Specification.
    @see http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html
    @param location Put config files in this directory if specified.
    @param home Use this directory instead of $HOME if specified.
    @param package This package's name.
    @return Directory with package name and a trailing '/' appended. */
static std::string getConfigDirectory (const char *location, const char *home, const char *package) {
    char *xdg_config_dir = getenv ("XDG_CONFIG_HOME");
    std::string dir;

    if (location && strlen (location) > 0) {
        dir = location;
    } else if (xdg_config_dir && strlen (xdg_config_dir) > 0) {
        dir = xdg_config_dir;
    } else if (home && strlen (home) > 0) {
        dir = std::string (home) + "/.config";
    } else if (running_as_root()) {
        dir = SYSCONFDIR;
    } else if ((xdg_config_dir = getenv ("HOME")) && strlen (xdg_config_dir) > 0) {
        // Standard config dir: $HOME/.config
        dir = std::string (xdg_config_dir) + "/.config";
    } else {
        // fallback: use working directory
        dir = ".";
    }
    // Append package name and fix any double-slashes
    dir = dir + "/" + package + "/";
    auto fix = dir.find ("//");
    while (fix != std::string::npos) {
        dir.erase (fix, 1);
        fix = dir.find ("//");
    }
    return dir;
}

/** Choose and configure a directory for configuration files.
    @param location Pathname to use, or nullptr to use default.
    @param package Name of this package.
    @param nobody_name Name of user to run as, if running as root.
    @param nobody_groups_list List of groups to adopt before giving up root privs.
    @return Path of the chosen configuration directory, which will exist and be
    read/write accessible. */
const std::string &setupConfigDirectory (const char *location,
                                    const char *package,
                                    const char *nobody_name,
                                    const char *nobody_groups_list) {
    char *nobody_home = nullptr;
    if (running_as_root()) {
        nobody_home = select_nobody_user (nobody_name, nobody_groups_list);
    }
    config_directory = getConfigDirectory (location, nobody_home, package);
    free (nobody_home);
    precreateDirectory (config_directory, running_as_root());
    if (running_as_root())
        drop_root_privs();
    return config_directory;
}

/** Write a document into a file.  Write into a temporary file, and when complete,
    then rename files so as to keep as backup while moving the new file into place.
    If compression is supported, writes compressed file; otherwise, writes
    uncompressed JSON.  This is set at compile time, with no run-time choice.
    @param filename The filename, without path.
    @param document The document to write.  Top level must be a document.
    @warning `document` parameter is altered to contain authoring package
    identification. */
bool carefullyWriteFile (std::string filename, Parsnip::Data &document) {
    assert (!filename.empty());
    assert (!config_directory.empty());
    assert (!document.contains (FileCreator));
    assert (!document.contains (FileCreatorVersion));
    document[FileCreator] = PACKAGE_NAME;
    document[FileCreatorVersion] = std::stoi (PACKAGE_VERSION);
    filename = config_directory + filename;
#ifdef HAVE_COMPRESSION
    filename += ".gz";
#endif
    std::string newfile = filename + "-new";
    std::string oldfile = filename + "-old";
    {
#ifdef HAVE_COMPRESSION
        ogzstream jsonfile;  // gzstream is dodgy, open from constructor no worky.
        jsonfile.open (newfile.c_str());
#else
        std::ofstream jsonfile (newfile);
#endif
        if (!jsonfile.good()) {
            flog (LOG_WHERE (Log::ERROR), "Could not open file for write: ", filename);
            return false;
        }
#ifdef NDEBUG
        jsonfile << document << std::endl;
#else
        document.toJson (jsonfile, 0);
#endif
        if (jsonfile.fail()) {
            flog (LOG_WHERE (Log::ERROR), "Could not write file: ", filename);
            jsonfile.close();
            unlink (newfile.c_str());
            return false;
        }
    }

    unlink (oldfile.c_str());
    (void) link (filename.c_str(), oldfile.c_str());
    bool status = (rename (newfile.c_str(), filename.c_str()) == 0);
    if (!status) {
        flog (LOG_WHERE (Log::ERROR), "Could not rename file into place: ", strerror (errno));
    }
    return status;
}

/** Restore from a file.  Tries .gz (if supported) first, then
    falls back to uncompressed version.
    @param filename The filename, without path.
    @param minimum_version The lowest file format version number supported.  If
    the input file has a version number lower than this, a DataFormatException
    is thrown.
    @return A parsed JSON document.
    @throw bad_alloc, io_base::failure, or a Parsnip serialization exception. */
const Parsnip::Data retrieveJsonFile (std::string filename, int minimum_version) {
    assert (!filename.empty());
    assert (!config_directory.empty());
    filename = config_directory + filename;

    std::ifstream jsonfile;
    std::istream *source = nullptr;
#ifdef HAVE_COMPRESSION
    igzstream gzjsonfile;  // gzstream is dodgy, open from constructor no worky.
    gzjsonfile.open ((filename + ".gz").c_str());
    if (gzjsonfile.good())
        source = &gzjsonfile;
#endif
    if (source == nullptr) {
        jsonfile.open (filename.c_str());
        if (jsonfile.good()) {
            source = &jsonfile;
        } else {
            flog (LOG_WHERE (Log::WARNING),
                  filename,
                  ": Cannot read file (probably does not exist).");
            throw std::ios_base::failure (filename + ": Cannot read file");
        }
    }
    Parsnip::Data data = Parsnip::parse_json (*source, true);
    try {
        std::string creator = data[FileCreator].asString();
        if (creator != PACKAGE_NAME) {
            throw Parsnip::DataFormatError (creator + " file is not a " PACKAGE_NAME " file.");
        }
        long version = data[FileCreatorVersion].asLong();
        if (version < minimum_version) {
            throw Parsnip::DataFormatError ("File version unsupported: " + std::to_string (version)
                                            + ", need " + std::to_string (minimum_version));
        }
    } catch (const Parsnip::NoSuchKey &ex) {
        throw Parsnip::DataFormatError (std::string{
            "Not a " PACKAGE_NAME " file: "} + ex.what());
    }
    return data;
    }

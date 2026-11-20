///
/// Multiuser multisource network music player.
/// @file       pianod.cpp - Initialization, shutdown, and main().
/// @author     Perette Barella
/// @date       2012-03-10
/// @copyright  Copyright 2012–2023 Devious Fish. All rights reserved.
///

#ifndef __FreeBSD__
#define _DARWIN_C_SOURCE  // setprogname() on OS X
#endif

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <cassert>

#include <iostream>

#include <signal.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>

#include <exception>

#include <football/fb_public.h>

#include "conductor.h"
#include "sources.h"
#include "fileio.h"

#ifdef WITH_TAGLIB
#include <taglib/taglib.h>
#endif

#ifndef HAVE_SETPROGNAME
static const char *progname;
void setprogname (const char *name) {
    const char *last = strrchr (name, '/');
    if (last) {
        progname = last + 1;
    } else {
        progname = name;
    }
}
const char *getprogname (void) {
    return progname;
}
#endif

/// Display command line usage and explanation of options.
static void usage() {
    std::cerr << "Usage: " << getprogname()
              << " [-v] [-S] [-n user] [-g groups] [-d directory] [-c clientdir]\n"
                 "              [-p port] [-P httpport] [-s httpsport]\n"
                 "  -?            : Display usage and exit\n"
                 "  -v            : Display version and exit (repeat for more detail).\n"
                 "  -n user       : the user pianod should change to when run as root\n"
                 "  -g groups     : supplementary groups pianod should use when run as root\n"
                 "  -p port       : the line-oriented port (default 4445); 0 to disable\n"
                 "  -P httpport   : the HTTP/greeted port (default 4446 or -p+1; 0 to disable)\n"
                 "  -s httpsport  : the HTTP Secure port (default 4447 or httpport+1; 0 to disable)\n"
                 "  -d directory  : the directory for " PACKAGE
                 " configuration files\n"
#ifdef SHADOW_CAPABLE
                 "  -S            : shadow system users\n"
#endif
                 "  -T            : suppress time in logged messages.\n"
                 "  -c clientdir  : a directory with web client files be served\n"
                 "                  (default " DEFAULT_WWWDIR
                 ")\n"
#ifndef NDEBUG
                 "  -z footflags  : set football logging flags\n"
                 "  -Z flags      : set pianod logging flags\n"
#endif
            ;
}

/// Display package version number and build options.
static void report_version (int verbose) {
    std::cerr << PACKAGE " version " PACKAGE_VERSION "\n";
    if (verbose) {
        std::cerr << "Build options:\n  Switches:"
#ifdef NDEBUG
                " --disable-debug"
#else
                " --enable-debug"
#endif
#ifdef HAVE_COMPRESSION
                " --with-compression"
#else
                " --without-compression"
#endif
#ifdef WITH_ACCESSCONTROL
                " --with-accesscontrol"
#else
                " --without-accesscontrol"
#endif
                "\n  SSL package: "
#ifdef HAVE_LIBGNUTLS
                "gnutls"
#elif defined(HAVE_LIBMBEDTLS)
                "mbed TLS"
#elif defined(HAVE_LIBSSL)
                "OpenSSL"
#elif defined(HAVE_SECURETRANSPORT)
                "SecureTransport"
#elif defined(HAVE_LIBTLS)
                "LibreSSL"
#else
                "None"
#endif
                "\n";
        Sources::report (verbose - 1);
        Media::reportLibrariesAndVersions (verbose - 1);
#ifdef WITH_TAGLIB
        std::cerr << "    taglib\n"
             << "      compile version " << TAGLIB_MAJOR_VERSION << '.' << TAGLIB_MINOR_VERSION << '.'
             << TAGLIB_PATCH_VERSION << '\n';
#endif
        Orchestra::reportLibrariesAndVersions (verbose - 1);
    }
}

static Orchestra::Conductor *conductor = nullptr;
static void run (void) {
    conductor->conduct();
}

/* Shutdown signal handler, which sets global variable to be read
   by the main run loop. (Had OS X issues with these being static...) */
void receive_signal (int signum) {
    if (conductor)
        conductor->shutdown();
    fb_interrupt();
    signal (signum, SIG_IGN);
}

int main (int argc, char **argv) {
    std::ios::sync_with_stdio (false);
    const char *nobody_name = "nobody";
    char *config_dir_option = nullptr;
    char *nobody_groups_list = nullptr;
    FB_SERVICE_OPTIONS options = {};
    AudioSettings audio = {};

    // Perturb the random number generator
#ifdef HAVE_SRANDOMDEV
    srandomdev();
#else
    srandom (time (NULL) ^ (getpid() << 16));
#endif

    options.line_port = 4445;
    options.http_port = 4446;
    options.https_port = 4447;
    options.greeting_timeout = 20; // 20 seconds to TLS negotiate & greet.
    options.serve_directory = (char *) DEFAULT_WWWDIR;
    options.internationalization = FB_INTERNATIONALIZATION_EXTENSION;
    int flag;
    int version_verbosity = 0;

    setprogname (*argv);

    while ((flag = getopt (argc, argv, "hvn:g:p:P:s:c:d:Z:z:ST")) > 0) {
        int argval;
        switch (flag) {
            case 'c':
                options.serve_directory = optarg;
                break;
            case 'p':
                argval = atoi (optarg);
                if (argval != 0) {
                    if (options.http_port == options.line_port + 1) {
                        options.http_port = argval + 1;
                    }
                    if (options.https_port == options.line_port + 2) {
                        options.https_port = argval + 2;
                    }
                }
                options.line_port = argval;
                break;
            case 'P':
                argval = atoi (optarg);
                if (argval != 0 && options.https_port == options.http_port + 1) {
                    options.https_port = ((argval == 80) ? 443 : argval + 1);
                }
                options.http_port = argval;
                break;
            case 's':
                options.https_port = atoi (optarg);
                break;
            case 'd':
                config_dir_option = optarg;
                break;
            case 'n':
                nobody_name = optarg;
                break;
            case 'g':
                nobody_groups_list = optarg;
                break;
            case 'S':
#ifdef SHADOW_CAPABLE
                User::shadow (true);
#else
                cerr << getprogname()
                     << ": Configured without shadow password support.\nRecompile or remove -S option.";
                exit (1);
#endif
                break;
            case 'T':
                flog_include_timestamp = false;
                break;
            case 'Z':
                set_logging (static_cast <LogType> (strtol (optarg, NULL, 0)));
                break;
            case 'z':
                fb_set_logging (strtol (optarg, NULL, 0), NULL);
                break;
            case 'v':
                version_verbosity++;
                break;
            case '?':
            case 'h':  // Undocumented alternative
                usage();
                exit (1);
            default:
                assert (!"Unhandled option");
                break;
        }
    }

    if (version_verbosity) {
        report_version (version_verbosity - 1);
        exit (0);
    }

    const std::string &config_dir = setupConfigDirectory (config_dir_option, PACKAGE, nobody_name, nobody_groups_list);
    // Root privs lost here

    // In the development environment, translations are in a subdirectory.
    // This is acknowledged to be a stupid problem, and a dumb solution.
    std::string locale_path{ options.serve_directory };
    struct stat junk;
    locale_path += "/locale";
    if (stat (locale_path.c_str(), &junk) == 0) {
        options.locale_directory = (char *) "locale";
    }

    try {
        conductor = new Orchestra::Conductor (config_dir, options, audio);

        // Intercept deadly signals to handle them gracefully.
        signal (SIGHUP, receive_signal);
        signal (SIGINT, receive_signal);
        signal (SIGTERM, receive_signal);
#if !defined(HAVE_SO_NOSIGPIPE)
        signal (SIGPIPE, SIG_IGN);
#endif

#ifdef WITH_AVFOUNDATION
        wrapInNSApplication (run);
#else
        run();
#endif
        delete conductor;
        conductor = nullptr;
    } catch (const InitializationException &e) {
        flog (LOG_WHERE (Log::ERROR), "Fatal initialization problem: ", e.what());
        return 1;
    }
    return 0;
}

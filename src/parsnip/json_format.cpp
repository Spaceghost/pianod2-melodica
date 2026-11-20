///
/// JSON formatter
/// A simple JSON formatter using the Parsnip library.
/// @file       json_format.cpp - JSON formatter
/// @author     Perette Barella
/// @date       2019-05-01
/// @copyright  Copyright 2019-2020 Devious Fish. All rights reserved.
///

#include <cstdlib>

#include <unistd.h>

#include <istream>
#include <iostream>
#include <fstream>

#include "parsnip.h"

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

/** Render a parsnip exception to standard error in a standard way.
    @param filename The filename to which the exception pertains.
    @param ex The exception. */
void render_exception (const char *filename, const Parsnip::Exception &ex) {
    std::cerr << filename << ": " << ex.what() << std::endl;
    if (ex.where()) {
        std::cerr << "  Location: " << ex.where() << std::endl;
    }
}

/** Read a JSON document from a stream.  On error, exit with failure. */
Parsnip::Data read_json_file (const char *name, std::istream &src) {
    try {
        return Parsnip::parse_json (src);
    } catch (const Parsnip::Exception &ex) {
        render_exception (name, ex);
        exit (EXIT_FAILURE);
    }
}

/** Read in JSON from a file.  On error, exit with failure. */
Parsnip::Data load_json_file (const char *name) {
    std::ifstream in (name);
    if (!in.good()) {
        perror (name);
        exit (EXIT_FAILURE);
    }
    return read_json_file (name, in);
}

/** Read in JSON from a file.
    @param name The name of the file.
    @param [out] file_contents On return, set to the parsed JSON.
    @return True on success, false on failure. */
bool try_load_json_file (const char *name, Parsnip::Data &file_contents) {
    std::ifstream in (name);
    if (!in.good()) {
        perror (name);
    } else {
        try {
            file_contents = Parsnip::parse_json (in);
            return true;
        } catch (const Parsnip::Exception &ex) {
            render_exception (name, ex);
        }
    }
    return false;
}

enum class Action { TIDY, VALIDATE, EXPLAIN, FLATTEN_W_DICT, FLATTEN_W_MEMBERS };

/** Reformat a JSON file into a series of assignments,
    perhaps for use with a shell script or diff.
    @param data The data to reformat
    @param action Either FLATTEN_W_DICTIONARY or FLATTEN_W_MEMBERS.
    @param varname The "variable" name to which options/array specifiers are appended. */
void flatten (const Parsnip::Data &data, Action action, const std::string varname) {
    try {
        for (Parsnip::Data::size_type i = 0; i < data.size(); i++) {
            flatten (data [i], action, varname + "[" + std::to_string (i) + "]");
        }
    } catch (Parsnip::IncorrectDataType &) {
        try {
            std::function<void (const std::string &, const Parsnip::Data &)> handle_member{
                [action, varname] (const std::string &key, const Parsnip::Data &member) -> void {
                    if (action == Action::FLATTEN_W_DICT) {
                        flatten (member, action, varname + "[\"" + key + "\"]");
                    } else {
                        flatten (member, action, varname + "." + key);
                    }
                }
            };
            data.foreach (handle_member);
        } catch (Parsnip::IncorrectDataType &) {
            std::cout << varname << "=";
            data.toJson (std::cout) << "\n";
        }
    }
}

struct Options {
    Action action{ Action::TIDY };
    bool quiet = false;
    const char *structure_name{ nullptr };
    const char *schema_file{ nullptr };
    Parsnip::Schema schema;
};

/** Take action on a file.
    @param name The name of the file (for error reporting).
    @param data The contents of the file.
    @param options Options and data for the action to be taken. */
bool process_file (const char *name, Parsnip::Data &data, const Options &options) {
    try {
        switch (options.action) {
            case Action::TIDY:
                data.toJson (std::cout, 0) << std::endl;
                return true;
            case Action::VALIDATE:
                options.schema.validate (data);
                if (!options.quiet) {
                    std::cerr << name << ": Complies with schema.\n";
                }
                return true;
            case Action::EXPLAIN:
                options.schema.dump (options.structure_name, std::cout);
                return true;
            case Action::FLATTEN_W_DICT:
            case Action::FLATTEN_W_MEMBERS:
                flatten (data, options.action, options.structure_name);
                return true;
            default:
                std::cerr << "Unimplemented action\n";
                exit (EXIT_FAILURE);
        }
    } catch (Parsnip::Exception &ex) {
        render_exception (name, ex);
        return false;
    }
}

/// Display command-line usage statement.
void usage() {
    std::cerr << "Usage: " << getprogname()
              << "[options] [file] ...\n"
                 "Options are:\n"
                 "  -q                        -- Quiet\n"
                 "  -t                        -- Tidy (reformat) JSON.  Comments are lost.\n"
                 "  -f <dictionary|member>    -- Flatten file into variable definitions.\n"
                 "  -n varname                -- Variable name to use in flattening.\n"
                 "  -s <schema_file>          -- Validate files against a schema.\n"
                 "  -x                        -- Translate JSON schema files descriptive text.\n";
}

int main (int argc, char *const *argv) {
    setprogname (*argv);
    Options options;
    int flag;

    options.structure_name = getprogname();
    Parsnip::Schema schema;
    while ((flag = getopt (argc, argv, "f:s:n:xtq")) > 0) {
        switch (flag) {
            case 'f': {
                std::string arg = optarg;
                if (arg == "dictionary" || arg == "array") {
                    options.action = Action::FLATTEN_W_DICT;
                } else if (arg == "member" || arg == "members") {
                    options.action = Action::FLATTEN_W_MEMBERS;
                } else {
                    std::cerr << getprogname() << ": Unknown flattening style: " << arg << std::endl;
                    exit (EXIT_FAILURE);
                }
                break;
            }
            case 'n':
                options.structure_name = optarg;
                break;
            case 'q':
                options.quiet = true;
                break;
            case 't':
                options.action = Action::TIDY;
                break;
            case 's':
                options.action = Action::VALIDATE;
                options.schema_file = optarg;
                break;
            case 'x':
                options.action = Action::EXPLAIN;
                break;
            default:
                usage();
                exit (EXIT_FAILURE);
        }
    }

    if (options.schema_file) {
        try {
            options.schema = Parsnip::Schema (load_json_file (options.schema_file));
        } catch (Parsnip::Exception &ex) {
            render_exception (options.schema_file, ex);
            exit (EXIT_FAILURE);
        }
    }

    bool success = true;
    if (optind >= argc) {
        Parsnip::Data data = read_json_file ("stdin", std::cin);
        success = process_file ("stdin", data, options);
    } else {
        Parsnip::Data data;
        for (int i = optind; i < argc; i++) {
            const char *filename = argv [i];
            success = (try_load_json_file (filename, data) && process_file (filename, data, options) && success);
        }
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

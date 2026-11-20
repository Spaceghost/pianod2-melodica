///
/// Parsnip command-line parsing: argument vector.
/// @file       parsnip_argv.cpp - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-05-06
/// @copyright  Copyright 2012-2020 Devious Fish. All rights reserved.
///

#include <vector>
#include <iostream>

#include <cassert>
#include <cctype>

#include "parsnip_argv.h"

namespace Parsnip {
    
    /** Construct an argument vector by splitting up a string into tokens.
        - An empty/whitespace-filled string results in an empty vector.
        - Strings may be quoted with single (apostrophe) or double
        quotes.  The quote characters are omitted from their tokens.
        - A literal of of the quote character may be inserted
        by doubling within the quoted region.
        - A mismatched quote is not treated as an error; the token is
        taken from the character after the open quote to end of string. */
    ArgumentVector::ArgumentVector (const StringType &cmd): original (cmd) {
        StringType::size_type point {0};
        while (true) {
            while (point < cmd.size() && isspace (cmd[point])) {
                point++;
            }
            if (point >= cmd.size()) {
                break;
            }
            StringType::size_type start = point;
            StringType::value_type quote = cmd[point];
            if (quote == '\'' || quote == '\"') {
                // Quoted string
                point++;
                std::string value;
                while (point < cmd.size()) {
                    if (cmd[point] == quote) {
                        if ((point + 1) == cmd.size() || isspace (cmd[point + 1])) {
                            // Exit on end of string or end of quoted section
                            point++;
                            break;
                        }
                        if (cmd [point + 1] == quote) {
                            // There's a double quote within the string, which means one literal quote.
                            point++;
                        }
                    }
                    value += cmd[point++];
                }
                push_back (std::move (value));
            } else {
                while (point < cmd.size() && !isspace (cmd[point])) {
                    point++;
                }
                push_back (cmd.substr (start, point - start));
            }
            remainder_start.push_back (start);
        }
        assert (size() == remainder_start.size());
    }

}  // namespace Parsnip

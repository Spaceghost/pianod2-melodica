///
/// Parsnip serialization - stream I/O and parsing helper functions.
/// @file       parsnip_helpers.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2019-05-01
/// @copyright  Copyright 2019-2021 Devious Fish. All rights reserved.
///

#pragma once

namespace Parsnip {

    /// Digits used for encoding/decoding hexadecimal.
    extern const char hex_digits[];

    /// Helper function: Retrieve a hexadecimal digit.
    static inline char hexdigit (int digit) {
        assert (digit >= 0 && digit <= 15);
        return hex_digits[digit];
    }

    /** Generate some indentation.
        @param target The stream to write indentation to.
        @param amount The amount of indentation, in spaces.
        Negative numbers = none. */
    static inline void do_indent (std::ostream &target, int amount) {
        for (int i = 0; i < amount; i++) {
            target << ' ';
        }
    }

    /** Write a newline to a stream.
        @param target The stream to write the newline to.
        @param amount Amount of indent.  If negative, we're not indenting,
        so don't worry about newlines either. */
    static inline void do_newline (std::ostream &target, int amount) {
        if (amount >= 0) {
            target << std::endl;
        }
    }

}  // namespace Parsnip

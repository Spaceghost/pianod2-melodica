///
/// Parsnip command-line parsing: argument vector.
/// @file       parsnip_argv.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-05-06
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <string>
#include <vector>

namespace Parsnip {
    /** This class lexically analyzes a string, splitting it up into tokens,
        which are accessed as a vector. This involves splitting at spaces,
        or better quoted strings.  At any token, the untokenized, original
        remainder can also be retrieved. */
    class ArgumentVector: public std::vector <std::string> {
        friend class ArgvCursor;
    public:
        using StringType = value_type;
    private:
        /// The original command string.
        const StringType original;
        /// Index of token locations in original string.
        std::vector <StringType::size_type> remainder_start;
    public:
        ArgumentVector (const StringType &command_line);
        
        /** Retrieve the raw, untokenized remainder of the string.
            @param index The index of the token to start retrieval.
            @return The remainder of the command line, with original
            spaces and quoting. */
        inline StringType remainder (size_type index) const {
            assert (index >= 0 && index < size());
            return original.substr (remainder_start [index]);
        }
    };

    /// An iterator for argument vectors.
    class ArgvCursor {
        const ArgumentVector *argv {nullptr};
        ArgumentVector::size_type point = 0;
    public:
        inline ArgvCursor (const ArgumentVector *the_argv): argv (the_argv) {
        }
        inline ArgvCursor (const ArgvCursor &from) = default;
        inline ArgvCursor (ArgvCursor &&from) = default;
        inline ArgvCursor &operator =(const ArgvCursor &from) = default;
        inline ArgvCursor &operator =(ArgvCursor &&from) = default;

        /** Check if the cursor is at the start of the command line.
            @return True if it is at the beginning. */
        inline bool isStart() const {
            return point == 0;
        }
        
        /** Check if the cursor is at the end of the command line.
            @return True if the cursor is at the end.  it does not
            point to a valid token. */
        inline bool isEnd() const {
            return point >= argv->size();
        }
        
        inline ArgvCursor &operator++() {
            point++;
            return *this;
        }
        
        inline ArgvCursor operator++(int) {
            ArgvCursor prior { *this };
            point++;
            return prior;
        }
        
        inline ArgvCursor operator+(int value) const {
            ArgvCursor result { *this };
            result.point += value;
            return result;
        }
        
        inline ArgvCursor operator-(int value) const {
            assert (point > 0);
            ArgvCursor result { *this };
            result.point -= value;
            return result;
        }

        /// Return the current token.
        inline const ArgumentVector::StringType &value () const {
            assert (point >= 0 && point < argv->size());
            return (*argv) [point];
        }
        
        /// Return the raw, unsplit remainder of the command line.
        inline const ArgumentVector::StringType remainingString () const {
            assert (point >= 0 && point < argv->size());
            return argv->remainder (point);
        }
        std::vector <ArgumentVector::StringType> remainingTokens () const;
    };
}

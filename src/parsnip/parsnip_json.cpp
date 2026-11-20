///
/// Parsnip serialization.
/// Serialization and parsing for JSON protocol.
/// @file       parsnip_json.cpp - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-02-27
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <ios>
#include <ostream>
#include <istream>
#include <sstream>

#include <cctype>

#include "parsnip.h"
#include "parsnip_helpers.h"

namespace Parsnip {

    /*
     *                  JSON Encoding
     */

    /** Write a string to a stream, literal-encoding special characters.
        @param target The stream to write to.
        @param value The string to write. */
    static void json_encode_string (std::ostream &target, const std::string &value) {
        target << '"';
        for (auto ch : value) {
            switch (ch) {
                case '\"':
                    target << "\\\"";
                    break;
                case '\\':
                    target << "\\\\";
                    break;
#ifdef PARSNIP_JSON_ENCODE_SOLIDUS
                case '/':
                    // This *may* be escaped, but is not required.
                    target << "\\/";  // Per json.org
                    break;
#endif
                case '\b':
                    target << "\\b";
                    break;
                case '\f':
                    target << "\\f";
                    break;
                case '\n':
                    target << "\\n";
                    break;
                case '\r':
                    target << "\\r";
                    break;
                case '\t':
                    target << "\\t";
                    break;
                default:
                    if (ch >= 0 && ch < 32) {
                        target << "\\u00" << hexdigit (ch >> 4) << hexdigit (ch & 0xf);
                    } else {
                        // Per RFC 8259, JSON shall be UTF-8 encoded.
                        // Since we do everything UTF-8, we're grand.
                        target << ch;
                    }
                    break;
            }
        }
        target << '"';
    }

    /** Write serial data encoded using JSON.
        @param target Stream to which to write data.
        @param indent Starting indentation for elements.
        @param suppress If true, suppress indent on first line.
        @returns The output stream. */
    std::ostream &Data::toJson (std::ostream &target, int indent, bool suppress) const {
        if (!suppress) {
            do_indent (target, indent);
        }
        switch (datatype) {
            case Type::Dictionary: {
                target << '{';
                bool first = true;
                for (const auto &value : *(data.dictionary)) {
                    if (!first) {
                        target << ',';
                    }
                    do_newline (target, indent);
                    do_indent (target, indent + 2);
                    json_encode_string (target, value.first);
                    target << ':';
                    value.second.toJson (target, indent + 2, true);
                    first = false;
                }
                do_newline (target, indent);
                do_indent (target, indent);
                target << '}';
                break;
            }
            case Type::List: {
                target << '[';
                bool first = true;
                for (const auto &value : *(data.list)) {
                    if (!first) {
                        target << ',';
                    }
                    do_newline (target, indent);
                    value.toJson (target, indent + 2);
                    first = false;
                }
                do_newline (target, indent);
                do_indent (target, indent);
                target << ']';
                break;
            }
            case Type::String:
            case Type::FlexibleString:
                json_encode_string (target, *(data.str));
                break;
            case Type::Real:
                target << data.real;
                break;
            case Type::Integer:
                target << data.integer;
                break;
            case Type::Boolean:
                target << (data.boolean ? "true" : "false");
                break;
            case Type::Null:
                target << "null";
                break;
        }
        return (target);
    }

    std::ostream &Data::dumpJson (const std::string &intro, std::ostream &target) const {
        target << intro << ": ";
        toJson (target, intro.size() + 2, true) << std::endl;
        return target;
    }

    /** Return serial data encoded using JSON.
        @param indent Starting indentation for elements.
        @returns A string. */
    std::string Data::toJson (int indent) const {
        std::ostringstream collector;
        toJson (collector, indent);
        return collector.str();
    }

    /*
     *                  JSON Parsing
     */

#ifdef PARSNIP_JSON_TRACK_POSITION
    /// A class to track file position the file is read in.
    class StreamPositionCounter {
    public:
        using Location = DataFormatError::Location;
        Location current_position {1, 0};

    private:
        Location next_position {1, 1};
        bool ignore_lf = false;
        std::istream &stream;

    public:
        inline StreamPositionCounter (std::istream &file) : stream (file){};

        /** Read the next character from a stream, counting position as we go.
        @param from The file to read from.
        @param eof_message The details of a DataFormatError that is thrown upon EOF.
        If a nullptr, the EOF is returned. */
        std::istream::int_type get () {
            std::istream::int_type ch = stream.get();
            current_position = next_position;
            if (ch == '\r') {
                next_position.line++;
                next_position.character = 1;
                ignore_lf = true;
            } else if (ch == '\n') {
                if (ignore_lf) {
                    ignore_lf = false;
                } else {
                    next_position.line++;
                    next_position.character = 1;
                }
            } else if (ch != std::istream::traits_type::eof()) {
                next_position.character++;
                ignore_lf = false;
            }
            return (ch);
        }
        std::istream::int_type peek () {
            return stream.peek();
        }
        void unget () {
            stream.unget ();
            next_position = current_position;
        }
        void clear() {
            stream.clear();
        }
        bool good () const {
            return stream.good ();
        }
    };
    DataFormatError::DataFormatError (const InputStream &loc, const std::string &detail)
    : Exception ("Invalid data format",
                 std::string (detail) + " at line " + std::to_string (loc.current_position.line) + ", character "
                         + std::to_string (loc.current_position.character)),
      error_location (loc.current_position) {
    }
#else
    DataFormatError::DataFormatError (const std::istream &, const std::string &detail)
    : Exception ("Invalid data format", detail) {
    }
#endif

    /** Read from a stream until a non-whitespace character is found.
        @throw DataFormatError if EOF is encountered. */
#ifdef PARSNIP_JSON_COMMENTS
    std::istream::int_type next_non_whitespace (InputStream &from) {
        while (from.good()) {
            std::istream::int_type ch = from.get();
            if (ch == std::istream::traits_type::eof()) {
                throw DataFormatError (from, "Unexpected end of file");
            } if (isspace (ch)) {
                continue;
            } else if (ch != '/') {
                return ch;
            }
            std::istream::int_type next = from.peek();
            if (next == '/') {
                // C++ style comment: read to end of line
                do {
                    ch = from.get();
                    if (ch == std::istream::traits_type::eof()) {
                        throw DataFormatError (from, "Unexpected end of file");
                    }
                } while (from.good() && (ch != '\r' && ch != '\n'));
            } else if (next == '*') {
                /* C-style comment */
                from.get();
                do {
                    ch = from.get();
                    if (ch == std::istream::traits_type::eof()) {
                        throw DataFormatError (from, "Unexpected end of file");
                    }
                } while (from.good() && (ch != '*' || from.peek() != '/'));
                from.get();
            } else {
                return ch;
            }
        };
        throw std::ios_base::failure (strerror (errno));
    };
#else
    std::istream::int_type next_non_whitespace (InputStream &from) {
        std::istream::int_type ch;
        while (from.good()) {
            ch = from.get();
            if (ch == std::istream::traits_type::eof()) {
                throw DataFormatError (from, "Unexpected end of file");
            } else if (!isspace (ch)) {
                return ch;
            }
        }
        throw std::ios_base::failure (strerror (errno));
    }
#endif

    /** Verify the next non-whitespace character in a stream is what's expected.
        @throws DataFormatError if the character isn't the one expected or EOF. */
    static inline void require_character (InputStream &from, std::istream::int_type expect) {
        auto ch = next_non_whitespace (from);
        if (ch != expect) {
            throw DataFormatError (from, std::string ("Expected: \'") + char (expect) + "\', got \'" + char (ch) + '\'');
        }
    }

    /** Parse hexadecimal digits from a stream.
        @param from The stream to take digits from.
        @param count The number of digits expected. */
    static int parse_hex_digits (InputStream &from, int count) {
        int result = 0;
        while (count-- > 0) {
            std::istream::int_type ch = from.get();
            if (ch == std::istream::traits_type::eof()) {
                throw DataFormatError (from, std::string ("End of file in hex digit-stream"));
            } else if (!isxdigit (tolower (ch))) {
                throw DataFormatError (from, std::string ("Non-hex digit: ") + char (ch));
            }
            result = (result << 4) | (strchr (hex_digits, tolower (ch)) - hex_digits);
        }
        return result;
    }

    /** Parse a string out of a JSON stream.
        @param from The stream to parse.
        @param quote The quote characters being used around the string.
        @return A string with literals converted to their real values. */
    static const std::string parse_json_string (InputStream &from, const char quote) {
        assert (quote == '\'' || quote == '\"');
        std::string value;
        std::istream::int_type ch;
        while ((ch = from.get()) != quote) {
            if (ch == std::istream::traits_type::eof()) {
                throw DataFormatError (from, "End of file while reading string: " + value);
            } else if (ch == '\\') {
                ch = from.get();
                switch (ch) {
                    case '\\':
                    case '/':
                    case '\"':
                    case '\'':
                        // Do nothing
                        break;
                    case 'b':
                        ch = '\b';
                        break;
                    case 'f':
                        ch = '\f';
                        break;
                    case 'n':
                        ch = '\n';
                        break;
                    case 'r':
                        ch = '\r';
                        break;
                    case 't':
                        ch = '\t';
                        break;
                    case 'u':
                        ch = parse_hex_digits (from, 4);
                        if (ch < 0 || ch >= 32) {
                            // No reason for \u with UTF-8, so refuse except control chars.
                            throw DataFormatError (from, "\\u-encoded non-control character");
                        }
                        break;
                    case std::istream::traits_type::eof():
                        throw DataFormatError (from, "Incomplete literal at end of file");
                    default:
                        throw DataFormatError (from, std::string ("Invalid literal: ") + std::to_string (ch));
                }
#ifdef PARSNIP_JSON_TRACK_POSITION
            } else if (ch == '\r' || ch == '\n') {
                throw DataFormatError (from, "Unterminated string");
#endif
            }
            value += ch;
        }
        return value;
    }

    /** Parse a keyword out of a JSON string.
        @param from The stream to read.
        @return A Data object corresponding to the keyword.
        @throws DataFormatError if the keyword is invalid. */
    static Data parse_json_inline_text (InputStream &from) {
        std::string word;
        std::istream::int_type ch{std::istream::traits_type::eof()};

        while (from.good() && (ch = from.get()) != std::istream::traits_type::eof() && isalpha (ch)) {
            word += ch;
        }
        // Reset the stream and put back unwanted characters that were read.
        from.clear();
        if (ch != std::istream::traits_type::eof()) {
            from.unget();
        }
        assert (!word.empty());
        if (word == "true") {
            return Data{true};
        } else if (word == "false") {
            return Data{false};
        } else if (word == "null") {
            return Data{};
        }
        throw DataFormatError (from, "Unknown word: " + word);
    }

    /** Parse a numeric value in a JSON stream.
        @param from The stream
        @return The number, wrapped in a Data object.
        @throws DataFormatError if there isn't a number. */
    static Data parse_json_inline_value (InputStream &from) {
        std::string number;
        std::istream::int_type ch{std::istream::traits_type::eof()};
        int base = 10;

        while (from.good() && (ch = from.get()) != std::istream::traits_type::eof()) {
#ifdef PARSNIP_JSON_HEXADECIMAL_NUMBERS
            if ((ch == 'x' || ch == 'X')) {
                base = 16;
            } else if (base == 16) {
                if (!isxdigit (ch)) {
                    break;
                }
            } else
#endif
            if (!isdigit (ch) && (ch != '.') && (ch != 'e') && (ch != 'E') && (ch != '-') && (ch != '+')) {
                break;
            }
            number += ch;
        }
        // Reset the stream and put back unwanted characters that were read.
        from.clear();
        if (ch != std::istream::traits_type::eof()) {
            from.unget();
        }

        // Try translating to an integer, if fails to parse, try again as a real.
        char *err;
        long int integer = strtol (number.c_str(), &err, base);
        if (*err == '\0') {
            return Data{integer};
        }
        if (base == 10) {
            double real = strtod (number.c_str(), &err);
            if (*err == '\0') {
                return Data{real};
            }
        }
        throw DataFormatError (from, number.c_str());
    }

    static Data parse_json_element (InputStream &from);

    /** Parse a JSON dictionary from a stream.
        @param from The stream to read.
        @return A Data object containing the dictionary.
        @throws DataFormatError if the dictionary is invalid. */
    static Data parse_json_dictionary (InputStream &from) {
        Data dict{Data::Dictionary};
        std::istream::int_type ch = next_non_whitespace (from);
        while (ch != '}') {
            if (ch != '\"' && ch != '\'') {
                throw DataFormatError (from, std::string ("Expected \'}\', \''\' or \'\"\', got \'") + char (ch) + '\'');
            }
            const std::string name = parse_json_string (from, ch);
            require_character (from, ':');
            dict [name] = parse_json_element (from);
            ch = next_non_whitespace (from);
            if (ch == ',') {
                // Tolerates a spare trailing comma before end of dictionary, which breaks spec.
                ch = next_non_whitespace (from);
            } else if (ch != '}') {
                throw DataFormatError (from, std::string ("Expected \',\' or \'}\', got \'") + char (ch) + '\'');
            }
        }
        return dict;
    }

    /** Parse a list from a JSON stream.
        @param from The stream to read from.
        @return A Data object containing the list. */
    static Data parse_json_list (InputStream &from) {
        Data list{Data::List};
        std::istream::int_type ch = next_non_whitespace (from);
        while (ch != ']') {
            from.unget();
            list.push_back (parse_json_element (from));
            ch = next_non_whitespace (from);
            if (ch == ',') {
                // Tolerates a spare trailing comma before end of list, which breaks spec.
                ch = next_non_whitespace (from);
            } else if (ch != ']') {
                throw DataFormatError (from, std::string ("Expected \',\' or \']\', got \'") + char (ch) + '\'');
            }
        }
        return list;
    }

    /** Extract data from a JSON stream.
        This function looks at the next non-whitespace character in the stream
        to determine the data type, then calls the appropriate handler to do the work.
        @param from The stream to parse.
        @return A Data object with the data extracted.
        @throws DataFormatError If the stream is invalid.
        @throws DataRangeError If the stream contains unrepresentable numbers. */
    static Data parse_json_element (InputStream &from) {
        std::istream::int_type ch = next_non_whitespace (from);
        switch (ch) {
            case '\"':
            case '\'':
                return Data{parse_json_string (from, ch)};
            case '{':
                return parse_json_dictionary (from);
            case '[':
                return parse_json_list (from);
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
            case '-': {
                from.unget();
                return parse_json_inline_value (from);
            }
            default: {
                from.unget();
                if (isalpha (ch)) {
                    return parse_json_inline_text (from);
                }
                throw DataFormatError (from, std::string ("At ") + char (ch));
            }
        }
    }

    /** Parse a stream as JSON data.
        @param from The stream to parse.
        @param check_termination If true (default), validates stream terminates (EOF)
        after end of JSON data.  If false, stops reading after a complete JSON object
        has been read.
        @returns Data structure accoring to parsed string.
        @throws DataFormatError if JSON is mangled.
        @throws DataRangeError If the stream contains unrepresentable numbers. */
    Data parse_json (std::istream &from, bool check_termination) {
        // Exceptions don't work reliably on OS X.  Turn them off and just don't use them,
        // but restore exception state before return in case the caller had them on.
        std::ios_base::iostate prior_exceptions_state = from.exceptions();
        from.exceptions (std::istream::goodbit);
#ifdef PARSNIP_JSON_TRACK_POSITION
        StreamPositionCounter input_stream (from);
#else
        std::istream &input_stream = from;
#endif
        Data data;
        try {
            data = parse_json_element (input_stream);
        } catch (const std::istream::failure &e) {
            from.exceptions (prior_exceptions_state);
            throw DataFormatError (input_stream, "Unexpected end of data");
        } catch (...) {
            from.exceptions (prior_exceptions_state);
            throw;
        }
        if (check_termination) {
            std::istream::int_type ch;
            do {
                ch = input_stream.get();
            } while (isspace (ch));
            if (ch != std::istream::traits_type::eof()) {
                from.exceptions (prior_exceptions_state);
                throw DataFormatError (input_stream, std::string ("Trailing junk @ \'") + char (ch) + '\'');
            }
        }
        from.exceptions (prior_exceptions_state);
        return data;
    }

    /** Parse a string as a JSON stream.
        @param from The string to parse.
        @returns Data structure accoring to parsed string.
        @throws DataFormatError if JSON is mangled.
        @throws DataRangeError If the stream contains unrepresentable numbers. */
    Data parse_json (const std::string &from) {
        std::istringstream source{from};
        return (parse_json (source));
    }

}  // namespace Parsnip

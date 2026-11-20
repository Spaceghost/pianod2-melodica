///
/// Various helpful templates, classes and functions.
/// @file       utility.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-08
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <cassert>

#include <exception>
#include <stdexcept>

#include "utility.h"


static inline const char *title_adjusted (const char *title) {
    assert (title);
    if (strncasecmp (title, "the ", 4) == 0) return title + 4;
    if (strncasecmp (title, "an ", 3) == 0) return title + 3;
    if (strncasecmp (title, "a ", 2) == 0) return title + 2;
    return title;
}

/** Compare titles, ignoring 'a', 'an', or 'the'.
    @param astr First title.
    @param bstr Second title.
    @return < 0 if astr < bstr; 0 if equal; >0 if astr > bstr. */
int compare_title_order (const std::string &astr, const std::string &bstr) {
    return strcasecmp (title_adjusted (astr.c_str()),
                       title_adjusted (bstr.c_str()));
}


/** Count words.
    @param start Start of string to count.
    @param end End of string to count.
    @return Number of words between `start` and `end`. */
static int count_words (const char *start, const char *end) {
    int count = 0;
    do {
        while (!isalpha (*start) && start != end)
            start++;
        if (start == end) return count;
        count++;
        while (isalpha (*start) && start != end)
            start++;
    } while (start != end);
    return count;
}

/** Compare an artist's name, which might be a person's name or a band.
    If it's a title, it may have 'a', 'an', or 'the', or maybe not.
    If it's a person, it may be /first last/ or /last, first/.
    @param astr First title.
    @param bstr Second title.
    @return true if values are equivalent, false if not.
    @warning Because this function guesses type of name by looking at the
    parameters, int return would not be sort-consistent.
    Thus, return type bool. */
bool compare_person_or_title (const std::string &astr, const std::string &bstr) {
    // Assess if this is a title comparison 
    const char *a = title_adjusted (astr.c_str());
    const char *b = title_adjusted (bstr.c_str());
    if (a != astr.c_str() || b != bstr.c_str()) {
        // Starts with a, an, or the; definitely title. 
        return strcasecmp (a, b) == 0;
    }

    // Look for commas in the names 
    const char *acomma = strchr (a, ',');
    const char *bcomma = strchr (b, ',');

    // If neither have commas, or both have commas great! 
    if (!acomma && !bcomma) {
        return strcasecmp (a, b) == 0;
    }

    // If either name has 2 commas, or more than one word before the comma, it's not a proper name 
    if ((acomma && strchr (acomma + 1, ',') != NULL) ||
        (bcomma && strchr (bcomma + 1, ',') != NULL) ||
        (acomma && count_words (a, acomma) != 1) ||
        (bcomma && count_words (b, bcomma) != 1)) {
        return strcasecmp (a, b) == 0;
    }

    const char *comma = acomma ? acomma+1 : bcomma+1;
    const char *before_comma = acomma ? a : b;
    const char *uncomma = acomma ? b : a;

    // Skip whitespace following the comma 
    while (isspace(*comma))
        comma++;
    // Try to match the given names 
    while (*uncomma && *comma &&
           tolower (*uncomma) == tolower (*comma)) {
        uncomma++; comma++;
    }
    if (isspace (*uncomma) && !*comma) {
        // Given names matched -- continue on surnames.
        while (isspace (*uncomma))
            uncomma++;
        while (*uncomma && *before_comma &&
               tolower (*uncomma) == tolower (*before_comma)) {
            uncomma++; before_comma++;
        }
        if (*uncomma == '\0' && *before_comma == ',') {
            return true;
        }
    }

    return strcasecmp (a, b) == 0;
}



/** Remove whitespace and other desireable characters, and make all
    lowercase for use as an ID. */
std::string makeIdentity (std::string s) {
    std::string::size_type index = 0;
    while (index < s.length()) {
        char ch = s[index];
        if (!isalnum (ch) && ch != '_' && ch != '-') {
            s.erase (index, 1);
        } else {
            s [index] = tolower (ch);
            index++;
        }
    }
    return s;
}



/** Split a string to words.  Mid-word punctuation is retained.
    - "Don't stop thinking" -> "Don't", "stop", "thinking"
    - "'Lyin' Eyes" -> "Lyin", "eyes"
    @param value The string to split.
    @return Words. */
std::vector<std::string> split_string (const std::string &value) {
    std::vector<std::string> results;
    std::string::size_type start = 0;
    while (start < value.length()) {
        while (start < value.length() && !isalnum (value.at (start)))
               start++;
        if (start < value.length()) {
            std::string::size_type end = start + 1;
            // Find span of the word, including apostrophes and hypens
            while (end < value.length() &&
                   (isalnum (value.at (end)) || value.at (end) == '\'' ||
                    value.at (end) == '-'))
                end++;

            // Trim trailing apostrophes and hypens
            std::string word = value.substr (start, end - start);
            while (!isalnum (word.back()))
                word.erase (word.length() - 1);
            results.push_back (word);
            start = end + 1;
        }
    }
    return results;
}

/** Split a string on a fixed pattern.
    @param value The string to split.
    @param separator The pattern for splitting the string.  It is removed from the result strings.
    @param include_empty_last If false, then if the separator occurs at end of string, then the
    null space following it is omitted from the results.
    @return 1 or more strings. */
std::vector<std::string> split_string (const std::string &value, const std::string separator, const bool include_empty_last) {
    assert (separator.size() != 0);
    std::vector<std::string> results;
    std::string::size_type last_start = 0;
    for (auto found = value.find (separator); found != std::string::npos; found = value.find (separator, last_start)) {
        results.push_back (value.substr (last_start, found - last_start));
        last_start = found + separator.size();
    }
    if (include_empty_last || last_start != value.size()) {
        results.push_back (value.substr (last_start));
    }
    return results;
}

/** Join strings together.
    @param list The strings to join.
    @param joiner The string to insert between list strings.
    @return The assembled string. */
std::string join_strings (const std::vector<std::string> &list, const std::string joiner) {
    std::string result;
    int len = 0;
    for (const auto &str : list) {
        len += str.size() + joiner.size();
    }
    if (!list.empty()) {
        result.reserve (len);
        result = list.front();
        for (auto it = list.begin() + 1; it != list.end(); it++) {
            result += joiner;
            result += *it;
        }
    }
    return result;
}




///
/// Symmetrical cipher using a text string as a key.
/// @author     Perette Barella
/// @date       2014-03-11
/// @copyright  Copyright 2012-2014 Devious Fish. All rights reserved.
///
/** @details
 This is a simplistic cipher intended to be difficult
 enough to inconvenience hand deciphering, but nothing
 truly secure.  In short: Enough to keep the riff-raff out.

 Usernames cannot be changed in pianod.  The user's name
 will be used to generate the key by feeding it through
 a 32-bit CRC generator.  Note that a 32-bit generator
 yields 31 bits of CRC.  We will then encipher the password
 symetrically with the key/CRC as follows:
 @code
 For each character, starting at the beginning:
 XOR the character with the least significant bits of
 the CRC as follows:
 0x01–0x1f (000x xxxx): Assert(0), or do nothing.
 0x20–0x3f (001x xxxx): XOR 5 bits
 0x40–0x7f (01xx xxxx): XOR 6 bits
 0x80–0xff (1xxx xxxx), XOR 7 bits of the CRC.
 Rotate right the CRC by the number of bits "consumed".
 @endcode
 This is certainly not NSA quality, and there are hazards of
 inventing your own encryption system; but given the inherent
 insecurity of anyone being able to strategically add
 printf ("%s's password is %s\n", user->password, user->name)
 and recompiling, this is adequate.  It is, after all,
 only a music server.
 */

static const lamerkey_t lamer_crc_generator = 0xae060ed2 ; // a/k/a divisor 
static const int lamer_keybits = sizeof (lamerkey_t) * 8 - 1;
// Thank random.org for our generator CRC. 

/** Compute a key from a string using a CRC algorithm.
    If VARIABLE_SIZE_DIVISORS is set, the CRC length is determined
    dynamically by the value in the divisor.  Otherwise, it is
    determined by the length of the generator set in constants above.
    @param data The string to use to generate the key.
    @param divisor The generator/divisor for performing the CRC.
    @param remainder For unit testing; set to 0 for generating keys.
    For test purposes, reinvoke this function pass the function's
    result for remainder.  The result should be 0; if not, it's broken.
    @return The computed key. */
lamerkey_t compute_crc (const char *data, lamerkey_t divisor, lamerkey_t remainder) {
    lamerkey_t key = 0;
#ifdef VARIABLE_SIZE_DIVISORS
    lamerkey_t divisor_flag = 1;
    lamerkey_t find_high_bit = divisor;
    // Grab the high bit from the generator, which will tell us when to apply divisor 
    for (divisor_flag = 1; divisor_flag ^ find_high_bit; divisor_flag <<= 1) {
        if (divisor_flag & find_high_bit) {
            find_high_bit ^= divisor_flag;
        }
    }
#else
    lamerkey_t divisor_flag = 1 << (lamer_keybits);
    // High bit of generator must be set 
    assert (lamer_crc_generator & divisor_flag);
#endif
    // Loop through the incoming bytes and bits, applying CRC math. 
    for (const char *in = data; *in; in++) {
        for (lamerkey_t bit = 0x80; bit; bit >>=1) {
            key <<= 1;
            if (*in & bit) {
                key |= 1;
            }
            if (key & divisor_flag) {
                key ^= divisor;
            }
        }
    }
    // Apply the remainder 
    for (lamerkey_t bit = divisor_flag >> 1; bit; bit >>= 1) {
        key <<= 1;
        if (remainder & bit) {
            key |= 1;
        }
        if (key & divisor_flag) {
            key ^= divisor;
        }
    }
#ifdef UNIT_TEST
    // Remainder should be symmetrical with the key, but beware infinitely recursing 
    static int recursing = 0;
    if (remainder == 0 && !recursing) {
        recursing = 1; // Avoid rare possibility that key is 0 and infinite recursion 
        assert (compute_crc (data, divisor, key) == remainder);
    }
    recursing = 0;
#endif
    return (key);
}

/** Generate a key from a string.
 @param source The string.
 @return The key. */
lamerkey_t create_key_from_string (const char *source) {
    return (compute_crc (source, lamer_crc_generator));
}


/** Encipher or decipher an item based on a key.
 @param keystr The string to use to generate a key.
 @param item The string to encipher or decipher.
 @return The enciphered/deciphered string, freshly malloced;
 or NULL on error.  Original strings are unmodified.
 @warning Allocates space on the heap for the returned string.
 Caller must free() this when done with the string. */
char *lamer_cipher (const char *keystr, const char *item) {
    char *output = strdup (item);
    if (output) {
        lamerkey_t key = create_key_from_string (keystr);
        char *out = output;
        for (const char *in = item; *in; in++, out++) {
            int bits;
            if ((*in & 0xe0) == 0x20) {
                bits = 5;
            } else if ((*in & 0xc0) == 0x40) {
                bits = 6;
            } else if ((*in & 0x80) == 0x80) {
                bits = 7;
            } else {
                // We canna encrypt this character, Cap'n. 
                assert (0);
                continue;
            }
            lamerkey_t mask = key & ((1 << bits) - 1);
            *out = (*in ^ mask);
            key = (key >> bits) | (mask << (lamer_keybits - bits));
        }
    }
    return output;
}

std::string lamer_cipher (const std::string &keystr, const std::string &item) {
    char *result = lamer_cipher (keystr.c_str(), item.c_str());
    std::string strresult (result);
    free (result);
    return strresult;
}

#ifdef UNIT_TEST

#include "failurecounter.h"

using FailureCounter = Test::FailureCounter;


void check_lamercrypt (const char *key, const char *value, FailureCounter &check) {
    lamerkey_t crc = compute_crc (key, lamer_crc_generator, 0);
    printf ("'%s' CRC: %08x\n", value, crc);
    lamerkey_t verify = compute_crc (key, lamer_crc_generator, crc);
    check.equal ("CRC key verification", 0, verify);
    char *crypting = new char[strlen (value) + 1];
    strcpy (crypting, value);
    char *encrypted = lamer_cipher (key, crypting);
    check.result (std::string (encrypted) != value, "Encryption", "String has not changed");
    char *decrypted = lamer_cipher (key, encrypted);
    check.equal (std::string (decrypted), std::string (value));
    free (encrypted);
    free (decrypted);
    delete []crypting;
}

void check_compare_title_order (const char *one, const char *two, long expect, FailureCounter &check) {
    check.signEqual (std::string (one) + " <=> " + two,
                          compare_title_order (one, two), expect);
}

void check_compare_person_or_title (const char *one, const char *two, bool expect, FailureCounter &check) {
    check.equal (std::string ("person_or_title_compare: ") + one + (expect ? " == " : " != ") + two,
                 expect, compare_person_or_title (one, two));
}
  
int main (int argc, char *argv[]) {
    if (argc == 2 && std::string (argv[1]) == "-c") {
        FailureCounter::initialize_colors();
    }
    FailureCounter check ("utility tests");
    check.subtest ("lamercrypt");
    check_lamercrypt("orca", "secrets", check);
    check_lamercrypt("baluga", "drowssap", check);
    check_lamercrypt("right", "youwontguess", check);
 
    check.subtest ("compare_title_order");
    check_compare_title_order ("foo", "bar", 1, check);
    check_compare_title_order ("the foo", "bar", 1, check);
    check_compare_title_order ("foo", "the bar", 1, check);
    check_compare_title_order ("the foo", "the bar", 1, check);

    check_compare_title_order ("bar", "foo", -1, check);
    check_compare_title_order ("a bar", "foo", -1, check);
    check_compare_title_order ("bar", "a foo", -1, check);
    check_compare_title_order ("a bar", "a foo", -1, check);

    check_compare_title_order ("an bar", "bar", 0, check);
    check_compare_title_order ("an bar", "bar", 0, check);
    check_compare_title_order ("bar", "an bar", 0, check);
    check_compare_title_order ("the bar", "an bar", 0, check);

    check_compare_person_or_title ("foo", "bar", false, check);
    check_compare_person_or_title ("the foo", "bar", false, check);
    check_compare_person_or_title ("foo", "the bar", false, check);
    check_compare_person_or_title ("the foo", "the bar", false, check);

    check_compare_person_or_title ("bar", "foo", false, check);
    check_compare_person_or_title ("a bar", "foo", false, check);
    check_compare_person_or_title ("bar", "a foo", false, check);
    check_compare_person_or_title ("a bar", "a foo", false, check);

    check_compare_person_or_title ("an bar", "bar", true, check);
    check_compare_person_or_title ("an bar", "bar", true, check);
    check_compare_person_or_title ("bar", "an bar", true, check);
    check_compare_person_or_title ("the bar", "an bar", true, check);
    
    check_compare_person_or_title("perette barella", "perette barella", true, check);
    check_compare_person_or_title("barella, perette", "pereTTe barella", true, check);
    check_compare_person_or_title("barella, perette", "barella, perEtte", true, check);
    check_compare_person_or_title("perette  bareLLa", "barella,perette", true, check);
    check_compare_person_or_title("johan sebastian beethoven",
                                    "beethoven, johan sebastian", true, check);

    check_compare_person_or_title("pat barella", "perette barella", false, check);
    check_compare_person_or_title("pat  barella", "barella,perette", false, check);
    check_compare_person_or_title("barella, pat", "perette barella", false, check);
    check_compare_person_or_title("barella, pat", "barella, perette", false, check);
    check_compare_person_or_title("johan sebastian beethoven",
                                    "beethoven, johan bach", false, check);
    return check.errorCount() ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif

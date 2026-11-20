///
/// Bruce Schneier's Blowfish Encryption.
/// @file       mediaunits/pandora/blowfish.cpp - pianod project
/// @author     Perette Barella, based on code by Bruce Schneier and Jim Conger
/// @date       2021-03-05
/// @license    MIT license.

#include "blowfish.h"

#include <stdexcept>

#include <cassert>
#include <cctype>
#include <vector>

#include <memory.h>
#include <arpa/inet.h>  // for hton, ntoh and friends (byte ordering)

// These routines implement the Melodica tuner wire protocol.
#include "melodica/blowfish.c"
#include <stdexcept>

BlowFish::BlowFish (const uint8_t *key, size_type length) {
    if (!melodica_blowfish_init(&cipher, key, length)) throw std::invalid_argument("Invalid tuner key");
}

BlowFish::BlowFish (const std::string &key)
: BlowFish (reinterpret_cast<const uint8_t *>(key.data()), key.size()) {}

void BlowFish::encrypt_or_decrypt (Action action, const uint8_t *input, size_type length,
                                    uint8_t *output, size_type capacity) const {
    const size_type padded = getOutputLength(length);
    if (padded > capacity) throw std::length_error("Tuner buffer is too small");
    if (length && input != output) memmove(output, input, length);
    if (padded > length) memset(output + length, 0, padded - length);
    for (size_type offset = 0; offset < padded; offset += 8) {
        if (action == Action::Encrypt) melodica_blowfish_encrypt_block(&cipher, output + offset);
        else melodica_blowfish_decrypt_block(&cipher, output + offset);
    }
}

std::string BlowFish::encrypt (const std::string &input) const {
    std::vector <uint8_t> output (getOutputLength (input.length()));
    encrypt_or_decrypt (Action::Encrypt,
                        reinterpret_cast<const uint8_t *> (input.c_str()),
                        input.length(),
                        output.data(),
                        output.size());
    return std::string (reinterpret_cast <char *> (output.data()), output.size());
}

/** Decipher ciphertext encrypted with Blowfish ECD.
    @param input The ciphertext to decipher.
    @return The deciphered message.  Any nulls added during
    encryption to pad the message to the necessary 64-bit/8-byte
    boundary are included.
    @warning There is no error checking.  If you have the wrong key,
    you'll get junk out.  There is no inherent error detection or
    validation. */
std::string BlowFish::decrypt (const std::string &input) const {
    assert (input.size() % 8 == 0);
    std::vector <uint8_t> output (input.length());
    encrypt_or_decrypt (Action::Decrypt,
                        reinterpret_cast<const uint8_t *> (input.c_str()),
                        input.length(),
                        output.data(),
                        output.size());
    return std::string (reinterpret_cast <char *> (output.data(), output.size()));
}

// Digits for translating to/from hex.
static const char *hex_digits = "0123456789abcdef";

/** Translate some bytes into their hexadecimal representation.
    @param bytes The beginning of the byte stream.
    @param length The number of bytes to hex encode.
    @return A string containing each byte encoded as 2 hex digits. */
std::string BlowFish::to_hex (const uint8_t *bytes, size_type length) {
    std::string hex_output;
    hex_output.reserve (2 * length);
    for (size_type i = 0; i < length; i++) {
        hex_output.push_back (hex_digits [bytes [i] >> 4]);
        hex_output.push_back (hex_digits [bytes [i] & 0xf]);
    }
    return hex_output;
}

/** Translate hexadecimal into a string.  Whitespace, punctuation and other
    junk is ignored.
    @param digits The digits to translate.
    @return The unhexed string.
    @throw invalid_argument upon bad input:
    - An odd number of digits.
    - Letters that are not hex digits. */
std::string BlowFish::from_hex (const std::string &digits) {
    if (digits.length() & 1) {
        throw std::invalid_argument ("Odd message length");
    }
    std::vector <uint8_t> bytes (digits.length() / 2);
    int pos = 0;
    for (auto ch : digits) {
        if (isxdigit (ch)) {
            char d = strchr (hex_digits, tolower (ch)) - hex_digits;
            if (pos & 1) {
                bytes [pos >> 1] |= d;
            } else {
                bytes [pos >> 1] = d << 4;
            }
            pos++;
        } else if (isalpha (ch)) {
            throw std::invalid_argument (std::string{ ch } + ": Not a hex digit");
        }
    }
    assert (pos == digits.length());
    return std::string (reinterpret_cast<char *> (bytes.data()), pos / 2);
}

/** Encrypt a string and return it has hex digits.
    @param input The message to encipher.
    @return A hex representation of the ciphertext. */
std::string BlowFish::encryptToHex (const std::string &input) const {
    return to_hex (encrypt (input));
}

/** Decrypt hex-encoded ciphertext and return it, optionally removing
    null padding that was added.  Whitespace or punctuation in the input
    are skipped.
    @param input The ciphertext.
    @param trim_nulls If true, any trailing nulls are removed.
    @return If the key is correct, the deciphered message.  If not, junk.
    @throw invalid_argument If the ciphertext has non-hex letters in it. */
std::string BlowFish::decryptFromHex (const std::string &input, bool trim_nulls) const {
    std::string message = from_hex (input);
    size_type len = message.length();
    if (len % 8 != 0) {
        throw std::invalid_argument ("Incorrect message length");
    }
    std::vector <uint8_t> output (len);
    encrypt_or_decrypt (Action::Decrypt, reinterpret_cast <const uint8_t *> (message.c_str()), len, output.data(), len);
    if (trim_nulls) {
        while (len > 0 && output [len - 1] == '\0') {
            len--;
        }
    }
    message.assign (reinterpret_cast<char *> (output.data()), len);
    return message;
}

///
/// Bruce Schneier's Blowfish Encryption.
/// @file       mediaunits/pandora/blowfish.h - pianod project
/// @author     Perette Barella, based on code by Bruce Schneier and Jim Conger
/// @date       2021-03-05
/// @license    MIT license.

#include <string>

#include <stdint.h>

class BlowFish {
public:
    using size_type = uint32_t;

private:
    static constexpr int NumberOfPasses = 16; // SBox passes; must be even
    static constexpr int MaximumKeyBytes = 56; // 448 bits max
    
    using PArray = uint32_t [NumberOfPasses + 2];
    using SBoxes = uint32_t [4][256];
    enum class Action { Encrypt, Decrypt };

    static const PArray InitialPArray;
    static const SBoxes InitialSBoxes;

    PArray parray;
    SBoxes sboxes;
    uint32_t fFunction (uint32_t in) const;
    void encipher (uint32_t *left_32, uint32_t *right_32) const;
    void decipher (uint32_t *left_32, uint32_t *right_32) const;

    void encrypt_or_decrypt (Action action,
                             const uint8_t *input,
                             size_type input_length,
                             uint8_t *output,
                             size_type output_size) const;

public:
    BlowFish (const uint8_t *key, size_type key_length);
    BlowFish (const std::string &key);

    /** Get length of required output buffer.
        @param length The length of the input message.
        @return The length the output buffer must be to accommodate the message. */
    static constexpr inline size_type getOutputLength (size_type length) {
        size_type r = length % 8;
        return (r ? (length + (8 - r)) : length);
    }

    /** Encipher a BlowFish ECD message.
        @warning Don't use this cipher for new stuff.  And don't use ECD.
        @param input The message to encrypt.
        @param input_length The length of the message.
        @param output A buffer for the resulting message.  This may be
        the same as the input to encrypt in place.
        @param output_size The size of the output.  This must be large
        enough to accommodate any added padding. */
    inline void encrypt (const uint8_t *input, size_type input_length, uint8_t *output, size_type output_size) const {
        encrypt_or_decrypt (Action::Encrypt, input, input_length, output, output_size);
    }

    /** Decipher a BlowFish ECD message.
        @param input The ciphertext.
        @param input_length The length of the message.  This *should* be a multiple of
        8, as all enciphered messages end up a multiple of 8 bytes.  Truncated messages
        will decipher incorrectly.
        @param output A buffer for the resulting message.  This may be
        the same as the input to encrypt in place.
        @param output_size The size of the output.  This must be large
        enough to accommodate any added padding. */
    inline void decrypt (const uint8_t *input, size_type input_length, uint8_t *output, size_type output_size) const {
        encrypt_or_decrypt (Action::Decrypt, input, input_length, output, output_size);
    }

    std::string encrypt (const std::string &input) const;
    std::string decrypt (const std::string &input) const;

    static std::string to_hex (const uint8_t *bytes, size_type length);
    /** Convert a string to its hexadecimal representation. */
    static inline std::string to_hex (const std::string &input) {
        return to_hex (reinterpret_cast <const uint8_t *> (input.c_str()), input.length());
    }
    static std::string from_hex (const std::string &digits);

    std::string encryptToHex (const std::string &input) const;
    std::string decryptFromHex (const std::string &input, bool trim_nulls = true) const;
};

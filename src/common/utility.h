///
/// Various helpful templates, classes and functions.
/// @file       utility.h - pianod
/// @author     Perette Barella
/// @date       2015-12-08
/// @copyright  Copyright (c) 2014-2022 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cctype>

#include <exception>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <functional>
#include <vector>


extern int compare_title_order (const std::string &a, const std::string &b);
/** Compare titles, ignoring 'a', 'an', or 'the'.
 @param astr First title.
 @param bstr Second title.
 @return true if titles match, false otherwise. */
inline bool compare_titles (const std::string &astr, const std::string &bstr) {
    return compare_title_order (astr, bstr) == 0;
}

extern bool compare_person_or_title (const std::string &astr, const std::string &bstr);

typedef uint32_t lamerkey_t;
extern lamerkey_t create_key_from_string (const char *source);
extern lamerkey_t compute_crc (const char *data, lamerkey_t divisor, lamerkey_t remainder = 0);
extern char *lamer_cipher (const char *keystr, const char *item);
std::string lamer_cipher (const std::string &keystr, const std::string &item);
inline lamerkey_t create_key_from_string (const std::string &source) {
    return create_key_from_string (source.c_str());
}

// Bring back strcasecmp, strncasecmp, strcasestr, strncasestr
inline int strcasecmp (const std::string &a, const char *b) {
    return strcasecmp (a.c_str(), b);
}
inline int strcasecmp (const char *a, const std::string &b) {
    return strcasecmp (a, b.c_str());
}
inline int strcasecmp (const std::string &a, const std::string &b) {
    return strcasecmp (a.c_str(), b.c_str());
}

inline int strncasecmp (const std::string &a, const char *b, size_t n) {
    return strncasecmp (a.c_str(), b, n);
}
inline int strncasecmp (const char *a, const std::string &b, size_t n) {
    return strncasecmp (a, b.c_str(), n);
}
inline int strncasecmp (const std::string &a, const std::string &b, size_t n) {
    return strncasecmp (a.c_str(), b.c_str(), n);
}

inline const char * strcasestr (const std::string &a, const char *b) {
    return strcasestr (a.c_str(), b);
}
inline const char * strcasestr (const char *a, const std::string &b) {
    return strcasestr (a, b.c_str());
}
inline const char * strcasestr (const std::string &a, const std::string &b) {
    return strcasestr (a.c_str(), b.c_str());
}
inline std::string strtolower (std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

/** Trim whitespace from beginning of a string.
    @author Evan Teran
    @see http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring */
inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace (ch); } ));
    return s;
}

/** Trim whitespace from end of a string.
    @author Evan Teran
    @see http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring */
inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {return !std::isspace (ch); }).base(), s.end());
    return s;
}

/** Trim whitespace from both ends of a string.
    @author Evan Teran
    @see http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring */
inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

inline std::string trim (const std::string &s) {
    std::string t {s};
    return ltrim (rtrim (t));
}

std::string makeIdentity (std::string s);

std::vector<std::string> split_string (const std::string &value);
std::vector<std::string> split_string (const std::string &value, const std::string separator, bool include_empty_last = true);

std::string join_strings (const std::vector<std::string> &list, const std::string joiner);
/// Joing strings together using a space between concatenated values.
inline std::string join_strings (const std::vector<std::string> &list) {
    return join_strings (list, " ");
}
    


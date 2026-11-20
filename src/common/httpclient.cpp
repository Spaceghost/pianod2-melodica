///
/// HTTP client request provider.
/// @file       httpclient.cpp - pianod project
/// @author     Perette Barella
/// @date       2020-03-24
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

#include <cassert>
#include <cstring>
#include <ostream>
#include <iomanip>

#include "httpclient.h"

HttpClient::Exception::Exception (const char *reason) : explanation (reason){};

HttpClient::Exception::Exception (const char *function, CURLcode code)
: explanation (std::string (function) + ": " + curl_easy_strerror (code)){};

HttpClient::Exception::Exception (const char *function, CURLcode code, const char *detail)
: explanation (std::string (function) + ": " + curl_easy_strerror (code) + ": " + detail){};

HttpClient::Exception::Exception (CURLcode code, const char *detail)
: explanation (std::string (curl_easy_strerror (code)) + ": " + detail){};

// Retrieve text corresponding to the request type.
static const char *request_type (HttpClient::RequestType type) {
    switch (type) {
        case HttpClient::RequestType::Get:
            return "Get";
        case HttpClient::RequestType::Post:
            return "Post";
        case HttpClient::RequestType::Head:
            return "Head";
        default:
            return "Unknown";
    }
}

/** Render the contents of a dictionary to a file/stream.
    @param target The stream to which to send the dictionary.
    @param title Descriptive text for the dictionary.
    @param dict The dictionary to render. */
static void dump_string_dict (std::ostream &target, const char *title, const HttpClient::StringDict &dict) {
    target << title << ":";
    if (dict.empty()) {
        target << " None" << std::endl;
    } else {
        target << std::endl;
        for (const auto &item : dict) {
            target << "  " << std::setw (25) << std::left << item.first << ": " << item.second << std::endl;
        }
    }
}

/** Log request details to a file/stream.
    @param target Stream to which to send the details. */
void HttpClient::Request::dump (std::ostream &target) const {
    target << "Request type: " << request_type (type) << std::endl << "URL: " << URL << std::endl;
    dump_string_dict (target, "Parameters", parameters);
    dump_string_dict (target, "Headers", headers);
    dump_string_dict (target, "Cookies", cookies);
    target << "Body:" << std::endl << body << std::endl << std::endl;
}

/** Log response details to a file/stream.
    @param target Stream to which to send the response. */
void HttpClient::Response::dump (std::ostream &target) const {
    target << "HTTP status: " << http_status << std::endl << "Status text: " << http_status_text << std::endl;
    dump_string_dict (target, header_corruption ? "Headers (corrupted)" : "Headers (Clean)", headers);
    dump_string_dict (target, "Cookies", cookies);
    target << "Body:" << std::endl << body << std::endl << std::endl;
}

/** Parse a `set-cookie` header and cookies to a dictionary.
    @param dict The dictionary to which to add cookies.
    @param text The header text containing cookies. */
static bool add_cookies (HttpClient::StringDict &dict, const std::string &text) {
    for (std::string::size_type index = 0; index < text.size();) {
        std::string::size_type equal = text.find ('=', index);
        if (equal == text.npos) {
            /* Cookie flags can be either "secure" (https only) or
               "httponly" (i.e., no JavaScript access).
               If we validate them, we run the risk of problems if
               some new flag is introduced in the future.  So just
               ignore/approve whatever is there. */
            return true;
        }
        std::string::size_type semi = text.find (';', equal);
        std::string name = text.substr (index, equal - index);

        if (semi == text.npos) {
            dict[name] = text.substr (equal + 1);
            return true;
        } else {
            dict[name] = text.substr (equal + 1, semi - equal - 1);
        }
        index = semi + 1;
        while (index < text.size() && isspace (text[index])) {
            index++;
        }
    }
    return true;
}

/// Callback to collect headers from CURL.
static size_t header_callback (char *buffer, size_t size, size_t nitems, void *userdata) {
    assert (size == 1);  // per https://curl.haxx.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
    HttpClient::Response *response = static_cast<HttpClient::Response *> (userdata);

    try {
        std::string header (buffer, nitems);

        // Strip whitespace from tail
        for (auto it = header.end(); it != header.begin(); /* nothing */) {
            it--;
            if (!isspace (*it)) {
                break;
            }
            header.erase (it);
        }
        if (response->debug) {
            std::cerr << "header_callback: processing: " << header << std::endl;
        }

        if (header.empty()) {
            // End of header.  If there ends up being multiple transactions,
            // there could be another, which will again start with a status line.
            response->awaiting_http_status_line = true;
            return (response->http_status == 0 ? 0 : nitems);
        }
        // First line is the HTTP status message.
        if (response->awaiting_http_status_line) {
            response->awaiting_http_status_line = false;
            std::string::size_type index = 0;
            while (index < header.size() && !isspace (header[index])) {
                index++;
            }
            std::string::size_type err_index;
            response->http_status = std::stoi (header.substr (index), &err_index);
            index += err_index;

            while (index < header.size() && isspace (header[index])) {
                index++;
            }
            response->http_status_text = header.substr (index);
            return ((header.substr (0, 5) == "HTTP/" && response->http_status >= 100 && response->http_status <= 599)
                            ? nitems
                            : 0);
        }

        // Other lines are name-value pairs, colon-separated.
        std::string::size_type index = header.find (':');
        if (index == std::string::npos) {
            response->header_corruption = true;
        } else {
            std::string name = header.substr (0, index++);
            // Per RFC 2616, header names are case insensitive, so make them all lowercase for our convenience.
            for (std::string::size_type i = 0; i < name.size(); i++) {
                name[i] = tolower (name[i]);
            }
            while (index < header.size() && isspace (header[index])) {
                index++;
            }
            if (index == header.size()) {
                response->header_corruption = true;
            } else if (name == "set-cookie") {
                if (add_cookies (response->cookies, header.substr (index, header.size()))
                    || response->header_corruption) {
                    return nitems;
                }
                response->header_corruption = true;
            } else {
                auto value = std::make_pair<> (name, header.substr (index, header.size()));
                if (name == "Content-length") {
                    response->body.reserve (atoi (value.second.c_str() + 5));
                }
                response->headers.insert (std::move (value));
                return nitems;
            }
        }
        std::cerr << "header_callback: error processing: " << header << std::endl;
    } catch (const std::exception &ex) {
        std::cerr << "header_callback: " << ex.what() << std::endl;
    }

    return 0;
}

/// callback to assembed received response from CURL.
static size_t receive_callback (char *buffer, size_t size, size_t nitems, void *userdata) {
    assert (size == 1);  // per https://curl.haxx.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
    std::string chunk (buffer, nitems);
    std::stringstream *morgue = static_cast<std::stringstream *> (userdata);
    *morgue << chunk;
    return (morgue->good() ? nitems : 0);
}

class CurlSList {
    struct curl_slist *list{nullptr};
    std::vector<std::string> retained;

public:
    CurlSList (std::vector<std::string> &&headers) : retained (std::move (headers)) {
        for (const auto &header : retained) {
            struct curl_slist *temp = curl_slist_append (list, header.c_str());
            if (temp) {
                list = temp;
            } else {
                curl_slist_free_all (list);
                list = nullptr;
                throw HttpClient::Exception ("curl_slist_append", CURLE_OUT_OF_MEMORY);
                break;
            }
        }
    }
    CurlSList (CurlSList &&) noexcept = default;
    inline ~CurlSList() noexcept {
        curl_slist_free_all (list);
    }
    inline curl_slist *get() const noexcept {
        return list;
    }
};

static CurlSList assemble_headers (const HttpClient::Request &request) {
    std::vector<std::string> headers;
    if (!request.cookies.empty()) {
        assert (request.headers.find ("Cookie") == request.headers.end());
        std::ostringstream cookies;
        cookies << "Cookie: ";
        bool first = true;
        for (const auto &cookie : request.cookies) {
            if (first) {
                first = false;
            } else {
                cookies << "; ";
            }
            cookies << cookie.first << "=" << cookie.second;
        }
        headers.push_back (cookies.str());
    }
    for (const auto &header : request.headers) {
        headers.push_back (header.first + ": " + header.second);
    }

    return (CurlSList (std::move (headers)));
}

static std::string assemble_url (CURL *session, const HttpClient::Request &request) {
    assert (session);
    if (request.parameters.empty()) {
        return request.URL;
    }
    std::string url = request.URL;
    assert (url.find ('?') == std::string::npos);
    url += "?";
    for (const auto &param : request.parameters) {
        char *encoded = curl_easy_escape (session, param.second.c_str(), param.second.length());
        if (!encoded) {
            throw HttpClient::Exception ("curl_easy_escape", CURLE_OUT_OF_MEMORY);
        }
        url += param.first + "=" + encoded + "&";
        curl_free (encoded);
    }
    url.erase (url.length() - 1, 1);
    return url;
}

// The macro may be ugly, but it's less ugly than repeating the checks.
#define set_and_check(option, value)                                          \
    if ((code = curl_easy_setopt (session, (option), (value))) != CURLE_OK) { \
        throw Exception ("curl_easy_setopt", code, #option);                  \
    }

// Perform an HTTP transaction.
const HttpClient::Response HttpClient::performHttpRequest (const HttpClient::Request &request) {
    // Assemble the headers, then put them in a list for CURL.
    CurlSList headers{assemble_headers (request)};
    std::string url = assemble_url (session, request);
    request.debug |= debug;
    
    try {
        CURLcode code;
        char error_message[CURL_ERROR_SIZE];
        strcpy (error_message, "No details.");

        set_and_check (CURLOPT_VERBOSE, debug ? 1L : 0L);
        set_and_check (CURLOPT_ERRORBUFFER, error_message);
        set_and_check (request.type == RequestType::Head
                               ? CURLOPT_NOBODY
                               : request.type == RequestType::Post ? CURLOPT_POST : CURLOPT_HTTPGET,
                       1L);
        set_and_check (CURLOPT_NOPROGRESS, 1L);
        set_and_check (CURLOPT_URL, url.c_str());
        set_and_check (CURLOPT_TIMEOUT, request.timeout);
        if (!request.proxy.empty()) {
            set_and_check (CURLOPT_PROXY, request.proxy.c_str());
        }
        if (!request.user_agent.empty()) {
            set_and_check (CURLOPT_USERAGENT, request.user_agent.c_str());
        }
        set_and_check (CURLOPT_HTTPHEADER, headers.get());

        // Set up request body callback.
        assert (request.body.empty() == (request.type != RequestType::Post));
        if (request.type == RequestType::Post) {
            set_and_check (CURLOPT_POSTFIELDS, request.body.c_str());
            set_and_check (CURLOPT_POSTFIELDSIZE, request.body.size());
        }

        // Set up response header callback.
        Response response;
        response.debug = request.debug;
        set_and_check (CURLOPT_HEADERFUNCTION, header_callback);
        set_and_check (CURLOPT_HEADERDATA, &response);

        // Set up response body collection callback.
        std::stringstream cemetery;
        set_and_check (CURLOPT_WRITEFUNCTION, receive_callback);
        set_and_check (CURLOPT_WRITEDATA, &cemetery);

        // Do it.
        code = curl_easy_perform (session);
        if (code != CURLE_OK) {
            throw Exception ("curl_easy_perform", code, error_message);
        }
        curl_easy_reset (session);
        response.curl_code = code;
        response.body = cemetery.str();
        return (response);
    } catch (const std::exception &ex) {
        curl_easy_reset (session);
        throw;
    }
}
#undef set_and_check

HttpClient::HttpClient (bool dbx) {
    debug = dbx;
    session = curl_easy_init();
    if (!session) {
        throw Exception ("curl_easy_init", CURLE_OUT_OF_MEMORY);
    }
}

HttpClient::~HttpClient() {
    curl_easy_cleanup (session);
}

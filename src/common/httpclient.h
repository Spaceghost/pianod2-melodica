///
/// HTTP client request provider.
/// @file       httpclient.h - pianod project
/// @author     Perette Barella
/// @date       2020-03-24
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <string>
#include <sstream>
#include <exception>
#include <unordered_map>
#include <ostream>
#include <iostream>

#include <curl/curl.h>

class HttpClient {
    CURL *session;
    bool debug {false};
    
public:
    class Exception : public std::exception {
    private:
        std::string explanation;
    public:
        Exception (const char *reason = "HTTP error");
        Exception (const char *function, CURLcode code);
        Exception (const char *function, CURLcode code, const char *detail);
        Exception (CURLcode code, const char *detail);
        virtual const char *what() const noexcept override final {
            return explanation.c_str();
        }
    };

    enum class RequestType {
        Head,
        Get,
        Post
    };

    using StringDict = std::unordered_map<std::string, std::string>;
    struct Request {
        mutable bool debug {false}; // If set, sends status information to stdlog.
        long timeout {0}; // in seconds
        RequestType type {RequestType::Get};
        std::string URL;  // URL.  May include query parameters, XOR they may be in `parameters`.
        std::string proxy; // If set, a proxy server through which to request
        StringDict parameters; // Query parameters to add to the URL
        StringDict headers; // Headers to include with HTTP request
        StringDict cookies; // Cookies to include in HTTP header
        std::string user_agent;  // If empty, a default (the package name) is used instead.
        std::string body; // Body to include with POST requests.
        void dump (std::ostream & = std::cerr) const;
    };

    struct Response {
        bool debug {false}; // Set to match the request debug flag.
        bool awaiting_http_status_line {true}; // Private state variable
        CURLcode curl_code; // Final libcurl status code before returning.
        int http_status{0}; // The HTTP status included in the response.
        std::string http_status_text; // The text portion of the HTTP status
        bool header_corruption{false}; // If true, the header wasn't parsed correctly.
        StringDict headers; // Headers included with the response.
        StringDict cookies; // Cookies included with the response.
        std::string body; // The body of the response.
        void dump (std::ostream & = std::cerr) const;
    };

    HttpClient (bool dbx = false);
    ~HttpClient ();
    HttpClient (const HttpClient &) = delete;
    HttpClient (HttpClient &&) = delete;
    HttpClient &operator= (const HttpClient &) = delete;
    HttpClient &operator= (HttpClient &&) = delete;
    const Response performHttpRequest (const Request &request);
};

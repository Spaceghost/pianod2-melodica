
#include <iostream>

#include "httpclient.h"

HttpClient client;

bool dump_site (const char *url) {
    HttpClient::Request request;
    request.type = HttpClient::RequestType::Post;
    request.URL = url;
    request.headers ["X-foo-bar"] = "Disregard";
    request.headers ["X-foo-bat"] = "Bakayaro";
    request.cookies ["simple"] = "chocolatechip";
    request.cookies ["complex"] = "snickerdoodle";
    request.body = "{}";
    request.dump (std::cerr);
    try {
        HttpClient::Response response = client.performHttpRequest (request);
        response.dump (std::cout);
        return true;
    } catch (const HttpClient::Exception &ex) {
        std::cerr << "HTTP error: " << ex.what() << std::endl;
        return false;
    } catch (const std::exception &ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return false;
    }
}



int main (int argc, char **argv) {
    if (argc <= 1) {
        std::cerr << "Usage: " << *argv << " <url>..." << std::endl;
        return 0;
    }
    int exit_status = 0;
    for (int i = 1; i < argc; i++) {
        if (!dump_site (*(argv + i))) {
            exit_status = 1;
        }
    }
    return exit_status;
}


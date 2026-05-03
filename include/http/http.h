#ifndef HTTP_H
#define HTTP_H

#include <string>
#include <map>
#include <winsock2.h>
#include <openssl/ssl.h>

struct URL {
    std::string host;
    std::string path;
    int port;
    bool is_https;
};

// This is in development part so any one can use this to modify the code and make it better. I am open to any suggestion and contribution. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback. I will be very happy to see your contribution in this project. I am also open to any suggestion and feedback.
// struct HttpResponse
// {
//     int statusCode;
//     std::map<std::string,std::string> headers;
//     std::string body;
// };

class HTTPClient {
public:
    HTTPClient();
    ~HTTPClient();

    std::string GET(const std::string& url);
    std::string POST(const std::string& url, const std::string& body, const std::string& contentType = "application/json");

private:
    // lifecycle
    void initWinsock();
    void cleanupWinsock();
    void initSSL();
    void cleanupSSL();

    // core steps
    URL parseURL(const std::string& url);
    SOCKET connectToHost(const std::string& host, int port);

    // TLS helpers
    SSL_CTX* ctx;

    // I/O helpers (unify HTTP + HTTPS)
    int writeAll(SOCKET sock, SSL* ssl, const char* data, int len);
    int readSome(SOCKET sock, SSL* ssl, char* buf, int cap);

};

#endif
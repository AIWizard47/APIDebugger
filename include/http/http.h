#ifndef HTTP_H
#define HTTP_H

#include <string>
#include <winsock2.h>
#include <openssl/ssl.h>

struct URL {
    std::string host;
    std::string path;
    int port;
    bool is_https;
};

class HTTPClient {
public:
    HTTPClient();
    ~HTTPClient();

    std::string GET(const std::string& url);

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
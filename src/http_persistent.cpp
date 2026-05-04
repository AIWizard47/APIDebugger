// http/http_persistent.cpp
#include "http/http.h"
#include <ws2tcpip.h>
#include <openssl/err.h>
#include <iostream>
#include <filesystem>
#pragma comment(lib, "Ws2_32.lib")

// ──────────────────────────────────────────────────────────────────────────────
// Init / cleanup
// ──────────────────────────────────────────────────────────────────────────────

PersistentHTTPClient::PersistentHTTPClient() {
    initWinsock();
    initSSL();
}

PersistentHTTPClient::~PersistentHTTPClient() {
    disconnect();
    if (ctx_) SSL_CTX_free(ctx_);
    EVP_cleanup();
    WSACleanup();
}

void PersistentHTTPClient::initWinsock() {
    WSADATA wd{};
    if (WSAStartup(MAKEWORD(2,2), &wd) != 0)
        throw std::runtime_error("WSAStartup failed");
}

void PersistentHTTPClient::initSSL() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ctx_) throw std::runtime_error("SSL_CTX_new failed");

    // Session caching → 0-RTT TLS resumption on reconnect
    SSL_CTX_set_session_cache_mode(ctx_,
        SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_LOOKUP);

    std::string certPath =
        (std::filesystem::current_path() / ".." / "certs" / "ca-bundle.crt").string();
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_load_verify_locations(ctx_, certPath.c_str(), nullptr);
    SSL_CTX_set_default_verify_paths(ctx_);
}

// ──────────────────────────────────────────────────────────────────────────────
// Connect / disconnect
// ──────────────────────────────────────────────────────────────────────────────

bool PersistentHTTPClient::connect(const std::string& urlStr) {
    // Parse URL once; store host/port/https for reconnects
    URL u{};
    std::string temp = urlStr;
    if (temp.rfind("https://", 0) == 0) { temp = temp.substr(8); u.port = 443; u.is_https = true; }
    else if (temp.rfind("http://", 0) == 0) { temp = temp.substr(7); u.port = 80;  u.is_https = false; }
    else throw std::runtime_error("Only http/https supported");

    size_t pos = temp.find('/');
    u.host = (pos == std::string::npos) ? temp : temp.substr(0, pos);
    u.path = (pos == std::string::npos) ? "/" : temp.substr(pos);

    host_  = u.host;
    port_  = u.port;
    https_ = u.is_https;

    return reconnect();
}

// Internal: (re)open the socket+TLS
bool PersistentHTTPClient::reconnect() {
    disconnect();   // clean up any leftover state first

    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &res) != 0)
        return false;

    for (addrinfo* p = res; p; p = p->ai_next) {
        sock_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock_ == INVALID_SOCKET) continue;

        // Disable Nagle — we're sending complete HTTP requests, no benefit from batching
        int flag = 1;
        setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

        if (::connect(sock_, p->ai_addr, (int)p->ai_addrlen) == 0) break;
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (sock_ == INVALID_SOCKET) return false;

    if (https_) {
        ssl_ = SSL_new(ctx_);
        if (!ssl_) { closesocket(sock_); sock_ = INVALID_SOCKET; return false; }

        SSL_set_tlsext_host_name(ssl_, host_.c_str());
        SSL_set_fd(ssl_, (int)sock_);

        // Hostname verification
        X509_VERIFY_PARAM* param = SSL_get0_param(ssl_);
        X509_VERIFY_PARAM_set1_host(param, host_.c_str(), 0);

        if (SSL_connect(ssl_) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl_); ssl_ = nullptr;
            closesocket(sock_); sock_ = INVALID_SOCKET;
            return false;
        }
    }
    return true;
}

void PersistentHTTPClient::disconnect() {
    if (ssl_) { SSL_shutdown(ssl_); SSL_free(ssl_); ssl_ = nullptr; }
    if (sock_ != INVALID_SOCKET) { closesocket(sock_); sock_ = INVALID_SOCKET; }
}

bool PersistentHTTPClient::isConnected() const {
    return sock_ != INVALID_SOCKET;
}

// ──────────────────────────────────────────────────────────────────────────────
// I/O helpers
// ──────────────────────────────────────────────────────────────────────────────

int PersistentHTTPClient::writeAll(const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = ssl_ ? SSL_write(ssl_, data + sent, len - sent)
                     : send(sock_, data + sent, len - sent, 0);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}

int PersistentHTTPClient::readSome(char* buf, int cap) {
    return ssl_ ? SSL_read(ssl_, buf, cap)
                : recv(sock_, buf, cap, 0);
}

// Reads a full HTTP/1.1 response, honouring Content-Length
// so the connection can be reused immediately after.
std::string PersistentHTTPClient::readResponse() {
    std::string raw;
    char buf[8192];
    int n;

    // Phase 1: read until we have the complete headers (\r\n\r\n)
    while (true) {
        n = readSome(buf, sizeof(buf));
        if (n <= 0) break;
        raw.append(buf, n);
        if (raw.find("\r\n\r\n") != std::string::npos) break;
    }

    // Extract Content-Length header
    size_t sep = raw.find("\r\n\r\n");
    if (sep == std::string::npos) return raw;

    std::string headers = raw.substr(0, sep);
    size_t bodyRead = raw.size() - sep - 4;
    int contentLength = -1;

    std::string clKey = "Content-Length: ";
    size_t clPos = headers.find(clKey);
    if (clPos != std::string::npos) {
        contentLength = std::stoi(headers.substr(
            clPos + clKey.size(),
            headers.find("\r\n", clPos) - clPos - clKey.size()
        ));
    }

    // Phase 2: read exactly the body bytes we expect
    while (contentLength > 0 && (int)bodyRead < contentLength) {
        n = readSome(buf, std::min((int)sizeof(buf),
                                   contentLength - (int)bodyRead));
        if (n <= 0) break;
        raw.append(buf, n);
        bodyRead += n;
    }

    return raw;
}

// ──────────────────────────────────────────────────────────────────────────────
// Public request methods (auto-reconnect on broken pipe)
// ──────────────────────────────────────────────────────────────────────────────

std::string PersistentHTTPClient::doRequest(const std::string& rawRequest) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!isConnected() && !reconnect())
            throw std::runtime_error("Cannot reconnect to host");

        if (writeAll(rawRequest.c_str(), (int)rawRequest.size()) <= 0) {
            // Server closed the keep-alive connection → reconnect once
            if (!reconnect()) throw std::runtime_error("Reconnect failed");
            continue;
        }

        std::string resp = readResponse();
        if (!resp.empty()) return resp;

        disconnect(); // empty read = server closed; retry
    }
    throw std::runtime_error("Request failed after reconnect");
}

std::string PersistentHTTPClient::GET(const std::string& path) {
    std::string req =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host_ + "\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    return doRequest(req);
}

std::string PersistentHTTPClient::POST(const std::string& path,
                                        const std::string& body,
                                        const std::string& contentType) {
    std::string req =
        "POST " + path + " HTTP/1.1\r\n"
        "Host: " + host_ + "\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: keep-alive\r\n"
        "\r\n" + body;
    return doRequest(req);
}
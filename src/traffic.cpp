// traffic/traffic.cpp
#include "traffic/traffic.h"
#include "http/http.h"

#include <thread>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <atomic>

// ──────────────────────────────────────────────────────────────────────────────
// Barrier: spin until every thread has called in, then release all together.
// std::barrier (C++20) would be cleaner but this works on C++17.
// ──────────────────────────────────────────────────────────────────────────────

static void waitForAll(std::atomic<int>& ready, int total) {
    ready.fetch_add(1, std::memory_order_release);
    // Spin-wait — tiny overhead, guarantees near-simultaneous launch
    while (ready.load(std::memory_order_acquire) < total)
        std::this_thread::yield();
}

// ──────────────────────────────────────────────────────────────────────────────
// Per-thread worker
// ──────────────────────────────────────────────────────────────────────────────

void TrafficTester::worker(
    const std::string& method,
    const std::string& url,
    const std::string& path,
    const std::string& body,
    int   requestCount,
    std::atomic<int>& readyCount,
    int   totalThreads,
    bool  thunderingHerd)
{
    // Each thread creates its OWN persistent connection
    PersistentHTTPClient client;

    try {
        client.connect(url);    // one TLS handshake per thread
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lk(mtx_);
        std::cerr << "[thread] connect failed: " << e.what() << "\n";
        failCount_.fetch_add(requestCount, std::memory_order_relaxed);
        return;
    }

    if (thunderingHerd) {
        // All threads hit the barrier; the last one releases everyone
        waitForAll(readyCount, totalThreads);
    }

    for (int i = 0; i < requestCount; ++i) {
        try {
            auto t0 = std::chrono::high_resolution_clock::now();

            std::string response = (method == "POST")
                ? client.POST(path, body)
                : client.GET(path);   // keep-alive: socket stays open

            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            bool ok = response.find("HTTP/1.1 2") != std::string::npos
                   || response.find("HTTP/1.1 3") != std::string::npos;

            {
                std::lock_guard<std::mutex> lk(mtx_);
                latencies_.push_back(ms);
            }

            if (ok) successCount_.fetch_add(1, std::memory_order_relaxed);
            else    failCount_.fetch_add(1,    std::memory_order_relaxed);

        } catch (const std::exception& e) {
            failCount_.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[thread] request error: " << e.what() << "\n";
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Percentile helper (call AFTER sorting)
// ──────────────────────────────────────────────────────────────────────────────

double TrafficTester::percentile(std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    size_t idx = (size_t)(p / 100.0 * (sorted.size() - 1));
    return sorted[std::min(idx, sorted.size() - 1)];
}

// ──────────────────────────────────────────────────────────────────────────────
// Run
// ──────────────────────────────────────────────────────────────────────────────

TrafficResult TrafficTester::run(
    const std::string& method,
    const std::string& url,
    int   totalRequests,
    int   threadCount,
    const std::string& body,
    bool  thunderingHerd)
{
    successCount_.store(0);
    failCount_.store(0);
    latencies_.clear();
    latencies_.reserve(totalRequests);

    // Pre-parse path from URL once
    std::string path = "/";
    auto strip = [](const std::string& u) -> std::pair<std::string,std::string> {
        size_t off = u.rfind("://");
        std::string rest = (off == std::string::npos) ? u : u.substr(off + 3);
        size_t p = rest.find('/');
        return { rest.substr(0, p == std::string::npos ? rest.size() : p),
                 p == std::string::npos ? "/" : rest.substr(p) };
    };
    path = strip(url).second;

    std::atomic<int> readyCount{0};
    int perThread = totalRequests / threadCount;
    int remainder = totalRequests % threadCount;

    auto wallStart = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        int count = perThread + (i < remainder ? 1 : 0); // distribute remainder
        threads.emplace_back(
            &TrafficTester::worker, this,
            method, url, path, body, count,
            std::ref(readyCount), threadCount, thunderingHerd
        );
    }

    for (auto& t : threads) t.join();

    auto wallEnd = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(wallEnd - wallStart).count();

    // Sort latencies once for all percentile calculations
    std::sort(latencies_.begin(), latencies_.end());

    TrafficResult r;
    r.successRequests  = successCount_.load();
    r.failedRequests   = failCount_.load();
    r.totalRequests    = r.successRequests + r.failedRequests;
    r.totalTimeMs      = totalMs;

    if (!latencies_.empty()) {
        r.minLatencyMs = latencies_.front();
        r.maxLatencyMs = latencies_.back();
        r.avgLatencyMs = std::accumulate(latencies_.begin(), latencies_.end(), 0.0)
                         / latencies_.size();
        r.p50LatencyMs = percentile(latencies_, 50.0);
        r.p95LatencyMs = percentile(latencies_, 95.0);
        r.p99LatencyMs = percentile(latencies_, 99.0);
    }

    r.requestsPerSecond = (r.totalRequests * 1000.0) / totalMs;
    return r;
}
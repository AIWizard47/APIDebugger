#include "traffic/traffic.h"
#include "http/http.h"

#include <thread>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iostream>

void TrafficTester::worker(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    int requestCount
)
{
    // each thread gets its own client
    HTTPClient client;
    for (int i = 0; i < requestCount; i++)
    {
        try
        {
            auto start =
                std::chrono::high_resolution_clock::now();

            std::string response;

            if (method == "GET")
            {
                response = client.GET(url);
            }
            else if (method == "POST")
            {
                response = client.POST(url, body);
            }
            auto end =
                std::chrono::high_resolution_clock::now();

            double latency =
                std::chrono::duration<double, std::milli>(
                    end - start
                ).count();

            {
                std::lock_guard<std::mutex> lock(mtx);

                latencies.push_back(latency);

                if (response.find("200") != std::string::npos ||
                    response.find("201") != std::string::npos)
                {
                    successCount++;
                }
                else
                {
                    failCount++;
                }
            }
            std::cout << "Thread done request "
                      << i + 1
                      << "\n";
        }
        catch (const std::exception& e)
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << "Error: "
                      << e.what()
                      << "\n";

            failCount++;
        }
    }
}

TrafficResult TrafficTester::run(
    HTTPClient& client,
    const std::string& method,
    const std::string& url,
    int totalRequests,
    int threadCount,
    const std::string& body
)
{
    successCount = 0;
    failCount = 0;
    latencies.clear();

    auto totalStart =
        std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    int perThread =
        totalRequests / threadCount;

    for (int i = 0; i < threadCount; i++)
    {
        threads.emplace_back(
            &TrafficTester::worker,
            this,
            method,
            url,
            body,
            perThread
        );
    }

    for (auto& t : threads)
    {
        t.join();
    }

    auto totalEnd =
        std::chrono::high_resolution_clock::now();

    double totalTime =
        std::chrono::duration<double, std::milli>(
            totalEnd - totalStart
        ).count();

    TrafficResult result;

    result.totalRequests =
        successCount + failCount;

    result.successRequests =
        successCount;

    result.failedRequests =
        failCount;

    result.totalTimeMs =
        totalTime;

    if (!latencies.empty())
    {
        result.minLatencyMs =
            *std::min_element(
                latencies.begin(),
                latencies.end()
            );

        result.maxLatencyMs =
            *std::max_element(
                latencies.begin(),
                latencies.end()
            );

        result.avgLatencyMs =
            std::accumulate(
                latencies.begin(),
                latencies.end(),
                0.0
            ) / latencies.size();
    }

    result.requestsPerSecond =
        (result.totalRequests * 1000.0) / totalTime;

    return result;
}
#pragma once

#include <vector>
#include <string>
#include <thread>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <map>

#include <curl/curl.h>

class HttpResponse
{
public:
    HttpResponse(int code)
        : m_code(code)
        , m_data(0)
    { }

    HttpResponse(int code, std::vector<uint8_t>* data)
        : m_code(code)
        , m_data(data)
    { }

    HttpResponse(const HttpResponse& other)
        : m_code(other.m_code)
        , m_data(other.m_data)
    { }

    ~HttpResponse() { }

    int code() const { return m_code; }
    const std::vector<uint8_t>* data() const { return m_data; }

private:
    int m_code;
    std::vector<uint8_t>* m_data;
};

///////////////////////////////////////////////////////////////////////////////

class Curl
{
    // Only CurlBatch can create.
    friend class CurlBatch;

public:
    ~Curl();

    HttpResponse get(std::string url, std::vector<std::string> headers);
    HttpResponse put(
            std::string url,
            std::vector<std::string> headers,
            const std::vector<uint8_t>* data);

private:
    Curl(std::size_t id);
    std::size_t id() const { return m_id; }

    CURL* m_curl;
    curl_slist* m_headers;

    std::vector<uint8_t> m_data;

    const std::size_t m_id;

    void init(std::string url, const std::vector<std::string>& headers);
};

///////////////////////////////////////////////////////////////////////////////

class CurlBatch
{
    // Only CurlPool can create.
    friend class CurlPool;

public:
    HttpResponse get(
            std::string url,
            std::vector<std::string> headers);

    HttpResponse put(
            std::string url,
            std::vector<std::string> headers,
            const std::vector<uint8_t>* data);

private:
    CurlBatch(std::size_t id, std::size_t batchSize);
    std::size_t id() const { return m_id; }

    std::shared_ptr<Curl> acquire();
    void release(std::shared_ptr<Curl>);

    std::vector<std::size_t> m_available;
    std::vector<std::shared_ptr<Curl>> m_curls;

    const std::size_t m_id;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};

///////////////////////////////////////////////////////////////////////////////

class CurlPool
{
public:
    CurlPool(std::size_t numBatches, std::size_t batchSize);

    std::shared_ptr<CurlBatch> acquire();
    void release(std::shared_ptr<CurlBatch> curlBatch);

private:
    std::vector<std::size_t> m_available;
    std::map<std::size_t, std::shared_ptr<CurlBatch>> m_curlBatches;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};


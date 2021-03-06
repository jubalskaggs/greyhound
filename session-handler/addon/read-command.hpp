#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include <v8.h>
#include <node.h>

#include <pdal/Dimension.hpp>
#include <pdal/PointContext.hpp>

void errorCallback(
        v8::Persistent<v8::Function> callback,
        std::string errMsg);

class QueryData;
class ItcBufferPool;
class ItcBuffer;

struct RasterMeta
{
    RasterMeta() : xBegin(), xEnd(), xStep(), yBegin(), yEnd(), yStep() { }

    RasterMeta(
            double xBegin,
            double xEnd,
            double xStep,
            double yBegin,
            double yEnd,
            double yStep)
        : xBegin(xBegin)
        , xEnd(xEnd)
        , xStep(xStep)
        , yBegin(yBegin)
        , yEnd(yEnd)
        , yStep(yStep)
    { }

    double xBegin;
    double xEnd;
    double xStep;
    double yBegin;
    double yEnd;
    double yStep;

    std::size_t xNum() const { return std::round((xEnd - xBegin) / xStep); }
    std::size_t yNum() const { return std::round((yEnd - yBegin) / yStep); }
};

struct DimInfo
{
    DimInfo(std::string name, std::string type, std::size_t size)
        : id(pdal::Dimension::id(name)), type(type), size(size)
    { }

    DimInfo(
            const pdal::Dimension::Id::Enum id,
            const pdal::Dimension::Type::Enum type)
        : id(id)
        , type(pdal::Dimension::toName(pdal::Dimension::base(type)))
        , size(pdal::Dimension::size(type))
    { }

    const pdal::Dimension::Id::Enum id;
    const std::string type;
    const std::size_t size;
};

struct Schema
{
    explicit Schema(std::vector<DimInfo> dims) : dims(dims) { }
    const std::vector<DimInfo> dims;

    std::size_t stride(bool rasterize = false) const
    {
        std::size_t stride(0);

        for (const auto& dim : dims)
        {
            if (!rasterize || !Schema::rasterOmit(dim.id))
            {
                stride += dim.size;
            }
        }

        if (rasterize)
        {
            // Clientward rasterization schemas always contain a byte to specify
            // whether a point at this location in the raster exists.
            ++stride;
        }

        return stride;
    }

    bool use(const DimInfo& dim, bool rasterize) const
    {
        return !rasterize || !Schema::rasterOmit(dim.id);
    }

    static bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }

};

class PdalSession;

struct Background
{
    void safe(std::string& err, std::function<void()> f)
    {
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            err = e.what();
        }
        catch (const std::bad_alloc& ba)
        {
            err = "Caught bad alloc";
        }
        catch (...)
        {
            err = "Unknown error";
        }
    }
};

class ReadCommand : public Background
{
public:
    void run();

    virtual void read(std::size_t maxNumBytes);

    virtual bool rasterize() const { return false; }
    virtual std::size_t numBytes() const
    {
        return numPoints() * m_schema.stride();
    }

    void cancel(bool cancel) { m_cancel = cancel; }
    std::string& errMsg() { return m_errMsg; }
    std::shared_ptr<ItcBuffer> getBuffer() { return m_itcBuffer; }
    ItcBufferPool& getBufferPool() { return m_itcBufferPool; }

    bool done() const;
    void acquire();

    std::size_t numPoints() const;
    std::string readId()    const { return m_readId; }
    bool        cancel()    const { return m_cancel; }
    v8::Persistent<v8::Function> queryCallback() const { return m_queryCallback; }
    v8::Persistent<v8::Function> dataCallback() const { return m_dataCallback; }

    ReadCommand(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

    virtual ~ReadCommand()
    {
        m_queryCallback.Dispose();
        m_dataCallback.Dispose();

        if (m_async)
        {
            uv_close((uv_handle_t*)m_async, NULL);
        }
    }

    uv_async_t* async() { return m_async; }

protected:
    virtual void query() = 0;

    std::shared_ptr<PdalSession> m_pdalSession;

    ItcBufferPool& m_itcBufferPool;
    std::shared_ptr<ItcBuffer> m_itcBuffer;
    uv_async_t* m_async;

    const std::string m_readId;
    const std::string m_host;
    const std::size_t m_port;
    const bool m_compress;
    const Schema m_schema;
    std::size_t m_numSent;
    std::shared_ptr<QueryData> m_queryData;

    v8::Persistent<v8::Function> m_queryCallback;
    v8::Persistent<v8::Function> m_dataCallback;
    bool m_cancel;

    std::string m_errMsg;

private:
    Schema schemaOrDefault(Schema reqSchema);
};

class ReadCommandUnindexed : public ReadCommand
{
public:
    ReadCommandUnindexed(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            std::size_t start,
            std::size_t count,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const std::size_t m_start;
    const std::size_t m_count;
};

class ReadCommandPointRadius : public ReadCommand
{
public:
    ReadCommandPointRadius(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            bool is3d,
            double radius,
            double x,
            double y,
            double z,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const bool m_is3d;
    const double m_radius;
    const double m_x;
    const double m_y;
    const double m_z;
};

class ReadCommandQuadIndex : public ReadCommand
{
public:
    ReadCommandQuadIndex(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

protected:
    virtual void query();

    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;
};

class ReadCommandBoundedQuadIndex : public ReadCommandQuadIndex
{
public:
    ReadCommandBoundedQuadIndex(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const double m_xMin;
    const double m_yMin;
    const double m_xMax;
    const double m_yMax;
};

class ReadCommandRastered : public ReadCommand
{
public:
    ReadCommandRastered(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

    ReadCommandRastered(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            RasterMeta rasterMeta,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

    virtual void read(std::size_t maxNumBytes);
    virtual std::size_t numBytes() const;

    virtual bool rasterize() const { return true; }
    RasterMeta rasterMeta() const { return m_rasterMeta; }

protected:
    virtual void query();

    RasterMeta m_rasterMeta;
};

class ReadCommandQuadLevel : public ReadCommandRastered
{
public:
    ReadCommandQuadLevel(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            Schema schema,
            std::size_t level,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const std::size_t m_level;
};

class ReadCommandFactory
{
public:
    static ReadCommand* create(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            const v8::Arguments& args);
};


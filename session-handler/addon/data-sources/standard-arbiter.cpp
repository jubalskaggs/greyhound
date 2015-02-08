#include <iostream>

#include "read-queries/base.hpp"
#include "types/schema.hpp"
#include "standard-arbiter.hpp"

StandardArbiter::StandardArbiter(
        const std::string& pipelineId,
        const std::string& filename,
        const bool serialCompress,
        const SerialPaths& serialPaths)
    : m_pipelineId(pipelineId)
    , m_filename(filename)
    , m_serialCompress(serialCompress)
    , m_serialPaths(serialPaths)
    , m_mutex()
    , m_serialDataSource()
    , m_liveDataSource()
{
    std::cout << "Init: " << pipelineId << " - " << filename << std::endl;

    // Try to awaken from serialized source.  If unsuccessful, initialize a
    // live source.
    if (!awaken())
    {
        if (!enliven())
        {
            std::cout << "Init FAILED for " << pipelineId << std::endl;
            throw std::runtime_error("Could not create " + m_pipelineId);
        }
    }
}

bool StandardArbiter::awaken()
{
    // Don't lock here!
    if (!m_serialDataSource)
    {
        GreyReader* reader(
                GreyReaderFactory::create(
                    m_pipelineId,
                    m_serialPaths));

        if (reader)
        {
            m_serialDataSource.reset(new SerialDataSource(reader));
            std::cout << "Created serial source " << m_pipelineId << std::endl;
        }
    }

    return !!m_serialDataSource;
}

bool StandardArbiter::enliven()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_liveDataSource)
    {
        try
        {
            m_liveDataSource.reset(
                    new LiveDataSource(m_pipelineId, m_filename));
        }
        catch (...)
        {
            std::cout << "Bad live source " << m_pipelineId << std::endl;
            m_liveDataSource.reset();
        }

        if (m_liveDataSource)
            std::cout << "Created live source " << m_pipelineId << std::endl;
    }

    return !!m_liveDataSource;
}

bool StandardArbiter::serialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_liveDataSource && !m_serialDataSource)
    {
        // Ok to call this multiple times.
        m_liveDataSource->serialize(m_serialCompress, m_serialPaths);
        std::cout << "Serialized - awakening " << m_pipelineId << std::endl;
        awaken();

        // TODO Safely clear the quadIndex from the live data source.  All its
        // pending READs must complete first - new ones (except for the custom
        // raster) will already be routed to the serial data source.
    }

    return !!m_serialDataSource;
}

std::size_t StandardArbiter::getNumPoints() const
{
    if      (m_serialDataSource)    return m_serialDataSource->getNumPoints();
    else if (m_liveDataSource)      return m_liveDataSource->getNumPoints();
    else throw std::runtime_error("Not initialized - " + m_pipelineId);
}

std::string StandardArbiter::getSchema() const
{
    if      (m_serialDataSource)    return m_serialDataSource->getSchema();
    else if (m_liveDataSource)      return m_liveDataSource->getSchema();
    else throw std::runtime_error("Not initialized - " + m_pipelineId);
}

std::string StandardArbiter::getStats() const
{
    if      (m_serialDataSource)    return m_serialDataSource->getStats();
    else if (m_liveDataSource)      return m_liveDataSource->getStats();
    else throw std::runtime_error("Not initialized - " + m_pipelineId);
}

std::string StandardArbiter::getSrs() const
{
    if      (m_serialDataSource)    return m_serialDataSource->getSrs();
    else if (m_liveDataSource)      return m_liveDataSource->getSrs();
    else throw std::runtime_error("Not initialized - " + m_pipelineId);
}

std::vector<std::size_t> StandardArbiter::getFills() const
{
    if      (m_serialDataSource)    return m_serialDataSource->getFills();
    else if (m_liveDataSource)      return m_liveDataSource->getFills();
    else throw std::runtime_error("Not initialized - " + m_pipelineId);
}

std::shared_ptr<ReadQuery> StandardArbiter::queryUnindexed(
        const Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    if (m_liveDataSource || enliven())
    {
        return m_liveDataSource->queryUnindexed(
                schema,
                compress,
                start,
                count);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::shared_ptr<ReadQuery> StandardArbiter::query(
        const Schema& schema,
        bool compress,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::ostringstream logParams;
    logParams <<
            "\tPipeline ID: " << m_pipelineId << "\n" <<
            "\tBBox: [" << bbox.min().x << "," << bbox.min().y << "," <<
                           bbox.max().x << "," << bbox.max().y << "]\n" <<
            "\tDepth range: " << depthBegin << "-" << depthEnd;

    if (m_serialDataSource)
    {
        std::cout <<
            "Reading from serial source\n" <<
            logParams.str() <<
            std::endl;

        return m_serialDataSource->query(
                schema,
                compress,
                bbox,
                depthBegin,
                depthEnd);
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return m_liveDataSource->query(
                schema,
                compress,
                bbox,
                depthBegin,
                depthEnd);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}


std::shared_ptr<ReadQuery> StandardArbiter::query(
        const Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::ostringstream logParams;
    logParams <<
            "\tPipeline ID: " << m_pipelineId << "\n" <<
            "\tDepth range: " << depthBegin << "-" << depthEnd;

    if (m_serialDataSource)
    {
        std::cout <<
            "Reading from serial source\n" <<
            logParams.str() <<
            std::endl;

        return m_serialDataSource->query(
                schema,
                compress,
                depthBegin,
                depthEnd);
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return m_liveDataSource->query(
                schema,
                compress,
                depthBegin,
                depthEnd);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}


std::shared_ptr<ReadQuery> StandardArbiter::query(
        const Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    std::ostringstream logParams;
    logParams <<
            "\tPipeline ID: " << m_pipelineId << "\n" <<
            "\tRasterize: " << rasterize << "\n";

    if (m_serialDataSource)
    {
        std::cout <<
            "Reading from serial source\n" <<
            logParams.str() <<
            std::endl;

        return m_serialDataSource->query(
                schema,
                compress,
                rasterize,
                rasterMeta);
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return m_liveDataSource->query(
                schema,
                compress,
                rasterize,
                rasterMeta);
    }
    else
    {
        throw std::runtime_error("not initialized!");
    }
}

std::shared_ptr<ReadQuery> StandardArbiter::query(
        const Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    if (m_liveDataSource || enliven())
    {
        return m_liveDataSource->query(
                schema,
                compress,
                rasterMeta);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::shared_ptr<ReadQuery> StandardArbiter::query(
        const Schema& schema,
        bool compress,
        bool is3d,
        double radius,
        double x,
        double y,
        double z)
{
    if (m_liveDataSource || enliven())
    {
        return m_liveDataSource->query(
                schema,
                compress,
                is3d,
                radius,
                x,
                y,
                z);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

const pdal::PointContext& StandardArbiter::pointContext() const
{
    if (m_serialDataSource) return m_serialDataSource->pointContext();
    else if (m_liveDataSource) return m_liveDataSource->pointContext();
    else throw std::runtime_error("Not initialized!");
}

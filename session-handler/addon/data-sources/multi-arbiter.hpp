#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "data-sources/arbiter.hpp"

namespace pdal
{
    class PointContext;
}

class BBox;
class MultiDataSource;
class RasterMeta;
class ReadQuery;
class Schema;
class SerialPaths;

class MultiArbiter : public Arbiter
{
public:
    MultiArbiter(
            const std::string& pipelineId,
            const std::vector<std::string>& paths,
            const Schema& schema,
            const BBox& bbox,
            const bool serialCompress,
            const SerialPaths& serialPaths);
    ~MultiArbiter() { }

    virtual std::size_t getNumPoints() const;
    virtual std::string getSchema() const;
    virtual std::string getStats() const;
    virtual std::string getSrs() const;
    virtual std::vector<std::size_t> getFills() const;

    virtual const pdal::PointContext& pointContext() const;

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

private:
    std::shared_ptr<MultiDataSource> m_multiDataSource;
};

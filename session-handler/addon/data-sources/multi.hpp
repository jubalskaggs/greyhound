#pragma once

#include <vector>
#include <string>

#include "data-sources/base.hpp"

namespace pdal
{
    class PointContext;
}

class ReadQuery;
class SerialPaths;
class Schema;
class SleepyTree;

class MultiDataSource : public DataSource
{
public:
    MultiDataSource(
            const std::string& pipelineId,
            const std::vector<std::string>& paths,
            const Schema& schema,
            const BBox& bbox);

    virtual std::size_t getNumPoints() const;
    virtual std::string getSchema() const;
    virtual std::string getStats() const;
    virtual std::string getSrs() const;
    virtual std::vector<std::size_t> getFills() const;

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            std::size_t depthBegin,
            std::size_t depthEnd);

    const pdal::PointContext& pointContext() const;

private:
    const std::string m_pipelineId;
    const std::vector<std::string> m_paths;

    std::shared_ptr<SleepyTree> m_sleepyTree;
};

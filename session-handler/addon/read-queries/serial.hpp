#pragma once

#include "read-queries/base.hpp"

class Schema;

class SerialReadQuery : public ReadQuery
{
public:
    SerialReadQuery(
            const Schema& schema,
            bool compress,
            bool rasterize,
            GreyQuery greyQuery);

    virtual std::size_t numPoints() const;
    virtual bool eof() const;
    virtual bool serial() const;

private:
    GreyQuery m_greyQuery;

    virtual void readPoint(
            uint8_t* pos,
            const Schema& schema,
            bool rasterize) const;
};

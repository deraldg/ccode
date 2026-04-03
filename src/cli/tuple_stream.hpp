// src/cli/tuple_stream.hpp
#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "tuple_types.hpp"

namespace dottalk {

// Minimal stream interface for Super Browser.
class TupleStream {
public:
    virtual ~TupleStream() = default;

    virtual void top() = 0;
    virtual void bottom() = 0;
    virtual void skip(long n) = 0;

    virtual std::vector<TupleRow> next_page(std::size_t max_rows) = 0;
    virtual std::string status_line() const = 0;
};

} // namespace dottalk

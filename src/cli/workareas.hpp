#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include "xbase.hpp"

// workareas: lightweight OOP wrapper over XBaseEngine area slots.
//
// This variant extends the wrapper surface so WorkArea can forward
// runtime kind/capability information from DbArea.
//
// NOTE:
//   This assumes DbArea exposes:
//     xbase::AreaKind kind() const noexcept;
//     bool supports(xbase::AreaCapability cap) const noexcept;

namespace workareas {

    class WorkArea {
    public:
        WorkArea() = default;
        explicit WorkArea(std::size_t slot0) noexcept : slot0_(slot0) {}

        std::size_t slot() const noexcept { return slot0_; }
        bool valid() const noexcept;
        xbase::DbArea* db() const noexcept;
        bool is_open() const noexcept;

        std::string logical_name() const;
        std::string file_name() const;
        std::string label() const;
        bool has_label() const;

        xbase::AreaKind kind() const noexcept;
        bool supports(xbase::AreaCapability cap) const noexcept;

    private:
        std::size_t slot0_ = static_cast<std::size_t>(-1);
    };

    class WorkAreaSet {
    public:
        WorkAreaSet() = default;

        std::size_t count() const noexcept;
        WorkArea operator[](std::size_t slot0) const noexcept;

        WorkArea current() const noexcept;
        std::size_t current_slot() const noexcept;

        void print(std::ostream& os) const;
    };

    std::size_t count();
    xbase::DbArea* at(std::size_t slot0);
    const char* name(std::size_t slot0);
    std::size_t current_slot();
    void print(std::ostream& os);
    inline void show(std::ostream& os) { print(os); }
    void show(xbase::DbArea& area);
    WorkAreaSet all();
    WorkArea current();

} // namespace workareas

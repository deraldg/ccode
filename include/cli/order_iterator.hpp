#pragma once
// include/cli/order_iterator.hpp
//
// Single-point order traversal provider for CLI commands.
// Goal: keep LIST/SMARTLIST/BROWSE/SCAN from re-implementing index walking.
//
// Policy:
// - If no order: NATURAL (1..recCount64)
// - INX: use INX loader
// - CNX: use CNX loader (or stopgap DB-derived order if that is your current rule)
// - CDX: use CDX family semantics, with execution mode selected per area
//        (Fallback or Lmdb)
//
// This file does not print. It only yields recnos.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace cli {

enum class OrderBackend {
    Natural,
    Inx,
    Cnx,
    Cdx,
    Isx,
    Csx,
};

enum class CdxExecMode {
    Fallback,
    Lmdb,
};

struct OrderIterSpec {
    OrderBackend backend = OrderBackend::Natural;
    CdxExecMode  cdx_mode = CdxExecMode::Fallback;
    std::string  container_path;  // active order file path (.inx/.cnx/.cdx/.isx/.csx), if any
    std::string  tag;             // active tag (CNX/CDX), if any
    bool         ascending = true;
};

// Collect ordered recnos in ASC order (caller can reverse if descending).
// Returns false if an ordered backend was requested but could not be read.
bool order_collect_recnos_asc(xbase::DbArea& area,
                              std::vector<uint64_t>& out_recnos,
                              OrderIterSpec* out_spec = nullptr,
                              std::string* out_err = nullptr);

// Visitor-based iteration (lets you avoid materializing all recnos if you later stream LMDB).
// Visitor returns false to stop early.
bool order_iterate_recnos(xbase::DbArea& area,
                          const std::function<bool(uint64_t recno)>& visitor,
                          OrderIterSpec* out_spec = nullptr,
                          std::string* out_err = nullptr);

} // namespace cli
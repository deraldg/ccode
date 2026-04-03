// src/cli/index_summary.cpp
#include "index_summary.hpp"

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "cli/order_state.hpp"
#include "cnx/cnx.hpp" // CNX tagdir reading (your existing CNX helper)

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using dottalk::IndexSummary;
namespace fs = std::filesystem;

// ----- helpers -------------------------------------------------------------

static inline std::string up_copy(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); }
    );
    return s;
}

static inline bool ends_with_ext_ci(const std::string& path, const char* ext3) {
    const size_t n = path.size();
    return n >= 4 && path[n - 4] == '.' &&
           (char)std::toupper((unsigned char)path[n - 3]) == ext3[0] &&
           (char)std::toupper((unsigned char)path[n - 2]) == ext3[1] &&
           (char)std::toupper((unsigned char)path[n - 1]) == ext3[2];
}

static inline bool rd_u16(std::istream& in, uint16_t& out) {
    unsigned char b[2]{0, 0};
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    out = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
    return true;
}

static bool read_bytes(std::istream& in, std::string& out, std::size_t n) {
    out.assign(n, '\0');
    if (n == 0) return true;
    return (bool)in.read(out.data(), static_cast<std::streamsize>(n));
}

// Reads the stored expression token ("exprTok") from 1INX/2INX.
// 1INX layout: magic(4) ver(u16) nameLen(u16) name(bytes)
// 2INX layout: magic(4) ver(u16) keylen(u16) nameLen(u16) name(bytes) ...
static bool read_inx_expr_token(const fs::path& p, std::string& exprTokOut, uint16_t& verOut) {
    exprTokOut.clear();
    verOut = 0;

    std::ifstream in(p, std::ios::binary);
    if (!in) return false;

    char magic[4]{0, 0, 0, 0};
    if (!in.read(magic, 4)) return false;

    if (magic[0] == '1' && magic[1] == 'I' && magic[2] == 'N' && magic[3] == 'X') {
        uint16_t ver = 0, nameLen = 0;
        if (!rd_u16(in, ver)) return false;
        if (!rd_u16(in, nameLen)) return false;
        if (nameLen == 0) return false;

        std::string tok;
        if (!read_bytes(in, tok, nameLen)) return false;

        exprTokOut = tok;
        verOut = ver;
        return true;
    }

    if (magic[0] == '2' && magic[1] == 'I' && magic[2] == 'N' && magic[3] == 'X') {
        uint16_t ver = 0, keylen = 0, nameLen = 0;
        if (!rd_u16(in, ver)) return false;
        if (!rd_u16(in, keylen)) return false;
        (void)keylen;
        if (!rd_u16(in, nameLen)) return false;
        if (nameLen == 0) return false;

        std::string tok;
        if (!read_bytes(in, tok, nameLen)) return false;

        exprTokOut = tok;
        verOut = ver;
        return true;
    }

    return false;
}

static bool try_parse_hash_field_index(const std::string& tok, int& outField1) {
    outField1 = 0;
    if (tok.size() < 2) return false;
    if (tok[0] != '#') return false;

    long v = 0;
    for (size_t i = 1; i < tok.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(tok[i]);
        if (!std::isdigit(c)) return false;
        v = v * 10 + (tok[i] - '0');
        if (v > std::numeric_limits<int>::max()) return false;
    }
    if (v <= 0) return false;
    outField1 = static_cast<int>(v);
    return true;
}

static int resolve_field_index_by_name_ci(xbase::DbArea& A, const std::string& name) {
    try {
        const std::string want = up_copy(name);
        const auto defs = A.fields();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (up_copy(defs[i].name) == want) {
                return static_cast<int>(i) + 1; // 1-based
            }
        }
    } catch (...) {
    }
    return 0;
}

// FieldDef in your xbase.hpp does NOT expose len/dec, so we only fill name/type.
// len/dec remain 0.
static void fill_tag_field_meta_if_possible(xbase::DbArea& A, int field1, IndexSummary::Tag& row) {
    if (field1 <= 0) return;

    try {
        const auto defs = A.fields();
        const std::size_t idx0 = static_cast<std::size_t>(field1 - 1);
        if (idx0 >= defs.size()) return;

        row.fieldName = defs[idx0].name;
        row.type = std::string(1, defs[idx0].type);
        // row.len / row.dec intentionally left at 0
    } catch (...) {
    }
}

static bool tag_exists_ci(const std::vector<IndexSummary::Tag>& tags, const std::string& name) {
    const std::string want = up_copy(name);
    for (const auto& t : tags) {
        if (up_copy(t.tagName) == want) return true;
    }
    return false;
}

// ----- public --------------------------------------------------------------

IndexSummary dottalk::summarize_index(xbase::DbArea& A) {
    IndexSummary S;

    S.record_count = A.recCount();

    if (!orderstate::hasOrder(A)) {
        S.kind = IndexSummary::OrderKind::Physical;
        return S;
    }

    S.index_path = orderstate::orderName(A);

    if (ends_with_ext_ci(S.index_path, "CNX")) {
        S.container_type = "CNX";
    } else if (ends_with_ext_ci(S.index_path, "INX")) {
        S.container_type = "INX";
    } else if (ends_with_ext_ci(S.index_path, "CDX")) {
        S.container_type = "CDX";
    } else {
        S.container_type.clear();
    }

    S.kind = orderstate::isAscending(A)
        ? IndexSummary::OrderKind::Ascending
        : IndexSummary::OrderKind::Descending;

    S.active_tag = orderstate::activeTag(A);

    try {
        if (S.container_type == "CNX") {
            std::vector<cnxfile::TagInfo> td;
            cnxfile::CNXHandle* h = nullptr;
            if (cnxfile::open(S.index_path, h)) {
                if (cnxfile::read_tagdir(h, td)) {
                    S.tags.reserve(td.size());
                    for (const auto& t : td) {
                        IndexSummary::Tag row;
                        row.tagName = t.name;
                        row.asc = (S.kind != IndexSummary::OrderKind::Descending);

                        const int field1 = resolve_field_index_by_name_ci(A, row.tagName);
                        fill_tag_field_meta_if_possible(A, field1, row);

                        S.tags.push_back(std::move(row));
                    }
                }
                cnxfile::close(h);
            }

            if (S.active_tag.empty() && !S.tags.empty()) {
                S.active_tag = S.tags.front().tagName;
            }
        } else if (S.container_type == "INX") {
            std::string exprTok;
            uint16_t ver = 0;
            if (read_inx_expr_token(S.index_path, exprTok, ver)) {
                IndexSummary::Tag row;
                row.tagName = exprTok;
                row.asc = (S.kind != IndexSummary::OrderKind::Descending);

                int field1 = 0;
                if (try_parse_hash_field_index(exprTok, field1)) {
                    fill_tag_field_meta_if_possible(A, field1, row);
                } else {
                    field1 = resolve_field_index_by_name_ci(A, exprTok);
                    fill_tag_field_meta_if_possible(A, field1, row);
                }

                S.tags.push_back(std::move(row));

                if (S.active_tag.empty()) {
                    S.active_tag = exprTok;
                }
            }
        } else if (S.container_type == "CDX") {
            const auto* im = A.indexManagerPtr();
            if (im && im->hasBackend() && im->isCdx()) {
                const std::string active = !im->activeTag().empty() ? im->activeTag() : S.active_tag;

                if (!active.empty()) {
                    IndexSummary::Tag row;
                    row.tagName = active;
                    row.asc = (S.kind != IndexSummary::OrderKind::Descending);

                    const int field1 = resolve_field_index_by_name_ci(A, active);
                    fill_tag_field_meta_if_possible(A, field1, row);

                    if (!tag_exists_ci(S.tags, row.tagName)) {
                        S.tags.push_back(std::move(row));
                    }

                    if (S.active_tag.empty()) {
                        S.active_tag = active;
                    }
                }
            }

            if (S.tags.empty() && !S.active_tag.empty()) {
                IndexSummary::Tag row;
                row.tagName = S.active_tag;
                row.asc = (S.kind != IndexSummary::OrderKind::Descending);

                const int field1 = resolve_field_index_by_name_ci(A, S.active_tag);
                fill_tag_field_meta_if_possible(A, field1, row);

                S.tags.push_back(std::move(row));
            }
        }
    } catch (...) {
        // best-effort: ignore failures
    }

    return S;
}
// simple_index.cpp
#define _CRT_SECURE_NO_WARNINGS

#include "xindex/simple_index.hpp"
#include "xbase.hpp"
#include "textio.hpp"

// ⬅️ Use the SAME header include that cmd_display.cpp uses for RecordView.
// If that file includes "record_view.hpp", do the same here.
#include "record_view.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using xbase::DbArea;

namespace {

struct Token {
    enum Kind { Field, Literal } kind;
    int fieldId;               // valid when kind==Field
    std::string literal;       // valid when kind==Literal
};

inline std::string upper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static int resolve_field_id(const DbArea& A, std::string_view name) {
    std::string U = textio::up(textio::trim(std::string(name)));
    const auto& F = A.fields();
    for (size_t i = 0; i < F.size(); ++i) {
        if (textio::up(textio::trim(F[i].name)) == U) return (int)i;
    }
    return -1;
}

// Parse meta.expression into tokens: FIELD refs and quoted literals. Split on '+'.
static std::vector<Token> parse_tokens(const DbArea& A, const std::string& expr) {
    std::vector<Token> out;
    bool inQ = false; char q = 0;
    std::string cur;

    auto push_fieldish = [&](){
        auto t = textio::trim(cur);
        cur.clear();
        if (t.empty()) return;
        int id = resolve_field_id(A, t);
        if (id >= 0) out.push_back(Token{Token::Field, id, {}});
        else         out.push_back(Token{Token::Literal, -1, t});
    };

    for (char c : expr) {
        if (inQ) {
            cur.push_back(c);
            if (c == q) {
                if (cur.size() >= 2) {
                    std::string lit = cur.substr(1, cur.size()-2);
                    out.push_back(Token{Token::Literal, -1, lit});
                }
                cur.clear();
                inQ = false;
            }
        } else {
            if (c=='\'' || c=='"') { inQ = true; q = c; cur.assign(1, c); }
            else if (c=='+') { push_fieldish(); }
            else cur.push_back(c);
        }
    }
    if (!cur.empty()) push_fieldish();

    // Single identifier → treat as field if resolvable
    if (out.size()==1 && out[0].kind==Token::Literal) {
        int id = resolve_field_id(A, out[0].literal);
        if (id >= 0) out[0] = Token{Token::Field, id, {}};
    }

    // Drop whitespace-only literals
    std::vector<Token> filtered;
    filtered.reserve(out.size());
    for (auto& t : out) {
        if (t.kind == Token::Literal) {
            auto trimmed = textio::trim(t.literal);
            if (!trimmed.empty())
                filtered.push_back(Token{Token::Literal, -1, trimmed});
        } else filtered.push_back(t);
    }
    return filtered;
}

// Use the SAME formatting that LIST/DISPLAY uses via RecordView.
// Note: RecordView appears NOT to live in namespace xbase.
static std::string to_key_component(DbArea& A, RecordView& rv, int fid) {
    char t = static_cast<char>(
    std::toupper(static_cast<unsigned char>(A.fields()[fid].type))
    );
    switch (t) {
        case 'C': {
            std::string s = rv.getString(fid);        // right-trim if needed
            return upper(textio::rtrim(s));
        }
        case 'N': {
            std::string s = rv.getNumericString(fid); // same formatting as LIST
            return upper(s);
        }
        case 'D': {
            return rv.getDateYYYYMMDD(fid);           // canonical YYYYMMDD
        }
        case 'L': {
            return rv.getLogical(fid) ? "T" : "F";
        }
        default: {
            std::string s = rv.getString(fid);
            return upper(textio::rtrim(s));
        }
    }
}

struct Entry { std::string key; uint32_t recno; };

// Simple flat on-disk index to prove keys are real.
// Replace with your B-tree writer when ready.
static bool save_flat_index(const std::filesystem::path& path,
                            const std::string& expr,
                            bool ascending,
                            const std::vector<Entry>& entries,
                            std::string* err) {
    try {
        std::FILE* f = std::fopen(path.string().c_str(), "wb");
        if (!f) { if (err) *err = "open failed"; return false; }

        const char magic[5] = {'1','I','N','X','\0'};
        std::fwrite(magic, 1, 5, f);

        unsigned char asc = ascending ? 1u : 0u;
        std::fwrite(&asc, 1, 1, f);

        uint16_t elen = (uint16_t)std::min<size_t>(expr.size(), 0xFFFFu);
        std::fwrite(&elen, 1, sizeof(elen), f);
        if (elen) std::fwrite(expr.data(), 1, elen, f);

        uint32_t count = (uint32_t)entries.size();
        std::fwrite(&count, 1, sizeof(count), f);

        for (auto& e : entries) {
            uint16_t klen = (uint16_t)std::min<size_t>(e.key.size(), 0xFFFFu);
            std::fwrite(&klen, 1, sizeof(klen), f);
            if (klen) std::fwrite(e.key.data(), 1, klen, f);
            std::fwrite(&e.recno, 1, sizeof(e.recno), f);
        }
        std::fclose(f);
        return true;
    } catch (...) {
        if (err) *err = "exception writing";
        return false;
    }
}

} // namespace

namespace xindex {

// 🔁 Match header: const IndexMeta&
bool SimpleIndex::build_and_save(DbArea& A,
                                 const IndexMeta& meta,
                                 const std::filesystem::path& out,
                                 std::string* err)
{
    if (!A.isOpen()) { if (err) *err = "no table open"; return false; }

    // Tokenize expression into fields + literals
    auto tokens = parse_tokens(A, meta.expression);
    if (tokens.empty()) { if (err) *err = "empty/invalid expression"; return false; }

    std::vector<Entry> entries;
    entries.reserve((size_t)A.recCount());

    int cur = A.recno();
    const int n = A.recCount();

    for (int r = 1; r <= n; ++r) {
        A.gotoRec(r);
        if (A.isDeleted()) continue;       // ✅ no-arg version

        // Same view/rendering as LIST/DISPLAY
        RecordView rv(A);

        // Build the key from evaluated values (NOT field names)
        std::string key;
        key.reserve(64);
        bool first = true;
        for (const auto& t : tokens) {
            if (!first) key.push_back('\x1F'); // composite separator
            first = false;

            if (t.kind == Token::Field) {
                key += to_key_component(A, rv, t.fieldId);
            } else {
                key += upper(t.literal);
            }
        }

        entries.push_back(Entry{ std::move(key), (uint32_t)r });
    }

    if (cur != A.recno()) A.gotoRec(cur);

    // Sort asc; reverse for DESC
    std::stable_sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b){
            if (a.key < b.key) return true;
            if (a.key > b.key) return false;
            return a.recno < b.recno;
        });
    if (!meta.ascending) std::reverse(entries.begin(), entries.end());

    // Persist (flat writer)
    if (!save_flat_index(out, textio::up(meta.expression), meta.ascending, entries, err))
        return false;

    return true;
}

} // namespace xindex

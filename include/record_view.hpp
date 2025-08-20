#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include "xbase.hpp"

// Lightweight, header-only view that formats field values the same way
// LIST/DISPLAY would present them (string, numeric, date, logical).
//
// IMPORTANT: All getters take a 0-based field index (fid0), because most of
// your code (e.g., cmd_index.cpp) uses 0-based indices. Internally we call
// DbArea::get(1-based).

class RecordView {
public:
    explicit RecordView(xbase::DbArea& area) : a(area) {}

    // Character fields: return right-trimmed string (DBF stores right-padded)
    std::string getString(int fid0) const {
        std::string s = a.get(fid0 + 1);   // DbArea::get is 1-based
        rtrim(s);
        return s;
    }

    // Numeric fields: trim both sides; keep whatever formatting the DBF has
    // (your LIST/DISPLAY likely just prints the stored text without padding).
    std::string getNumericString(int fid0) const {
        std::string s = a.get(fid0 + 1);
        trim(s);
        return s;
    }

    // Date fields: DBF canonical "YYYYMMDD" as text. We return the first 8.
    std::string getDateYYYYMMDD(int fid0) const {
        std::string s = a.get(fid0 + 1);
        // Some sources might be blank; just return as-is or truncate to 8
        if (s.size() > 8) s.resize(8);
        return s;
    }

    // Logical fields: map T/Y => true, F/N => false, others => false.
    bool getLogical(int fid0) const {
        std::string s = a.get(fid0 + 1);
        if (s.empty()) return false;
        unsigned char c = static_cast<unsigned char>(s[0]);
        c = static_cast<unsigned char>(std::toupper(c));
        return (c == 'T' || c == 'Y');
    }

private:
    static void ltrim(std::string& s) {
        auto it = std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); });
        s.erase(s.begin(), it);
    }
    static void rtrim(std::string& s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    }
    static void trim(std::string& s) { ltrim(s); rtrim(s); }

    xbase::DbArea& a;
};

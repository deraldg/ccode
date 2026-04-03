#include "dli/replace_api.hpp"

#include <sstream>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

namespace xbase { class DbArea; }

// We call the *existing* command handler to guarantee indexes/memo hooks fire.
// Provide this from your current build (already present alongside other dli command handlers).
extern void cmd_REPLACE(xbase::DbArea& db, std::istringstream& args);

namespace dli {

static std::string quote_and_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

bool do_replace_text(xbase::DbArea& db,
                     const std::string& fieldName,
                     const std::string& rawUserText,
                     std::string* errorOut) {
    try {
        // We conservatively quote the raw user text; your REPLACE handler already
        // converts/types according to the field definition (N/D/L/C, etc.).
        std::ostringstream oss;
        oss << fieldName << " WITH " << quote_and_escape(rawUserText);
        std::istringstream iss(oss.str());
        cmd_REPLACE(db, iss);
        return true;
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = ex.what();
        return false;
    } catch (...) {
        if (errorOut) *errorOut = "Unknown error in REPLACE.";
        return false;
    }
}

static fs::path write_temp_memo(const std::string& text) {
    auto dir = fs::temp_directory_path();
    // Naive unique name using time since epoch + pid-ish randomness
    auto ts  = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path p = dir / ("dli_memo_" + std::to_string(ts) + ".txt");
    std::ofstream ofs(p, std::ios::binary);
    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
    ofs.close();
    return p;
}

bool do_replace_memo_text(xbase::DbArea& db,
                          const std::string& fieldName,
                          const std::string& memoText,
                          std::string* errorOut) {
    try {
        // Use FILE path route to avoid escaping issues for long/complex memo contents.
        fs::path tmp = write_temp_memo(memoText);
        std::ostringstream oss;
        oss << fieldName << " WITH FILE " << quote_and_escape(tmp.string());
        std::istringstream iss(oss.str());
        cmd_REPLACE(db, iss);
        // Optionally delete temp; if your REPLACE reads immediately, safe to remove now
        std::error_code ec;
        fs::remove(tmp, ec);
        return true;
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = ex.what();
        return false;
    } catch (...) {
        if (errorOut) *errorOut = "Unknown error in REPLACE (memo path).";
        return false;
    }
}

} // namespace dli




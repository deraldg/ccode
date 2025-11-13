#include "dtx/glue.hpp"
#include "xbase.hpp"
#include <filesystem>
#include <fstream>
#include <cctype>

using namespace xbase;
namespace fs = std::filesystem;

namespace {

// case-insensitive equality for field names
bool iequals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    unsigned char ca = static_cast<unsigned char>(a[i]);
    unsigned char cb = static_cast<unsigned char>(b[i]);
    if (std::tolower(ca) != std::tolower(cb)) return false;
  }
  return true;
}

void csv_emit(std::ofstream& out, const std::string& v) {
  const bool needs = v.find_first_of(",\"\r\n") != std::string::npos;
  if (!needs) { out << v; return; }
  out << '"';
  for (char c : v) out << (c == '"' ? "\"\"" : std::string(1, c));
  out << '"';
}

} // namespace

namespace dtx {

void open_table(Session& s, const std::string& table_name) {
  if (!s.area) s.area = new DbArea();
  const fs::path p = fs::path(s.workdir) / (table_name + ".dbf");
  // DbArea::open throws on error; let exceptions propagate to the caller.
  s.area->open(p.string());
}

void close_table(Session& s) {
  if (!s.area) return;
  if (s.area->isOpen()) s.area->close();
  delete s.area;
  s.area = nullptr;
}

bool is_open(const Session& s) {
  return s.area && s.area->isOpen();
}

std::vector<FieldDef> schema(const Session& s) {
  if (!is_open(s)) return {};
  return s.area->fields();
}

size_t rec_count(const Session& s) {
  return is_open(s) ? static_cast<size_t>(s.area->recCount()) : 0U;
}

bool export_csv(const Session& s,
                const std::string& /*table_name*/,
                const std::vector<std::string>& fields,
                const std::optional<std::string>& /*for_clause*/,
                const std::string& out_path)
{
  if (!is_open(s)) return false;

  std::ofstream out(out_path, std::ios::binary);
  if (!out) return false;

  // header
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i) out << ',';
    out << fields[i];
  }
  out << "\r\n";

  // map requested names -> 1-based indices
  const auto& fdefs = s.area->fields();
  std::vector<int> col_idx; col_idx.reserve(fields.size());
  for (const auto& name : fields) {
    int idx1 = 0;
    for (size_t j = 0; j < fdefs.size(); ++j) {
      if (iequals(fdefs[j].name, name)) { idx1 = static_cast<int>(j + 1); break; }
    }
    col_idx.push_back(idx1); // 0 => not found => write empty
  }

  const int32_t total = s.area->recCount();
  for (int32_t n = 1; n <= total; ++n) {
    if (!s.area->gotoRec(n))        continue;
    if (!s.area->readCurrent())     continue;
    if (s.area->isDeleted())        continue; // skip logically deleted rows

    for (size_t c = 0; c < col_idx.size(); ++c) {
      if (c) out << ',';
      const int i1 = col_idx[c];
      const std::string v = (i1 > 0) ? s.area->get(i1) : std::string();
      csv_emit(out, v);
    }
    out << "\r\n";
  }
  return true;
}

bool run_commands(Session&, const std::string&, const std::vector<std::string>&, std::string&) {
  // No native dispatcher yet.
  return false;
}

} // namespace dtx

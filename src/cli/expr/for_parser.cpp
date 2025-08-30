#include "dottalk/expr/for_parser.hpp"
#include <cctype>

using namespace dottalk::expr;

static std::string up(std::string s) {
  for (auto& c: s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
static std::string ltrim(std::string s) {
  size_t i=0; while (i<s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  return s.substr(i);
}

bool extract_for_clause(std::istringstream& iss, std::string& out) {
  auto pos = iss.tellg();
  std::string first;
  if (!(iss >> first)) { iss.clear(); iss.seekg(pos); return false; }
  if (up(first) != "FOR") { iss.clear(); iss.seekg(pos); return false; }
  std::string rest;
  std::getline(iss, rest);
  out = ltrim(rest);
  return true;
}

// src/bindings/pydottalk.cpp
// Minimal pybind11 module exposing a CLI-backed Session (Windows-focused).
// Build with: -DBUILD_PYDOT_TALK=ON -DDTX_BACKEND_NATIVE=OFF

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <chrono>

namespace py = pybind11;
namespace fs = std::filesystem;

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
#endif

// -------------------- Windows CreateProcessW helper -------------------------
#if defined(_WIN32)
static std::string run_batch_cli(const std::string& exe,
                                 const std::string& workdir,
                                 const std::vector<std::string>& commands,
                                 double timeout_sec)
{
  SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
  HANDLE out_rd = NULL, out_wr = NULL, in_rd = NULL, in_wr = NULL;

  if (!CreatePipe(&out_rd, &out_wr, &sa, 0)) throw std::runtime_error("CreatePipe stdout failed");
  if (!SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0)) throw std::runtime_error("SetHandleInformation stdout failed");
  if (!CreatePipe(&in_rd, &in_wr, &sa, 0)) throw std::runtime_error("CreatePipe stdin failed");
  if (!SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0)) throw std::runtime_error("SetHandleInformation stdin failed");

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.hStdError  = out_wr;
  si.hStdOutput = out_wr;
  si.hStdInput  = in_rd;
  si.dwFlags   |= STARTF_USESTDHANDLES;

  PROCESS_INFORMATION pi{};
  std::wstring wexe(exe.begin(), exe.end());
  std::wstring wdir(workdir.begin(), workdir.end());
  std::wstring cmdline = L"\"" + wexe + L"\"";

  if (!CreateProcessW(
        NULL, (LPWSTR)cmdline.c_str(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL,
        workdir.empty() ? NULL : wdir.c_str(),
        &si, &pi))
  {
    CloseHandle(out_rd); CloseHandle(out_wr);
    CloseHandle(in_rd);  CloseHandle(in_wr);
    throw std::runtime_error("CreateProcessW failed");
  }

  // Build script: commands + QUIT
  std::string script;
  for (auto& c : commands) { script += c; script += "\n"; }
  script += "QUIT\n";

  // Send script, close child's stdin to signal EOF
  DWORD written = 0;
  if (!WriteFile(in_wr, script.data(), (DWORD)script.size(), &written, NULL)) {
    // continue; child may still emit diagnostics
  }
  CloseHandle(in_wr);

  // Drain output until process exits or timeout
  std::string out;
  out.reserve(4096);
  auto deadline = std::chrono::steady_clock::now()
                + std::chrono::milliseconds((int)(timeout_sec * 1000));

  char buf[4096];
  DWORD avail = 0, read = 0;
  for (;;) {
    while (PeekNamedPipe(out_rd, NULL, 0, NULL, &avail, NULL) && avail > 0) {
      if (!ReadFile(out_rd, buf, (DWORD)std::min<DWORD>(avail, sizeof(buf)), &read, NULL) || read == 0) break;
      out.append(buf, buf + read);
    }
    DWORD code = 0;
    if (GetExitCodeProcess(pi.hProcess, &code) && code != STILL_ACTIVE) {
      // final drain
      while (PeekNamedPipe(out_rd, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        if (!ReadFile(out_rd, buf, (DWORD)std::min<DWORD>(avail, sizeof(buf)), &read, NULL) || read == 0) break;
        out.append(buf, buf + read);
      }
      break;
    }
    if (std::chrono::steady_clock::now() > deadline) {
      TerminateProcess(pi.hProcess, 1);
      break;
    }
    Sleep(10);
  }

  CloseHandle(out_wr);
  CloseHandle(out_rd);
  CloseHandle(in_rd);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return out;
}
#else
static std::string run_batch_cli(const std::string& exe,
                                 const std::string& workdir,
                                 const std::vector<std::string>& commands,
                                 double /*timeout_sec*/)
{
  (void)workdir;
  // Portable fallback: write commands to a temp script and system() it.
  std::ostringstream os;
  for (auto& c : commands) os << c << "\n";
  os << "QUIT\n";
  std::string script = os.str();
  fs::path p = fs::temp_directory_path() / "dtx_script.txt";
  {
    std::ofstream f(p.string(), std::ios::binary);
    f.write(script.data(), (std::streamsize)script.size());
  }
  std::string cmd = "\"" + exe + "\" < \"" + p.string() + "\" > \"" + (p.string() + ".out") + "\" 2>&1";
  int rc = std::system(cmd.c_str());
     (void)rc;

  std::ifstream fin((p.string() + ".out"), std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(fin)), {});
}
#endif

static std::string newest_csv(const std::string& dir) {
  namespace fs = std::filesystem;
  fs::path best;
  fs::file_time_type best_time{};
  bool have = false;

  for (const auto& e : fs::directory_iterator(dir)) {
    if (!e.is_regular_file()) continue;
    if (e.path().extension() != ".csv") continue;
    auto t = e.last_write_time();
    if (!have || t > best_time) {
      best_time = t;
      best = e.path();
      have = true;
    }
  }
  return have ? best.string() : std::string{};
}

// -------------------- Python-facing class -----------------------------------
class Session {
public:
  Session(std::string workdir,
          std::optional<std::string> exe,
          double timeout_sec)
  : workdir_(std::move(workdir)), exe_(exe.value_or("")), timeout_(timeout_sec) {}

  std::string export_csv(const std::string& table,
                         std::optional<std::vector<std::string>> fields,
                         std::optional<std::string> for_clause,
                         std::optional<std::string> out_path)
  {
    if (exe_.empty()) throw std::runtime_error("CLI backend requires exe path");
    std::ostringstream cmd;
    if (fields && !fields->empty()) {
      cmd << "EXPORT FIELDS ";
      for (size_t i = 0; i < fields->size(); ++i) { if (i) cmd << ","; cmd << (*fields)[i]; }
      if (for_clause) cmd << " FOR " << *for_clause;
      if (out_path)   cmd << " TO "  << *out_path;
    } else {
      cmd << "EXPORT";
      if (for_clause) cmd << " FOR " << *for_clause;
      if (out_path)   cmd << " TO "  << *out_path;
    }
    (void)run_batch_cli(exe_, workdir_, { "USE " + table, cmd.str() }, timeout_);
    return newest_csv(workdir_);
  }

  std::string fields(const std::string& table) {
    if (exe_.empty()) throw std::runtime_error("CLI backend requires exe path");
    return run_batch_cli(exe_, workdir_, { "USE " + table, "FIELDS" }, timeout_);
  }

  std::string run(const std::string& table, const std::vector<std::string>& commands) {
    if (exe_.empty()) throw std::runtime_error("CLI backend requires exe path");
    std::vector<std::string> script; script.reserve(commands.size() + 1);
    script.push_back("USE " + table);
    script.insert(script.end(), commands.begin(), commands.end());
    return run_batch_cli(exe_, workdir_, script, timeout_);
  }

private:
  std::string workdir_;
  std::string exe_;
  double      timeout_{6.0};
};

PYBIND11_MODULE(pydottalk, m) {
  py::class_<Session>(m, "Session")
    .def(py::init<std::string, std::optional<std::string>, double>(),
         py::arg("workdir"),
         py::arg("exe") = std::nullopt,
         py::arg("timeout_sec") = 6.0)
    .def("export_csv", &Session::export_csv,
         py::arg("table"),
         py::arg("fields") = std::nullopt,
         py::arg("for_clause") = std::nullopt,
         py::arg("out_path") = std::nullopt)
    .def("fields", &Session::fields, py::arg("table"))
    .def("run", &Session::run, py::arg("table"), py::arg("commands"));
}


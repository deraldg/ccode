#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <memory>

namespace py = pybind11;
namespace fs = std::filesystem;

static std::string pydottalk_version()
{
    return "pydottalk 0.2.0";
}

static std::string ping(const std::string& name)
{
    return "hello, " + name;
}

// --------------------- DBF wrapper (placeholder) ---------------------
// This wrapper compiles even without xbase. When HAVE_XBASE is defined
// and your headers are present, replace the stub internals with real calls.

struct DbfImpl {
    fs::path path;
    bool open = false;
    size_t record_count = 0;
    std::vector<std::string> field_names;

#if defined(HAVE_XBASE)
    // TODO: include your real xbase headers and add concrete members, e.g.:
    // #if __has_include(<xbase/dbf.hpp>)
    //   #include <xbase/dbf.hpp>
    //   xbase::Dbf dbf;
    // #endif
#endif
};

class Dbf {
public:
    Dbf() : p_(std::make_shared<DbfImpl>()) {}

    void open(const std::string& path) {
        p_->path = fs::u8path(path);
#if defined(HAVE_XBASE)
        // ---- Real implementation goes here ----
        // Example sketch:
        // p_->dbf.open(p_->path.string().c_str());
        // p_->record_count = p_->dbf.record_count();
        // p_->field_names = p_->dbf.field_names();
        // p_->open = true;
        // ---------------------------------------
        // For now, fail fast to make it obvious when xbase is linked but not wired:
        throw std::runtime_error("pydottalk was built with HAVE_XBASE, but the binding is not wired yet. Replace the stub with calls into xbase.");
#else
        // No xbase linked — keep behavior explicit:
        throw std::runtime_error("pydottalk built without xbase support; cannot open DBF.");
#endif
    }

    bool is_open() const { return p_->open; }

    std::string path() const {
        return p_->path.string();
    }

    std::size_t record_count() const {
        ensure_open("record_count");
        return p_->record_count;
    }

    std::vector<std::string> fields() const {
        ensure_open("fields");
        return p_->field_names;
    }

    void close() {
#if defined(HAVE_XBASE)
        // if (p_->open) p_->dbf.close();
#endif
        p_->open = false;
        p_->record_count = 0;
        p_->field_names.clear();
    }

private:
    void ensure_open(const char* what) const {
        if (!p_->open) {
            throw std::runtime_error(std::string("DBF not open; cannot call ") + what);
        }
    }

    std::shared_ptr<DbfImpl> p_;
};

// Utility: list *.dbf under a directory (non-recursive)
static std::vector<std::string> list_dbf(const std::string& dir) {
    std::vector<std::string> out;
    fs::path base = fs::u8path(dir);
    if (!fs::exists(base) || !fs::is_directory(base)) return out;
    for (auto& e : fs::directory_iterator(base)) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".dbf") out.push_back(e.path().string());
    }
    return out;
}

PYBIND11_MODULE(pydottalk, m)
{
    m.doc() = "DotTalk++ Python bindings (scaffold)\n"
              "This build may not be linked to xbase yet; DBF operations will raise.";

    // Module & API versions
    m.def("version", &pydottalk_version, "Return pydottalk version string");
    m.def("ping", &ping, py::arg("name") = "world", "Say hello");

    // Utility
    m.def("list_dbf", &list_dbf, py::arg("directory"),
          "List .dbf files in a directory (non-recursive).");

    // Dbf class
    py::class_<Dbf>(m, "Dbf")
        .def(py::init<>())
        .def("open", &Dbf::open, py::arg("path"),
             "Open a DBF file. Requires build with xbase; otherwise raises.")
        .def("is_open", &Dbf::is_open)
        .def_property_readonly("path", &Dbf::path)
        .def("record_count", &Dbf::record_count,
             "Return number of records (requires open).")
        .def("fields", &Dbf::fields,
             "Return list of field names (requires open).")
        .def("close", &Dbf::close)
        // Pythonic context manager: with pydottalk.Dbf() as db: ...
        .def("__enter__", [](Dbf& self) -> Dbf& { return self; })
        .def("__exit__", [](Dbf& self, py::object, py::object, py::object) { self.close(); });
}

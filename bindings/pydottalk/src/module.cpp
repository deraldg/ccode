#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace py = pybind11;
namespace fs = std::filesystem;

// =============================================================
// CRITICAL INCLUDE ORDER (MSVC)
// -------------------------------------------------------------
// xbase.hpp forward-declares xindex::IndexManager, but DbArea has
// unique_ptr<IndexManager> AND defaulted move ops inline:
//   DbArea(DbArea&&) = default;
//   DbArea& operator=(DbArea&&) = default;
//
// MSVC will require IndexManager COMPLETE in any TU that parses
// those inline definitions (pybind11 tends to trigger this).
//
// Therefore: include the header that DEFINES xindex::IndexManager
// BEFORE including xbase.hpp.
// =============================================================

#if defined(HAVE_XBASE)
  // Try common locations first. If your project uses a different header name,
  // add it here (or see the findstr command below).
  #if __has_include("xindex/index_manager.hpp")
    #include "xindex/index_manager.hpp"
  #elif __has_include("xindex/index_manager.h")
    #include "xindex/index_manager.h"
  #elif __has_include("xindex/IndexManager.hpp")
    #include "xindex/IndexManager.hpp"
  #elif __has_include("xindex/IndexManager.h")
    #include "xindex/IndexManager.h"
  #elif __has_include("xindex.hpp")
    #include "xindex.hpp"
  #else
    #error "pydottalk: cannot find a header that DEFINES xindex::IndexManager. Add the correct include above."
  #endif

  #include "xbase.hpp"
#endif

static std::string pydottalk_version()
{
    return "pydottalk 0.3.0";
}

static std::string ping(const std::string& name)
{
    return "hello, " + name;
}

static std::vector<std::string> list_dbf(const std::string& dir)
{
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

#if defined(HAVE_XBASE)
static std::string resolve_abs_path(const std::string& path)
{
    fs::path p = fs::u8path(path);
    return fs::absolute(p).lexically_normal().string();
}

static py::list fields_as_dict_list(const xbase::DbArea& A)
{
    py::list out;
    const auto& defs = A.fields();
    for (const auto& f : defs) {
        py::dict d;
        d["name"]     = f.name;
        d["type"]     = std::string(1, f.type);
        d["length"]   = static_cast<int>(f.length);
        d["decimals"] = static_cast<int>(f.decimals);
        out.append(d);
    }
    return out;
}
#endif

PYBIND11_MODULE(pydottalk, m)
{
    m.doc() = "DotTalk++ Python bindings (canonical: xbase::DbArea).";

    m.def("version", &pydottalk_version);
    m.def("ping", &ping, py::arg("name") = "world");
    m.def("list_dbf", &list_dbf, py::arg("directory"));

#if !defined(HAVE_XBASE)
    m.attr("HAVE_XBASE") = py::bool_(false);
    return;
#else
    m.attr("HAVE_XBASE") = py::bool_(true);

    py::module_ mx = m.def_submodule("xbase", "xbase engine bindings (DbArea is canonical).");

    py::enum_<xbase::DbArea::MemoKind>(mx, "MemoKind")
        .value("NONE", xbase::DbArea::MemoKind::NONE)
        .value("FPT",  xbase::DbArea::MemoKind::FPT)
        .value("DBT",  xbase::DbArea::MemoKind::DBT)
        .export_values();

    py::class_<xbase::DbArea>(mx, "DbArea")
        .def(py::init<>())

        // lifecycle
        .def("open", [](xbase::DbArea& self, const std::string& path) {
            self.open(resolve_abs_path(path));
        }, py::arg("path"))
        .def("close", &xbase::DbArea::close)
        .def("isOpen", &xbase::DbArea::isOpen)
        .def("is_open", &xbase::DbArea::isOpen)

        // deleted flag
        .def("isDeleted", &xbase::DbArea::isDeleted)
        .def("is_deleted", &xbase::DbArea::isDeleted)

        // navigation
        .def("gotoRec", &xbase::DbArea::gotoRec, py::arg("recno"))
        .def("goto_rec", &xbase::DbArea::gotoRec, py::arg("recno"))
        .def("top", &xbase::DbArea::top)
        .def("bottom", &xbase::DbArea::bottom)
        .def("skip", &xbase::DbArea::skip, py::arg("delta"))

        // record I/O
        .def("readCurrent", &xbase::DbArea::readCurrent)
        .def("read_current", &xbase::DbArea::readCurrent)
        .def("writeCurrent", &xbase::DbArea::writeCurrent)
        .def("write_current", &xbase::DbArea::writeCurrent)
        .def("appendBlank", &xbase::DbArea::appendBlank)
        .def("append_blank", &xbase::DbArea::appendBlank)
        .def("deleteCurrent", &xbase::DbArea::deleteCurrent)
        .def("delete_current", &xbase::DbArea::deleteCurrent)

        // schema/fields
        .def("fields", [](const xbase::DbArea& self) { return fields_as_dict_list(self); })
        .def("fieldCount", &xbase::DbArea::fieldCount)
        .def("field_count", &xbase::DbArea::fieldCount)
        .def("get", &xbase::DbArea::get, py::arg("idx1"))
        .def("set", &xbase::DbArea::set, py::arg("idx1"), py::arg("value"))

        // info
        .def("recno", &xbase::DbArea::recno)
        .def("recCount", &xbase::DbArea::recCount)
        .def("rec_count", &xbase::DbArea::recCount)
        .def("bof", &xbase::DbArea::bof)
        .def("eof", &xbase::DbArea::eof)
        .def("recLength", &xbase::DbArea::recLength)
        .def("rec_length", &xbase::DbArea::recLength)
        .def("recordLength", &xbase::DbArea::recordLength)
        .def("cpr", &xbase::DbArea::cpr)

        // canonical identity
        .def("filename", &xbase::DbArea::filename)
        .def("dbfDir", &xbase::DbArea::dbfDir)
        .def("dbf_dir", &xbase::DbArea::dbfDir)
        .def("dbfBasename", &xbase::DbArea::dbfBasename)
        .def("dbf_basename", &xbase::DbArea::dbfBasename)
        .def("logicalName", &xbase::DbArea::logicalName)
        .def("logical_name", &xbase::DbArea::logicalName)
        .def("name", &xbase::DbArea::name)

        // memo facts
        .def("memoPath", &xbase::DbArea::memoPath)
        .def("memo_path", &xbase::DbArea::memoPath)
        .def("memoKind", &xbase::DbArea::memoKind)
        .def("memo_kind", &xbase::DbArea::memoKind);

    // convenience alias
    m.attr("Dbf") = mx.attr("DbArea");
#endif
}

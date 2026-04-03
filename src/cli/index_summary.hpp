#pragma once
#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace dottalk {

struct IndexSummary {
    enum class OrderKind { Physical, Ascending, Descending };

    // high-level
    OrderKind   kind { OrderKind::Physical };
    std::string index_path;       // full path or empty
    std::string container_type;   // "CNX", "INX", or ""
    std::string active_tag;       // may be empty
    int         record_count {0};

    // optional tag rows (best-effort)
    struct Tag {
        std::string tagName;      // tag name in container (or field name fallback)
        std::string fieldName;    // if known; else may be empty
        std::string type;         // "C","N","D"... if known
        int         len {0};
        int         dec {0};
        bool        asc {true};
    };
    std::vector<Tag> tags;
};

// Build a summary from the currently attached order on area A.
// Never throws; returns a best-effort object.
IndexSummary summarize_index(xbase::DbArea& A);

} // namespace dottalk




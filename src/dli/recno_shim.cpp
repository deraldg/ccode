// src/dli/recno_shim.cpp ? provides dli_current_recno for browse_edit.cpp
#include "xbase.hpp"

// Global (not in namespace) to match the 'extern' in browse_edit.cpp
long long dli_current_recno(xbase::DbArea& db) {
    return static_cast<long long>(db.recno());
}




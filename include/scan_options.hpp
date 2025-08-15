#pragma once
#include <optional>
#include <string>

struct ScanOptions {
    enum class DeleteMode { SkipDeleted, IncludeDeleted, OnlyDeleted };
    DeleteMode del_mode = DeleteMode::SkipDeleted;

    std::optional<std::string> for_expr;    // FOR <expr>
    std::optional<std::string> while_expr;  // WHILE <expr>

    enum class Range { AllFromCurrent, NextN, RecordN, Rest };
    Range range = Range::AllFromCurrent;
    int   n = 0; // for NEXT/RECORD

    std::string usageVerb;
};

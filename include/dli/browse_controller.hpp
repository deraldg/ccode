#pragma once
#include "dli/browsetui_integration.hpp"
#include "dli/browse_edit.hpp"
#include "dli/screen.hpp"
#include <vector>
#include <string>

namespace xbase { class DbArea; }

namespace dli {

enum class BrowseMode {
    Form,   // Single record, field: value layout (like your screenshot)
    Grid    // Multi-row tabular BROWSE view
};

class BrowseController {
public:
    explicit BrowseController(xbase::DbArea& db);

    void run();

private:
    void switch_mode(BrowseMode newMode);
    void redraw();
    void handle_key(int vk);           // Windows virtual key code

    // Form View helpers
    void draw_form_view();
    void draw_form_border();
    void refresh_current_record_form();

    // Grid View helpers
    void draw_grid_view();
    void refresh_current_record_grid();

    // Editing
    void start_edit(const std::string& fieldName);
    void commit_edit();
    void cancel_edit();

    xbase::DbArea& m_db;
    BrowsePaint m_gridPaint;           // Used only in Grid mode
    BrowseMode m_mode = BrowseMode::Form;

    EditSession m_editSession;
    bool m_running = true;
    bool m_inEdit = false;
    std::string m_currentEditingField;

    // Cached current record data for Form view
    std::vector<std::pair<std::string, std::string>> m_currentFields; // fieldName, value
    int64_t m_currentRecno = 0;
};

} // namespace dli
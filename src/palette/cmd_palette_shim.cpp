      #include <sstream>
      namespace xbase { class DbArea; }
      void cmd_FOX_PALETTE(xbase::DbArea&, std::istringstream&);
      extern "C" void cmd_PALETTE(xbase::DbArea& area, std::istringstream& args) {
          static bool warned = false; // avoid spam on loops
          if (!warned) { /* optional: log deprecation */ warned = true; }
          cmd_FOX_PALETTE(area, args);
      }
    
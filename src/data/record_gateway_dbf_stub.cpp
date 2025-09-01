#include "data/irecord_gateway.hpp"
#include <iostream>
namespace browse {
class DbfGatewayStub final : public IRecordGateway {
public:
    int total_rows() override { return 0; }
    std::vector<std::string> column_names() override { return {}; }
    std::vector<GatewayRow> load_page(const PageRequest& req) override { (void)req; return {}; }
    bool update_record(std::uint32_t recno, const std::unordered_map<std::string,std::string>& vals) override {
        (void)recno; (void)vals; std::cerr << "[dbf_gateway_stub] update_record (noop)\n"; return false; }
};
IRecordGateway* create_dbf_gateway_stub(){ return new DbfGatewayStub(); }
} // namespace browse

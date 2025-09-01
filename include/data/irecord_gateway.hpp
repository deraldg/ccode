#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
namespace browse {
struct GatewayRow { std::uint32_t recno{0}; std::vector<std::string> fields; };
struct PageRequest { int offset{0}; int count{0}; };
class IRecordGateway {
public:
    virtual ~IRecordGateway() = default;
    virtual int total_rows() = 0;
    virtual std::vector<std::string> column_names() = 0;
    virtual std::vector<GatewayRow> load_page(const PageRequest& req) = 0;
    virtual bool update_record(std::uint32_t recno, const std::unordered_map<std::string,std::string>& newValues) = 0;
};
IRecordGateway* create_dbf_gateway_stub();
} // namespace browse

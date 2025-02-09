#pragma once

#include <gmock/gmock.h>

#include <blackhole/logger.hpp>
#include <blackhole/scoped.hpp>

namespace blackhole {
namespace testing {
namespace mock {

class logger_t : public ::blackhole::logger_t {
public:
    logger_t();
    ~logger_t();

    MOCK_METHOD2(log, void(int, string_view));
    MOCK_METHOD3(log, void(int, string_view, attribute_pack&));
    MOCK_METHOD4(log, void(int, string_view, attribute_pack&, const format_t&));

    MOCK_METHOD1(_scoped, std::function<scoped_t()>(attributes_t));
    auto scoped(attributes_t attributes) -> scoped_t;
};

}  // namespace mock
}  // namespace testing
}  // namespace blackhole

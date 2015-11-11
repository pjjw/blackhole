#include <gtest/gtest.h>

#include <blackhole/record.hpp>

namespace blackhole {
namespace testing {

TEST(Record, Severity) {
    attribute_pack pack{};

    record_t record(42, "GET /porn.png HTTP/1.1", pack);

    EXPECT_EQ(42, record.severity());
}

TEST(Record, Message) {
    attribute_pack pack{};

    record_t record(42, "GET /porn.png HTTP/1.1", pack);

    EXPECT_EQ("GET /porn.png HTTP/1.1", record.message().to_string());
}

TEST(Record, Attributes) {
    const view_of<attributes_t>::type attributes{{"key#1", {42}}};
    attribute_pack pack{attributes};

    record_t record(42, "GET /porn.png HTTP/1.1", pack);

    ASSERT_EQ(1, record.attributes().size());
    EXPECT_EQ(attributes, record.attributes().at(0));
}

TEST(Record, Pid) {
    attribute_pack pack;

    record_t record(42, "GET /porn.png HTTP/1.1", pack);

    EXPECT_EQ(::getpid(), record.pid());
}

TEST(Record, Tid) {
    attribute_pack pack;

    record_t record(42, "GET /porn.png HTTP/1.1", pack);

    // TODO: Fail for now, need to check platform dependent behavior.
    // EXPECT_EQ(std::this_thread::get_id(), record.tid())
}

TEST(Record, Timestamp) {
    attribute_pack pack;

    const auto min = std::chrono::high_resolution_clock::now();
    record_t record(42, "GET /porn.png HTTP/1.1", pack);
    const auto max = std::chrono::high_resolution_clock::now();

    EXPECT_TRUE(min <= record.timestamp());
    EXPECT_TRUE(max >= record.timestamp());
}

}  // namespace testing
}  // namespace blackhole

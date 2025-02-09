#ifdef __linux__

#include "blackhole/detail/datetime/linux/generator.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/variant.hpp>

#include "blackhole/extensions/format.hpp"

namespace blackhole {
namespace detail {
namespace datetime {

namespace {

typedef fmt::MemoryWriter writer_type;

}  // namespace

namespace {

struct visitor_t : public boost::static_visitor<> {
    fmt::MemoryWriter& stream;
    const std::tm& tm;
    std::uint64_t usec;
    char buffer[1024];

    visitor_t(fmt::MemoryWriter& stream, const std::tm& tm, std::uint64_t usec) :
        stream(stream),
        tm(tm),
        usec(usec)
    {}

    auto operator()(const literal_t& value) -> void {
        std::size_t ret = std::strftime(buffer, sizeof(buffer), value.value.c_str(), &tm);
        stream << fmt::StringRef(buffer, ret);
    }

    auto operator()(usecond_t) -> void {
        stream.write("{:06d}", usec);
    }
};

}  // namespace

generator_t::generator_t(std::string pattern) {
    std::string literal;

    auto pos = std::begin(pattern);
    while (pos != std::end(pattern)) {
        if (boost::starts_with(boost::make_iterator_range(pos, std::end(pattern)), "%f")) {
            tokens.emplace_back(literal_t{std::move(literal)});
            tokens.emplace_back(usecond_t{});
            literal.clear();
            ++pos;
            ++pos;
        } else {
            literal.push_back(*pos);
            ++pos;
        }
    }

    if (!literal.empty()) {
        tokens.emplace_back(literal_t{std::move(literal)});
    }
}

generator_t::~generator_t() {}

template<typename Stream>
auto
generator_t::operator()(Stream& stream, const std::tm& tm, std::uint64_t usec) const -> void {
    visitor_t visitor(stream, tm, usec);

    for (const auto& token : tokens) {
        boost::apply_visitor(visitor, token);
    }
}

auto make_generator(const std::string& pattern) -> generator_t {
    return generator_t(pattern);
}

template
auto generator_t::operator()<writer_type>(writer_type&, const std::tm&, std::uint64_t) const -> void;

}  // namespace datetime
}  // namespace detail
}  // namespace blackhole

#endif

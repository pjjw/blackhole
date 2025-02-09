#include "blackhole/detail/formatter/string/token.hpp"

namespace blackhole {
namespace detail {
namespace formatter {
namespace string {
namespace placeholder {

generic<required>::generic(std::string name) :
    name(std::move(name)),
    spec("{}")
{}

generic<required>::generic(std::string name, std::string spec) :
    name(std::move(name)),
    spec(std::move(spec))
{}

generic<optional>::generic(std::string name) :
    generic<required>(std::move(name))
{}

generic<optional>::generic(std::string name, std::string spec) :
    generic<required>(std::move(name), std::move(spec))
{}

generic<optional>::generic(generic<required> token, std::string prefix, std::string suffix) :
    generic<required>(std::move(token)),
    prefix(std::move(prefix)),
    suffix(std::move(suffix))
{}

message_t::message_t() : spec("{}") {}
message_t::message_t(std::string spec) : spec(std::move(spec)) {}

template<typename T>
severity<T>::severity() : spec("{}") {}

template<typename T>
severity<T>::severity(std::string spec) : spec(std::move(spec)) {}

timestamp<num>::timestamp() : spec("{}") {}
timestamp<num>::timestamp(std::string spec) : spec(std::move(spec)) {}

timestamp<user>::timestamp() :
    pattern("%Y-%m-%d %H:%M:%S.%f"),
    spec("{}"),
    generator(datetime::make_generator(pattern))
{}

timestamp<user>::timestamp(std::string pattern, std::string spec) :
    pattern(pattern.empty() ? "%Y-%m-%d %H:%M:%S.%f" : std::move(pattern)),
    spec(std::move(spec)),
    generator(datetime::make_generator(this->pattern))
{}

template<typename T>
process<T>::process() : spec("{}") {}

template<typename T>
process<T>::process(std::string spec) : spec(std::move(spec)) {}

template<typename T>
thread<T>::thread() : spec("{}") {}

template<typename T>
thread<T>::thread(std::string spec) : spec(std::move(spec)) {}

thread<hex>::thread() : spec("{:#x}") {}
thread<hex>::thread(std::string spec) : spec(std::move(spec)) {}

leftover_t::leftover_t() :
    unique(false),
    separator(", ")
{}

leftover_t::leftover_t(std::string name) :
    name(std::move(name)),
    unique(false),
    separator(", ")
{}

leftover_t::leftover_t(std::string name, bool unique, std::string prefix, std::string suffix,
    std::string pattern, std::string separator) :
    name(std::move(name)),
    unique(unique),
    prefix(std::move(prefix)),
    suffix(std::move(suffix)),
    pattern(std::move(pattern)),
    separator(std::move(separator))
{}

template struct severity<num>;
template struct severity<user>;

template struct process<id>;
template struct process<name>;

template struct thread<id>;
template struct thread<name>;

}  // namespace placeholder
}  // namespace string
}  // namespace formatter
}  // namespace detail
}  // namespace blackhole

#include "blackhole/detail/procname.hpp"

#include <cstring>

#include "blackhole/cpp17/string_view.hpp"

#ifdef __linux__
#   include <sys/types.h>
#   include <unistd.h>
#elif __APPLE__
#   include <libproc.h>
#endif

namespace blackhole {
namespace detail {

namespace {

auto procname(pid_t pid) -> cpp17::string_view {
#ifdef __linux__
    return cpp17::string_view(program_invocation_short_name, ::strlen(program_invocation_short_name));
#elif __APPLE__
    static char path[PROC_PIDPATHINFO_MAXSIZE];

    if (::proc_name(pid, path, sizeof(path)) > 0) {
        return cpp17::string_view(path, ::strlen(path));
    } else {
        const auto nwritten = ::snprintf(path, sizeof(path), "%d", pid);
        return cpp17::string_view(path, static_cast<std::size_t>(nwritten));
    }
#endif
}

}  // namespace

auto procname() -> cpp17::string_view {
    static const cpp17::string_view name = procname(::getpid());
    return name;
}

}  // namespace detail
}  // namespace blackhole

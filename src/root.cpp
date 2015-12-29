#include "blackhole/root.hpp"

#include <atomic>
#include <mutex>

#include <boost/thread/tss.hpp>

#include "blackhole/extensions/facade.hpp"
#include "blackhole/handler.hpp"
#include "blackhole/record.hpp"
#include "blackhole/scoped.hpp"

#include "blackhole/detail/spinlock.hpp"

namespace blackhole {

using detail::spinlock_t;

struct root_logger_t::sync_t {
    typedef spinlock_t mutex_type;
    mutable mutex_type mutex;

    boost::thread_specific_ptr<scoped_t> context;

    sync_t(): context([](scoped_t*) {}) {}

    auto load(const std::shared_ptr<inner_t>& source) const noexcept -> std::shared_ptr<inner_t> {
        std::lock_guard<mutex_type> lock(mutex);
        return source;
    }

    auto store(std::shared_ptr<inner_t>& source, std::shared_ptr<inner_t> value) noexcept -> void {
        std::lock_guard<mutex_type> lock(mutex);
        source = std::move(value);
    }
};

struct root_logger_t::inner_t {
    typedef spinlock_t mutex_type;

    filter_t filter;
    const std::vector<std::unique_ptr<handler_t>> handlers;

    inner_t(std::vector<std::unique_ptr<handler_t>> handlers):
        filter([](const record_t&) -> bool { return true; }),
        handlers(std::move(handlers))
    {}

    inner_t(filter_t filter, std::vector<std::unique_ptr<handler_t>> handlers):
        filter(std::move(filter)),
        handlers(std::move(handlers))
    {}

    template<typename F>
    auto apply(F&& fn) -> decltype(fn(std::declval<inner_t&>())) {
        std::lock_guard<mutex_type> lock(mutex);
        return fn(*this);
    }

private:
    mutable mutex_type mutex;
};

root_logger_t::root_logger_t(std::vector<std::unique_ptr<handler_t>> handlers):
    sync(new sync_t),
    inner(std::make_shared<inner_t>(std::move(handlers)))
{}

root_logger_t::root_logger_t(filter_t filter, std::vector<std::unique_ptr<handler_t>> handlers):
    sync(new sync_t),
    inner(std::make_shared<inner_t>(std::move(filter), std::move(handlers)))
{}

root_logger_t::root_logger_t(root_logger_t&& other) noexcept :
    sync(new sync_t),
    inner(std::move(other.sync->load(other.inner)))
{
    sync->context.reset(other.sync->context.get());

    if (sync->context.get()) {
        sync->context.get()->rebind(&sync->context);
    }
}

root_logger_t::~root_logger_t() {}

auto
root_logger_t::operator=(root_logger_t&& other) noexcept -> root_logger_t& {
    if (this == &other) {
        return *this;
    }

    const auto inner = std::move(other.sync->load(other.inner));
    sync->store(this->inner, std::move(inner));

    sync->context.reset(other.sync->context.get());

    if (sync->context.get()) {
        sync->context.get()->rebind(&sync->context);
    }

    return *this;
}

auto
root_logger_t::filter(filter_t fn) -> void {
    auto inner = sync->load(this->inner);

    inner->apply([&](inner_t& inner) {
        inner.filter = std::move(fn);
    });

    sync->store(this->inner, std::move(inner));
}

namespace {

struct null_format_t {
    typedef struct {} writer_type;

    constexpr
    auto operator()(writer_type&) const -> string_view {
        return string_view();
    }
};

struct real_format_t {
    typedef writer_t writer_type;

    const logger_t::format_t& fn;

    auto operator()(writer_type& writer) const -> string_view {
        fn(writer);
        return string_view(writer.inner.data(), writer.inner.size());
    }
};

}  // namespace

auto
root_logger_t::log(int severity, string_view pattern) -> void {
    attribute_pack pack;
    log(severity, pattern, pack);
}

auto
root_logger_t::log(int severity, string_view pattern, attribute_pack& pack) -> void {
    consume(severity, pattern, pack, null_format_t());
}

auto
root_logger_t::log(int severity, string_view pattern, attribute_pack& pack, const format_t& fn) -> void {
    consume(severity, pattern, pack, real_format_t{fn});
}

template<typename F>
auto
root_logger_t::consume(int severity, const string_view& pattern, attribute_pack& pack, const F& fn) -> void {
    const auto inner = sync->load(this->inner);
    const auto filter = inner->apply([&](inner_t& inner) -> filter_t {
        return inner.filter;
    });

    // TODO: Consider "null object" pattern.
    if (sync->context.get()) {
        sync->context.get()->collect(&pack);
    }

    record_t record(severity, pattern, pack);
    if (filter(record)) {
        typedef typename F::writer_type writer_type;

        // Keep this writer on stack to be sure that formatted message is alive.
        writer_type writer;
        const auto formatted = fn(writer);

        record.activate(formatted);
        for (auto& handler : inner->handlers) {
            try {
                handler->execute(record);
            } catch (const std::exception& err) {
                std::cout << "logging core error occurred: " << err.what() << std::endl;
            } catch (...) {
                std::cout << "logging core error occurred: unknown" << std::endl;
            }
        }
    }
}

auto
root_logger_t::scoped(attributes_t attributes) -> scoped_t {
    return scoped_t(&sync->context, std::move(attributes));
}

}  // namespace blackhole

#pragma once

#include <memory>
#include <vector>

#include <boost/thread/tss.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "blackhole/attribute.hpp"
#include "blackhole/config.hpp"
#include "blackhole/detail/config/atomic.hpp"
#include "blackhole/detail/config/noncopyable.hpp"
#include "blackhole/detail/config/nullptr.hpp"
#include "blackhole/detail/util/unique.hpp"
#include "blackhole/error/handler.hpp"
#include "blackhole/forwards.hpp"
#include "blackhole/filter.hpp"
#include "blackhole/frontend.hpp"
#include "blackhole/keyword/severity.hpp"

namespace blackhole {

namespace aux {

namespace deleter {

static inline void empty(scoped_attributes_concept_t*) {}

} // namespace deleter

} // namespace aux

class scope_feature_t {
    friend class scoped_attributes_concept_t;

protected:
    boost::thread_specific_ptr<scoped_attributes_concept_t> scoped;

public:
    scope_feature_t() :
        scoped(&aux::deleter::empty)
    {}
};

class base_logger_t {};

template<class T, class... FilterArgs>
class composite_logger_t : public scope_feature_t, public base_logger_t {
public:
    typedef std::function<bool(const attribute::combined_view_t&, const FilterArgs&...)> filter_type;

    typedef boost::shared_mutex rw_mutex_type;
    typedef boost::shared_lock<rw_mutex_type> reader_lock_type;
    typedef boost::unique_lock<rw_mutex_type> writer_lock_type;

protected:
    struct {
        std::atomic<bool> enabled;

        filter_type filter;

        log::exception_handler_t exception;
        std::vector<std::unique_ptr<base_frontend_t>> frontends;

        struct {
            mutable rw_mutex_type open;
            mutable rw_mutex_type push;
        } lock;
    } d;

public:
    composite_logger_t(filter_type filter) {
        d.enabled = true;
        d.exception = log::default_exception_handler_t();
        d.filter = std::move(filter);
    }

    composite_logger_t(composite_logger_t&& other) {
        *this = std::move(other);
    }

    composite_logger_t& operator=(composite_logger_t&& other);

    bool enabled() const {
        return d.enabled;
    }

    void enabled(bool enable) {
        d.enabled.store(enable);
    }

    void set_filter(filter_type filter) {
        writer_lock_type lock(d.lock.open);
        d.filter = std::move(filter);
    }

    void add_frontend(std::unique_ptr<base_frontend_t> frontend) {
        writer_lock_type lock(d.lock.push);
        d.frontends.push_back(std::move(frontend));
    }

    void set_exception_handler(log::exception_handler_t handler) {
        writer_lock_type lock(d.lock.push);
        d.exception = handler;
    }

    record_t open_record(FilterArgs... args) const {
        return open_record(attribute::set_t(), std::forward<FilterArgs>(args)...);
    }

    record_t open_record(attribute::pair_t pair, FilterArgs... args) const {
        return open_record(attribute::set_t({ pair }), std::forward<FilterArgs>(args)...);
    }

    record_t open_record(attribute::set_t external, FilterArgs... args) const {
        if (!enabled()) {
            return record_t::invalid();
        }

        reader_lock_type lock(d.lock.open);
        const attribute::combined_view_t view = with_scoped(external, lock);
        if (!d.filter(view, args...)) {
            return record_t::invalid();
        }

        attribute::set_t internal;
        populate(internal);
        static_cast<const T&>(*this).populate_additional(internal, args...);
        populate(external, lock);
        return record_t(std::move(internal), std::move(external));
    }

    void push(record_t&& record) const {
        reader_lock_type lock(d.lock.push);
        for (auto it = d.frontends.begin(); it != d.frontends.end(); ++it) {
            try {
                (*it)->handle(record);
            } catch (...) {
                d.exception();
            }
        }
    }

private:
    void populate(attribute::set_t& internal) const;
    void populate(attribute::set_t& external, const reader_lock_type&) const;

    attribute::combined_view_t with_scoped(const attribute::set_t& external, const reader_lock_type&) const;
};

class logger_base_t : public composite_logger_t<logger_base_t> {
    friend class composite_logger_t<logger_base_t>;

    typedef composite_logger_t<logger_base_t> base_type;

public:
    logger_base_t() :
        base_type(&filter::none)
    {}

#ifdef BLACKHOLE_HAS_GCC44
    logger_base_t(logger_base_t&& other) : base_type(std::move(other)) {}
    logger_base_t& operator=(logger_base_t&& other) {
        base_type::operator=(std::move(other));
        return *this;
    }
#endif

private:
    void populate_additional(attribute::set_t&) const {}
};

template<typename Level>
class verbose_logger_t : public composite_logger_t<verbose_logger_t<Level>, Level> {
    friend class composite_logger_t<verbose_logger_t<Level>, Level>;

    typedef composite_logger_t<verbose_logger_t<Level>, Level> base_type;

public:
    typedef Level level_type;
    typedef typename aux::underlying_type<level_type>::type underlying_type;

private:
    std::atomic<underlying_type> level;

public:
    verbose_logger_t(level_type level) :
        base_type(default_filter { level }),
        level(static_cast<underlying_type>(level))
    {}

    verbose_logger_t(verbose_logger_t&& other) :
        base_type(std::move(other)),
        level(other.level.load())
    {}

    verbose_logger_t& operator=(verbose_logger_t&& other) {
        base_type::operator=(std::move(other));
        level.store(other.level);
        return *this;
    }

    level_type verbosity() const {
        return static_cast<level_type>(level.load());
    }

    void set_filter(level_type level) {
        base_type::set_filter(default_filter { level });
        this->level.store(level);
    }

    void set_filter(level_type level, typename base_type::filter_type filter) {
        base_type::set_filter(std::move(filter));
        this->level.store(level);
    }

    record_t open_record(level_type level, attribute::set_t external = attribute::set_t()) const {
        return base_type::open_record(std::move(external), level);
    }

private:
    void populate_additional(attribute::set_t& internal, level_type level) const {
        internal.emplace_back(keyword::severity<level_type>() = level);
    }

    struct default_filter {
        const level_type threshold;

        inline bool operator()(const attribute::combined_view_t&, level_type level) const {
            typedef typename aux::underlying_type<level_type>::type underlying_type;
            return static_cast<underlying_type>(level) >= static_cast<underlying_type>(threshold);
        }
    };

};

}

namespace blackhole {

/// Concept form scoped attributes holder.
/*!
 * @note: It's not movable to avoid moving to another thread.
 */
class scoped_attributes_concept_t {
    BLACKHOLE_DECLARE_NONCOPYABLE(scoped_attributes_concept_t);

    scope_feature_t *m_logger;
    scoped_attributes_concept_t *m_previous;

    template<class T, class... FilterArgs>
    friend class composite_logger_t;

public:
    scoped_attributes_concept_t(scope_feature_t& log);
    virtual ~scoped_attributes_concept_t();

    virtual const attribute::set_t& attributes() const = 0;

protected:
    bool has_parent() const;
    const scoped_attributes_concept_t& parent() const;
};

} // namespace blackhole

#if defined(BLACKHOLE_HEADER_ONLY)
#include "blackhole/logger.ipp"
#endif

// TODO: Write tests, which will test multithreading logger/wrapper usage. Helpful, when checking with Valgrind.

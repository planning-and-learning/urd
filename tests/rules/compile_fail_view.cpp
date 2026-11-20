// Views that must not compile, checked like compile_fail_store.cpp: without URD_CASE the file compiles; with
// URD_CASE=<n>, case n alone must fail. Without a case, data<Bare> is a record (Serializable) that urd::resolve views
// through its context; each case changes only the record, so it fails for its own reason.

#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/view.hpp"

#include <cstddef>
#include <type_traits>
#include <vector>

namespace
{
struct Bare;
}  // namespace

#if URD_CASE == 1  // a record that is not Serializable (no cista_members()): urd::resolve views none (resolution<data<T>>)
template<>
struct urd::data<Bare>
{
    int weight = 0;
};
#else
template<>
struct urd::data<Bare>
{
    urd::index<Bare> index;
    int weight = 0;

    data() = default;

    explicit data(int weight_) : index(), weight(weight_) {}

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &data::weight>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::weight> {}; }
};

template<typename V>
struct urd::view_mixin<Bare, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] auto get_weight() const noexcept { return self().template member<&urd::record_of<V>::weight>(); }
};
#endif

namespace
{
// A context that holds the records of Bare.
struct Bares
{
    std::vector<urd::data<Bare>> records;

    const urd::data<Bare>& operator[](urd::index<Bare> at) const { return records[at.value]; }

    [[nodiscard]] std::size_t layer() const noexcept { return 6; }

    [[nodiscard]] urd::index_view<Bare, Bares> view(urd::index<Bare> at) const { return { at, this }; }
};
}  // namespace

int main()
{
    auto bares = Bares {};
    bares.records.emplace_back();
    const auto bare = urd::resolve(bares.records[0], bares);
    return bare.template member<&urd::data<Bare>::weight>();
}

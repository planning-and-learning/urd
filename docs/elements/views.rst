Views
=====

.. cpp:namespace:: urd

A ``T`` is read through a context ``C`` that resolves the indices in its record (:cpp:concept:`resolves`), by one of
four views (``urd/elements/view.hpp``), which differ only in how they hold the record. Each borrows its context as a
``const C*``, which must outlive it, and has the same surface: ``context()``, ``member<&T::m>()``, the
accessors of its :cpp:struct:`view_mixin`, ``value_type`` (``T``), and an identity (what identifies the record, and
the context's number) that ``==``, ``<=>``, ``EqualTo``, ``Hash`` and ``Less`` compare by. A positional set and a pool
of records resolve their own type; any type that gives the record, its number and the view of an index serves.

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type> index_view

   Holds an ``index<T>``, read through the context as ``context[index]`` (a ``T``, or its stored record ``data<T>``)
   and identified by it: what a context makes of its indices, ``context.view(index)``. Two views of one element from one context are equal and hash equally, wherever they were
   made; views from differently numbered contexts are not.

   .. cpp:function:: index_view(index<T> at, const C* context) noexcept
   .. cpp:function:: index<T> index() const noexcept

      Only an index_view names its index; a view of a record names none, so that the record's mixin may.

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type, typename R = data<T>> borrowed_view

   Points at a record ``R`` (``data<T>``, a ``builder<T>``, or another form of ``T``) held outside the context, which
   must outlive the view; read and identified by the record. What :cpp:func:`resolve` makes of a record or builder
   held in place (never of a temporary).

   .. cpp:function:: borrowed_view(const R* record, const C* context)

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type, typename R = data<T>> owning_view

   Holds a record ``R`` by value: one computed rather than stored. Read and identified by the record.

   .. cpp:function:: owning_view(R record, const C* context)

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type, typename R = data<T>> shared_view

   Shares a record ``R`` (``std::shared_ptr<const R>``), released with the last copy. Read and identified by the
   record.

   .. cpp:function:: shared_view(std::shared_ptr<const R> record, const C* context)

Each reads its record (:cpp:type:`record_of`) whole: a borrowed, owning or shared view the record it holds; an
index_view the record its context hands out, ``context[index]`` (``HandsOutRecordsBy``), as a ``stored_ref<T>``: a
reference to the stored ``data<T>``, or a reader of a compressed record (bit-packed) that reads it field by field
(:cpp:struct:`compressed_form`). Its record is ``data<T>`` all the same: whatever the layout, the key is the handle
and ``data<T>`` the schema.

What all four give:

   .. cpp:function:: const C& context() const noexcept

   .. cpp:function:: template<auto Member> decltype(auto) get() const

      A member of the record resolved through the context (:cpp:func:`resolve`): an ``index<U>`` becomes the
      ``index_view<U, C>`` the context makes, pointing at the layer that owns the element; a range of indices a range
      of views; a variant the variant of its alternatives resolved; an optional the optional of its value resolved; a
      pair the pair of its members resolved; a stored record held in the record its borrowed_view; anything else
      stays as it is.

   .. cpp:function:: static constexpr auto identifying_members() noexcept

      The identity: the index, or the record, and the context's number (:cpp:struct:`layer`); only where the context
      has a number.

.. cpp:concept:: template<typename C, typename T> resolves

   ``context[index<T>]`` is convertible to ``const T&``, or is the stored record ``data<T>`` (the slot of
   ``urd/elements/data.hpp``, which must be Serializable); ``context.layer()`` is its number; ``context.view(index<T>)``
   gives the ``index_view<T, C>`` of the index, through the context that owns it (a layer points it at the layer that owns
   the element; a context made of several Stores writes one per type).

.. cpp:function:: template<typename M, typename C> decltype(auto) resolve(M&& member, const C& context)

   A record's member as a view sees it: ``resolution<M>::of(member, context)``, the member passed on as given (so
   that a record or builder, which its resolution borrows, is refused as a temporary); see :cpp:func:`index_view::get`.

.. cpp:concept:: template<typename V> View

   One of urd's views (``is_view``): an :cpp:class:`index_view`, ``borrowed_view``, ``owning_view``, ``shared_view`` or
   :cpp:class:`pooled_view`.

.. cpp:function:: template<View V, typename F> decltype(auto) visit_view(const V& view, F&& f)
                  template<View... Vs, typename F> decltype(auto) visit_view(const std::variant<Vs...>& views, F&& f)

   ``f`` of a view, or of the view a resolved :cpp:type:`child` holds (a variant of the views of its forms). One
   reading serves a member that resolves to a view in a record and to a variant of views in its builder.

.. cpp:struct:: template<typename M> resolution

   How a member of type ``M`` resolves: as it is, unless specialized. Given for indices (``index<U>``: the context's view),
   ranges of indices (a range of views), and variants, optionals and pairs, std's and cista's alike (their contents
   resolved; ``urd/elements/view.hpp``, by the concepts ``variant_value``, ``optional_value`` and ``pair_value``). A
   new member type resolves by specializing it.

.. cpp:class:: template<std::ranges::input_range R, std::copy_constructible F> transformed_range
.. cpp:function:: template<std::ranges::input_range R, typename F> transformed_range<std::remove_cvref_t<R>, std::decay_t<F>> transform(R&& range, F&& function)

   ``range`` read through ``function``: each element ``x`` read as ``function(x)``, a borrowed
   ``std::views::transform``. It holds ``range``'s iterators and the function, and so do its iterators: unlike
   ``std::views::transform``'s, they do not point into the range object and outlive it, so it is a
   ``std::ranges::borrowed_range`` (a function may return it, Python may iterate it after the call). It traverses
   as ``range`` does (random access, sized where it is), ends at ``range``'s sentinel where it has one, and has
   ``base()``, ``function()`` and a checked ``at(position)``. ``range`` must outlive it unless it is a borrowed view
   (iota, a span, the set bits of a bitset); a view that is not borrowed is refused, and ``transform`` takes an rvalue
   only where it is borrowed. The function is a small copy-constructible function object: a lambda (capturing by value
   what it reads through, such as a view) or a struct holding it (a pointer). It is held in a box, as
   ``std::views::transform`` holds it, so that the range and its iterators stay default-constructible and assignable;
   each iterator holds its own copy. It owns nothing else. Where what it reads is a value range
   (``urd::value_range``) or a view, it is a value range itself.

.. cpp:type:: template<typename M, typename C> resolved_range = transformed_range<M, resolver<C>>

   A range of resolving elements read through a context: the :cpp:class:`transformed_range` of ``M`` whose function,
   ``resolver<C>`` (``{ &context }``), resolves each element (:cpp:func:`resolve`) through the context, which it
   borrows; what :cpp:struct:`resolution` makes of a range of indices. ``context()`` gives the context. It can be
   spelled while ``C`` is still incomplete.

.. cpp:struct:: template<typename T> context_of

   Names a record type's default context once (``template<> struct urd::context_of<Atom> { using type =
   Repository; };``), so that ``index_view<Atom>`` needs no second argument.

.. cpp:struct:: template<typename T, typename V> view_mixin

   Where a library adds the named accessors of a record's views, one line each over ``member<>``: ``template<typename
   V> struct urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template
   member<&urd::record_of<V>::predicate>(); } };``, where ``record_of<V>`` is the record the view reads (named through
   ``V``, so it may be incomplete where the mixin is declared). The mixin is keyed on what the view reads:
   views of an index and of a record of ``T`` share ``T``'s mixin; views of a builder use ``builder<T>``'s, which is
   ``T``'s unless specialized. The view's
   ``member<&T::m>()`` hides any ``member`` a mixin would declare.

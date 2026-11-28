Datas
=====

.. cpp:namespace:: urd

A record is a value a Store keeps once and never changes: the stored form of a tag ``T``
(``urd/elements/data.hpp``).

.. cpp:struct:: template<typename T> data

   Specialized per tag. It holds, in this order: ``index<T> index``, where the record is interned (the index its Store
   gives it, none until then; a value held in other records has none); its fields, data members of any type urd can
   store as bytes; and the declarations urd reads it by:

   - ``fields()``: each field once, under its member's name (:cpp:struct:`Fields`, below);
   - ``identifying_members()``: the members that make it what it is (:cpp:struct:`Members`; ``members_of(fields())``
     for all fields), by which it is hashed, compared and ordered (:doc:`identity`);
   - ``cista_members()``: its index, where it has one, and its fields in order, which cista serializes;
   - ``copyable()``, for a record that is trivially copyable (indices and numbers, no memory of its own): it declares
     that it is copied, and the compiler checks it; any other record is move-only.

   A Store of records (:doc:`/kinds/records`) holds ``data<T>`` for an ``index<T>``; a view reads it
   (:doc:`views`); a builder has its fields in another memory organization (:doc:`builders`).

.. cpp:concept:: template<typename T> stored_record

   ``data<T>`` is ``Serializable``: ``cista_members()`` names its members as :cpp:struct:`Members`.

A record with ``identifying_members()`` compares and orders by them (``==``, ``<=>``).

Compressed records
------------------

A Store may keep the records of a kind compressed, in a form of its own (bits packed at their widths, a node of a
tree, a row of a pool) rather than as ``data<T>``, which stays the owning form a record is inserted and found from. The
kind says so once, by a specialization of :cpp:struct:`compressed_form` declared beside its tag (before its key is
used: the key reads it). Unspecialized, a lookup hands out a reference to the stored ``data<T>`` (``record_ref<T>``),
each field is read in place and the key is a number.

.. cpp:struct:: template<typename T> compressed_form

   How the Store of ``T`` hands its records out (``urd/elements/concepts.hpp``). A specialization declares:

   - ``using ref = R;``: what a lookup hands out, by value (``stored_ref<T>``: ``urd::record_at``, a context's
     ``operator[]`` and ``at()``), made from ``context[key]``;
   - ``template<auto Member> static auto read(const R& stored);``: each field ``Member`` of ``data<T>`` read from it,
     by value or in place as the form allows; a view's ``member<Member>()``, a walk over the record's children and a
     builder made from it read every field through it (``stored_field``);
   - and only where the Store names a record by a handle rather than a number: ``using key_value = H;`` and
     ``static constexpr H none;``, the handle that names nothing. The key's part then holds the handle
     (``key_value_of<T>``): ``key<T>`` is compared, hashed, stored and resolved as an index is, and is no index
     (``NumberedKind<T>`` is false).

   A state's storage in its pool's tree hands out its pool and its key, unpacks its values on reading and is named by
   the pool's handle; a row in its relation's slot hands out a reader of its packed bits (its values) and of its slot's
   relation, under a numbered key.

.. cpp:concept:: template<typename T> CompressedKind

   ``compressed_form<T>`` names ``ref``.

.. cpp:concept:: template<typename T> HandleKeyedKind

   ``T`` is a :cpp:concept:`CompressedKind` whose form names ``key_value`` and ``none``.

.. cpp:concept:: template<auto Member, typename R> CompressedField

   ``Member`` is a field of ``data<T>`` for a :cpp:concept:`CompressedKind` ``T``, and ``R`` is what its Store hands
   out: the field is read by ``compressed_form<T>::read<Member>``.

Fields
------

A record may name its fields once: each a public name and the data member that holds it, in data order. Its
identifying members are then their members, and whatever reads a record field by field (serialization, printing,
Python bindings, a walk over its children) iterates the declaration instead of asking for each member, so that a field
is never left out.

.. code-block:: cpp

   struct Atom
   {
       urd::index<Predicate> predicate;
       urd::flat_vector<urd::index<Object>> objects;
       static constexpr auto fields() noexcept
       {
           return urd::Fields<urd::field<"predicate", &Atom::predicate>, urd::field<"objects", &Atom::objects>> {};
       }
       static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }
   };

.. cpp:struct:: template<fixed_string Name, auto Member> field

   ``name`` (a ``std::string_view``, null-terminated), ``member``, and ``prefixed<"get_">``, the name after a prefix.

.. cpp:function:: template<HasFields T, typename F> constexpr void for_each_field(F&& f)

   ``f.template operator()<Field>()`` for each field, in order. A field is read through a view as
   ``view.template member<Field::member>()``: resolved through the view's context, like any member.

Flat containers
---------------

.. cpp:type:: template<typename T> flat_vector = cista::offset::vector<T>
.. cpp:type:: flat_string = cista::offset::string
.. cpp:type:: template<typename... Ts> flat_variant = cista::offset::variant<Ts...>
.. cpp:type:: template<typename T> flat_optional = cista::optional<T>
.. cpp:type:: template<typename T1, typename T2> flat_pair = cista::pair<T1, T2>

   ``is_flat_vector<T>``, ``is_flat_variant<T>``, ``is_flat_optional<T>`` and ``is_flat_pair<T>`` say whether a type is
   one of them.

   With :cpp:struct:`Hash` and :cpp:struct:`EqualTo` (the same byte hash as ``std::string``, so equal text hashes
   alike), and resolved by views like their standard counterparts.

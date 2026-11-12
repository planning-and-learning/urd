# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later
# Types for the part of libclang's Python bindings tests/exposure.py reads; the bindings declare none.

from collections.abc import Iterator

class CursorKind:
    CLASS_DECL: CursorKind
    CLASS_TEMPLATE: CursorKind
    CLASS_TEMPLATE_PARTIAL_SPECIALIZATION: CursorKind
    CONVERSION_FUNCTION: CursorKind
    CXX_BASE_SPECIFIER: CursorKind
    CXX_METHOD: CursorKind
    FUNCTION_TEMPLATE: CursorKind
    STRUCT_DECL: CursorKind
    TEMPLATE_REF: CursorKind
    TRANSLATION_UNIT: CursorKind
    TYPE_REF: CursorKind
    TYPE_ALIAS_DECL: CursorKind

class AccessSpecifier:
    PUBLIC: AccessSpecifier

class File:
    @property
    def name(self) -> str: ...

class SourceLocation:
    @property
    def file(self) -> File | None: ...
    @property
    def line(self) -> int: ...

class SourceRange: ...

class Type:
    @property
    def spelling(self) -> str: ...

class Token:
    @property
    def spelling(self) -> str: ...

class Cursor:
    @property
    def kind(self) -> CursorKind: ...
    @property
    def spelling(self) -> str: ...
    @property
    def semantic_parent(self) -> Cursor | None: ...
    @property
    def referenced(self) -> Cursor | None: ...
    @property
    def access_specifier(self) -> AccessSpecifier: ...
    @property
    def location(self) -> SourceLocation: ...
    @property
    def type(self) -> Type: ...
    @property
    def extent(self) -> SourceRange: ...
    @property
    def underlying_typedef_type(self) -> Type: ...
    def get_children(self) -> Iterator[Cursor]: ...
    def get_tokens(self) -> Iterator[Token]: ...
    def is_definition(self) -> bool: ...

class Diagnostic:
    Error: int
    @property
    def severity(self) -> int: ...

class TranslationUnit:
    @property
    def cursor(self) -> Cursor: ...
    @property
    def diagnostics(self) -> Iterator[Diagnostic]: ...
    def get_tokens(self, extent: SourceRange) -> Iterator[Token]: ...

class Index:
    @staticmethod
    def create() -> Index: ...
    def parse(self, path: str, args: list[str] | None = None) -> TranslationUnit: ...

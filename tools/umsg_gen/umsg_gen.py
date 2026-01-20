#!/usr/bin/env python3
"""umsg_gen: generate C++11 headers from .umsg message definitions.

Design goals:
- stdlib only (portable)
- deterministic output and hashing
- simple restricted grammar (see design.md)

Usage:
  python3 tools/umsg_gen/umsg_gen.py input.umsg -o out_dir

This emits: out_dir/<struct_name>.hpp
"""

from __future__ import annotations

import argparse
import dataclasses
import os
import re
import sys
from typing import List, Optional, Sequence, Tuple


@dataclasses.dataclass(frozen=True)
class Field:
    type_name: str  # e.g. uint32_t, double
    name: str
    array_len: Optional[int] = None


@dataclasses.dataclass(frozen=True)
class Message:
    struct_name: str
    package: Optional[str]
    fields: Tuple[Field, ...]
    canonical_text: str
    msg_hash: int


# ---- hashing (design.md: FNV-1a 32-bit) ----

def fnv1a_32(data: bytes) -> int:
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


# ---- canonicalization of .umsg source ----

def strip_comments(text: str) -> str:
    # Remove /* ... */ block comments
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    # Remove // ... to end-of-line (handles \n and \r\n)
    text = re.sub(r"//[^\n\r]*", "", text)
    return text


def remove_ascii_whitespace(text: str) -> str:
    # Remove space, tab, CR, LF
    return re.sub(r"[ \t\r\n]+", "", text)


def canonicalize_for_hash(umsg_text: str) -> str:
    text = strip_comments(umsg_text)
    # Exclude optional package directive from schema hash.
    # This is done before whitespace stripping so patterns are simple and robust.
    text = re.sub(r"\bpackage\s+[A-Za-z_][A-Za-z0-9_.]*\s*;", "", text)
    return remove_ascii_whitespace(text)


# ---- parsing (restricted grammar) ----

_ALLOWED_TYPES = {
    "uint8_t",
    "int8_t",
    "uint16_t",
    "int16_t",
    "uint32_t",
    "int32_t",
    "uint64_t",
    "int64_t",
    "bool",
    "float",
    "double",
}

_IDENT_RE = r"[A-Za-z_][A-Za-z0-9_]*"


class ParseError(Exception):
    pass


def _expect_no_extra_tokens(src: str) -> None:
    # We keep this simple: the parser either consumes or raises.
    # This helper exists for future expansion.
    _ = src


def parse_umsg(text: str) -> Message:
    """Parse a .umsg file containing exactly one struct definition."""

    # Keep original text for hashing.
    canonical = canonicalize_for_hash(text)
    msg_hash = fnv1a_32(canonical.encode("utf-8"))

    # Parse from comment-stripped text (but keep whitespace for easier regex boundaries).
    src = strip_comments(text)

    # Find the struct header.
    m = re.search(rf"\bstruct\s+({_IDENT_RE})\s*\{{", src)
    if not m:
        raise ParseError("expected 'struct <name> { ... };'")

    # Optional: package directive in the preamble. Allowed forms:
    #   package foo;
    #   package foo.bar;
    preamble = src[: m.start()]
    pm = re.fullmatch(r"\s*(?:package\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;\s*)?", preamble)
    if not pm:
        raise ParseError("unexpected content before struct (only optional 'package <name>;' allowed)")
    package = pm.group(1)

    struct_name = m.group(1)

    # Extract body by matching braces (simple, single struct).
    brace_start = src.find("{", m.end() - 1)
    if brace_start < 0:
        raise ParseError("expected '{' after struct name")

    depth = 0
    end = None
    for i in range(brace_start, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    if end is None:
        raise ParseError("unterminated '{' in struct")

    body = src[brace_start + 1 : end]
    rest = src[end + 1 :]

    sm = re.match(r"^\s*;", rest)
    if not sm:
        raise ParseError("expected ';' after closing '}'")

    trailing = rest[sm.end() :]
    if trailing.strip():
        raise ParseError("unexpected trailing content after struct definition")

    fields: List[Field] = []

    # Split into statements by ';' and parse each.
    for stmt in body.split(";"):
        stmt = stmt.strip()
        if not stmt:
            continue

        # Match: <type> <name> [ [N] ]
        fm = re.fullmatch(
            rf"({_IDENT_RE})\s+({_IDENT_RE})\s*(?:\[\s*(\d+)\s*\])?\s*",
            stmt,
        )
        if not fm:
            raise ParseError(f"invalid field declaration: '{stmt}'")

        type_name, name, arr = fm.group(1), fm.group(2), fm.group(3)
        if type_name not in _ALLOWED_TYPES:
            raise ParseError(f"unsupported type '{type_name}'")

        array_len: Optional[int] = None
        if arr is not None:
            array_len = int(arr)
            if array_len <= 0:
                raise ParseError("array length must be > 0")

        fields.append(Field(type_name=type_name, name=name, array_len=array_len))

    if not fields:
        raise ParseError("struct has no fields")

    _expect_no_extra_tokens(src)

    return Message(
        struct_name=struct_name,
        package=package,
        fields=tuple(fields),
        canonical_text=canonical,
        msg_hash=msg_hash,
    )


# ---- code generation ----


def cpp_type_size_expr(type_name: str) -> str:
    # Use sizeof(T) for portability; the generator emits C++11 and includes <stdint.h>.
    return f"sizeof({type_name})"


def cpp_payload_size_expr(fields: Sequence[Field]) -> str:
    parts: List[str] = []
    for f in fields:
        if f.array_len is None:
            parts.append(cpp_type_size_expr(f.type_name))
        else:
            parts.append(f"({cpp_type_size_expr(f.type_name)} * {f.array_len}u)")
    return " + ".join(parts) if parts else "0u"


def emit_header(msg: Message, source_path: Optional[str] = None, header_guard: Optional[str] = None) -> str:
    # Prefer #pragma once in this repo.
    payload_size_expr = cpp_payload_size_expr(msg.fields)

    struct_lines: List[str] = [f"struct {msg.struct_name}", "{"
    ]
    for f in msg.fields:
        if f.array_len is None:
            struct_lines.append(f"    {f.type_name} {f.name};")
        else:
            struct_lines.append(f"    {f.type_name} {f.name}[{f.array_len}];")
    struct_lines.append("")
    struct_lines.append(f"    static const uint32_t kMsgHash = 0x{msg.msg_hash:08X}u;")
    struct_lines.append(f"    static const size_t kPayloadSize = {payload_size_expr};")
    struct_lines.append("")

    # encode/decode use capacity-in/length-out for encode.
    struct_lines.append("    bool encode(umsg::bufferSpan& payload) const")
    struct_lines.append("    {")
    struct_lines.append("        if (!payload.data) return false;")
    struct_lines.append("        const size_t cap = payload.length;")
    struct_lines.append("        umsg::Writer w(payload);")

    for f in msg.fields:
        if f.array_len is None:
            struct_lines.append(f"        if (!w.write({f.name})) return false;")
        else:
            struct_lines.append(f"        if (!w.writeArray({f.name}, {f.array_len}u)) return false;")

    struct_lines.append("        if (w.bytesWritten() > cap) return false;")
    struct_lines.append("        payload.length = w.bytesWritten();")
    struct_lines.append("        return true;")
    struct_lines.append("    }")
    struct_lines.append("")

    struct_lines.append("    bool decode(umsg::bufferSpan payload)")
    struct_lines.append("    {")
    struct_lines.append("        umsg::Reader r(payload);")

    for f in msg.fields:
        if f.array_len is None:
            struct_lines.append(f"        if (!r.read({f.name})) return false;")
        else:
            struct_lines.append(f"        if (!r.readArray({f.name}, {f.array_len}u)) return false;")

    struct_lines.append("        return r.fullyConsumed();")
    struct_lines.append("    }")

    struct_lines.append("};")

    source_note = ""
    if source_path:
        source_note = os.path.basename(source_path)

    generated_comment = (
        "// -----------------------------------------------------------------------------\n"
        "// This file was generated by umsg-gen.\n"
        + (f"// Source: {source_note}\n" if source_note else "")
        + "//\n"
          "// DO NOT EDIT THIS FILE DIRECTLY.\n"
          "// Edit the corresponding .umsg schema and re-run umsg-gen instead.\n"
          "// -----------------------------------------------------------------------------\n"
    )

    header = "\n".join(
        [
            "#pragma once",
            "#include <stddef.h>",
            "#include <stdint.h>",
            "",
            generated_comment.rstrip("\n"),
            "",
            "#include \"marshalling.hpp\"",
            "",
            *struct_lines,
            "",
        ]
    )

    if header_guard:
        # Not used by default, but available if you prefer guards.
        header = (
            f"#ifndef {header_guard}\n#define {header_guard}\n" + header + f"\n#endif // {header_guard}\n"
        )

    return header


# ---- CLI ----


def write_if_changed(path: str, content: str) -> None:
    try:
        with open(path, "r", encoding="utf-8") as f:
            old = f.read()
        if old == content:
            return
    except FileNotFoundError:
        pass

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def main(argv: Sequence[str]) -> int:
    ap = argparse.ArgumentParser(prog="umsg_gen", description="Generate C++ headers from .umsg")
    ap.add_argument("input", nargs="+", help=".umsg file(s)")
    ap.add_argument("-o", "--out", required=True, help="output directory")
    ap.add_argument("--stdout", action="store_true", help="write generated header(s) to stdout")

    args = ap.parse_args(list(argv))

    out_dir = args.out

    for in_path in args.input:
        with open(in_path, "r", encoding="utf-8") as f:
            text = f.read()

        msg = parse_umsg(text)
        header = emit_header(msg, source_path=in_path)

        if args.stdout:
            sys.stdout.write(header)
            continue

        if msg.package:
            package_dir = msg.package.replace(".", os.sep)
            out_path = os.path.join(out_dir, package_dir, f"{msg.struct_name}.hpp")
        else:
            out_path = os.path.join(out_dir, f"{msg.struct_name}.hpp")
        write_if_changed(out_path, header)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

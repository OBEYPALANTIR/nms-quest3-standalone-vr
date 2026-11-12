#!/usr/bin/env python3
"""Apply explicit, checked scalar overrides to MBINCompiler XML files."""

import argparse
import json
import re
import struct
from decimal import Decimal, InvalidOperation
from pathlib import Path
from xml.etree import ElementTree as ET
from xml.parsers import expat


SEGMENT = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)(?:\[(0|[1-9][0-9]*)\])?")
FILENAME = re.compile(r"[A-Z][A-Z_0-9]*\.GLOBAL\.EXML")
INTEGER = re.compile(r"-?(?:0|[1-9][0-9]*)")
NUMBER = re.compile(r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?")


def scalar(value, kind):
    if kind == "bool" and value in ("true", "false"):
        return value
    if kind == "int32" and INTEGER.fullmatch(value):
        number = int(value)
        if -(2**31) <= number < 2**31:
            return number
    if kind == "float" and NUMBER.fullmatch(value):
        number = Decimal(value)
        try:
            binary32 = struct.pack("<f", float(number))
        except OverflowError:
            pass
        else:
            decoded = struct.unpack("<f", binary32)[0]
            if abs(decoded) != float("inf") and (decoded != 0 or number == 0):
                return binary32
    if kind == "enum" and SEGMENT.fullmatch(value) and "[" not in value:
        return value
    raise ValueError(f"invalid {kind} scalar {value!r}")


def resolve(root, path):
    node = root
    for part in path.split("/"):
        segment = SEGMENT.fullmatch(part)
        if segment is None:
            raise ValueError(f"invalid path {path!r}")
        name, index = segment.groups()
        matches = [child for child in node if child.tag == "Property"
                   and child.get("name") == name
                   and (index is None or child.get("_index") == index)]
        if len(matches) != 1 or (index is None and matches[0].get("_index") is not None):
            raise ValueError(f"unknown or ambiguous property {path!r} at {part!r}")
        node = matches[0]
    if len(node) or node.get("value") is None or node.get("array_size") is not None:
        raise ValueError(f"expected a scalar property at {path!r}")
    return node


ATTRIBUTE = re.compile(rb"""\s+([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(?:"([^"]*)"|'([^']*)')""")


def parse_document(document):
    offsets = {}
    tree = ET.TreeBuilder()
    parser = expat.ParserCreate("utf-8")

    def start(name, attributes):
        node = tree.start(name, attributes)
        if name == "Property":
            offsets[node] = parser.CurrentByteIndex

    def reject_doctype(*_):
        raise ValueError("document type declarations are unsupported")

    parser.StartElementHandler = start
    parser.EndElementHandler = tree.end
    parser.CharacterDataHandler = tree.data
    parser.StartDoctypeDeclHandler = reject_doctype
    parser.Parse(document, True)
    return tree.close(), offsets


def value_span(document, offset):
    cursor = offset + len(b"<Property")
    while match := ATTRIBUTE.match(document, cursor):
        if match[1] == b"value":
            return match.span(2 if match[2] is not None else 3)
        cursor = match.end()
    raise ValueError("could not locate the original value attribute")


def merge(document, spec):
    """Return the output XML and a report of requested values and no-ops."""
    root, offsets = parse_document(document)
    if root.tag != "Data" or root.get("template") != spec["template"]:
        raise ValueError(f"expected template {spec['template']!r}")

    report = []
    seen = set()
    replacements = []
    for edit in spec["edits"]:
        path, kind = edit["path"], edit["type"]
        node = resolve(root, path)
        if node in seen:
            raise ValueError(f"duplicate edit for {path!r}")
        seen.add(node)
        old = node.attrib["value"]
        new = edit["value"]
        if not all(isinstance(value, str) for value in (edit["expected"], new)):
            raise ValueError(f"{path}: expected and value must be strings")
        if old != edit["expected"]:
            raise ValueError(f"{path}: expected {edit['expected']!r}, found {old!r}")
        try:
            changed = scalar(old, kind) != scalar(new, kind)
        except (InvalidOperation, ValueError) as error:
            raise ValueError(f"{path}: {error}") from error
        if changed:
            start, end = value_span(document, offsets[node])
            replacements.append((start, end, new.encode("ascii")))
        report.append({"path": path, "before": old, "after": new if changed else old,
                       "changed": changed})

    # Work backwards so each byte span remains relative to the original input.
    for start, end, value in sorted(replacements, reverse=True):
        document = document[:start] + value + document[end:]
    return document, report


def run(source, overrides, output):
    specs = json.loads(overrides.read_text(encoding="utf-8"))
    if not isinstance(specs, list) or not specs:
        raise ValueError("override file must be a nonempty list of file specifications")
    results = {}
    report = {}
    for spec in specs:
        name = spec["file"]
        if not FILENAME.fullmatch(name) or name in results:
            raise ValueError(f"invalid or duplicate filename {name!r}")
        try:
            results[name], report[name] = merge((source / name).read_bytes(), spec)
        except (expat.ExpatError, ValueError) as error:
            raise ValueError(f"{name}: {error}") from error

    # Validate the entire batch before creating an output directory.
    output.mkdir(parents=True)
    for name, result in results.items():
        (output / name).write_bytes(result)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("overrides", type=Path)
    parser.add_argument("output", type=Path, help="new output directory")
    args = parser.parse_args()
    try:
        report = run(args.source, args.overrides, args.output)
    except (KeyError, OSError, TypeError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from merge_overrides import merge, run


BASE = b'''<?xml version="1.0" encoding="utf-8"?>
<!-- a preamble with UTF-8: \xc3\xa9 -->
<Data template="cSynthetic">
  <Property name="Item" value="20" />
  <Property name="Group" value="Nested">
    <Property name="Items" array_size="2">
      <Property name="Item" _index="0" value='1' />
      <Property value="2" _index="1" name="Item" />
    </Property>
  </Property>
  <!-- keep &amp; and every byte outside target attributes -->
  <Property name="Flag" value="false" />
  <Property name="Float" value="1.000000" />
</Data>
'''


def edit(path, old, new, kind="int32"):
    return {"path": path, "type": kind, "expected": old, "value": new}


def spec(*edits, name="SYNTHETIC.GLOBAL.EXML"):
    return {"file": name, "template": "cSynthetic", "edits": edits}


class MergeTests(unittest.TestCase):
    def test_nested_repeated_fields_change_only_the_selected_values(self):
        output, report = merge(BASE, spec(
            edit("Group/Items/Item[1]", "2", "42"),
            edit("Group/Items/Item[0]", "1", "11"),
        ))
        self.assertEqual(output, BASE.replace(b"value='1'", b"value='11'").replace(
            b'value="2"', b'value="42"'))
        self.assertEqual([row["after"] for row in report], ["42", "11"])

    def test_unknown_and_ambiguous_paths_fail(self):
        for path in ("Unknown", "Group/Items/Item", "Group/Items/Item[2]",
                     "Group/Items", "Item[0]", "Group//Items"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                merge(BASE, spec(edit(path, "1", "2")))

    def test_wrong_type_and_stale_source_fail(self):
        for change in (edit("Flag", "false", "1", "bool"),
                       edit("Flag", "false", "true", "int32"),
                       edit("Item", "20", "0.1"),
                       edit("Float", "1.000000", "1e100", "float"),
                       edit("Item", "20", "2147483648"),
                       edit("Item", "21", "22")):
            with self.subTest(change=change), self.assertRaises(ValueError):
                merge(BASE, spec(change))

    def test_duplicate_edit_fails(self):
        with self.assertRaisesRegex(ValueError, "duplicate edit"):
            merge(BASE, spec(edit("Item", "20", "21"), edit("Item", "20", "22")))

    def test_noops_keep_original_bytes_and_value_spelling(self):
        output, report = merge(BASE, spec(edit("Float", "1.000000", "1", "float")))
        self.assertEqual(output, BASE)
        self.assertFalse(report[0]["changed"])
        self.assertEqual(report[0]["after"], "1.000000")
        self.assertEqual(merge(BASE, spec()), (BASE, []))

    def test_batch_rejects_unknown_property_before_creating_output(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            valid = spec(edit("Item", "20", "42"))
            invalid = spec(edit("NoSuchProperty", "0", "1"), name="SECOND.GLOBAL.EXML")
            for item in (valid, invalid):
                (root / item["file"]).write_bytes(BASE)
            overrides = root / "overrides.json"
            overrides.write_text(json.dumps([valid, invalid]))
            output = root / "merged"
            with self.assertRaises(ValueError):
                run(root, overrides, output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()

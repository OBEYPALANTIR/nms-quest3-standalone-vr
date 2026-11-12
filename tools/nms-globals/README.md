# Merge No Man's Sky globals overrides

The merge utility applies checked scalar edits to complete globals XML exported
by MBINCompiler. It edits only the selected value bytes. Use the original
globals from the game build under test and keep game data outside this checkout.

## Extract and verify the baseline

Use [HGPAKtool 1.1.3](https://github.com/monkeyman192/HGPAKtool/tree/2ea9b0351f93b72e4fa3d199bf4ac54149f748f8)
for Windows HGPAK v2 archives. List members of the specific PAK first. A member
name may be bare even though its EXML belongs in a mod's `GLOBALS/` directory.

```sh
hgpaktool --platform windows -L -p /path/to/NMSARC.globals.pak
hgpaktool --platform windows -U --upper -O "$original" \
  -f 'gcgraphicsglobals.global.mbin' /path/to/NMSARC.globals.pak
```

Choose the [MBINCompiler release](https://github.com/monkeyman192/MBINCompiler/releases)
by testing it on the actual MBIN. Record the executable, PAK and compiler
hashes. These commands were exercised with the macOS arm64 release
[v7.10.0-pre1](https://github.com/monkeyman192/MBINCompiler/releases/tag/v7.10.0-pre1)
and .NET 8.0.33. Create the output directories before using the compiler.

```sh
MBINCompiler convert --output-format=EXML --output-dir="$shipped" "$original"
MBINCompiler convert --input-format=MXML --include='*.EXML' \
  --output-dir="$roundtrip" "$shipped"
cmp "$original/GCGRAPHICSGLOBALS.GLOBAL.MBIN" \
  "$roundtrip/GCGRAPHICSGLOBALS.GLOBAL.EXML.MBIN"
```

Treat a byte difference as a failed stock roundtrip. For format 3300,
[MBINHeader.SetDefaults](https://github.com/monkeyman192/MBINCompiler/blob/5c8d1ab10e6199c282297585788bf9a0024a3f67/libMBIN/Source/MBIN/MBINHeader.cs#L110)
writes the compiler's marker and version in the header. If those differ,
record the exact offsets separately from any payload differences.

For a stock output whose only differences are the format-3300 compiler marker
and timestamp bytes, the verification utility checks the full payload and all
other header bytes before copying the six approved metadata positions from the
matching shipped file. Keep its output separate from the stock roundtrip.

```sh
python3 tools/nms-globals/verify_roundtrip.py \
  "$original/GCGRAPHICSGLOBALS.GLOBAL.MBIN" \
  "$roundtrip/GCGRAPHICSGLOBALS.GLOBAL.EXML.MBIN" "$verification_output"
cmp "$original/GCGRAPHICSGLOBALS.GLOBAL.MBIN" "$verification_output"
```

This comparison uses the original file as the reference for header metadata.
The output is a verification artifact. Preserve the stock comparison result
separately, and deploy the loose EXML for any authorized runtime test.

## Apply a checked edit

Supply a UTF-8 JSON list with one entry per output file. The example is synthetic.

```json
[
  {
    "file": "SYNTHETIC.GLOBAL.EXML",
    "template": "cSynthetic",
    "edits": [
      {
        "path": "Rows/Entry[1]/Enabled",
        "type": "bool",
        "expected": "true",
        "value": "false"
      }
    ]
  }
]
```

Paths are relative to the root `Data` element. Each path segment selects
one immediate child `Property` by name. Include `[index]` where the XML
defines an explicit `_index`. For an enum-indexed array, use the emitted
member name. Supported scalar types are `bool`, `int32`, `float` (binary32)
and `enum` (identifier). Derive types and allowed enum identifiers from the
matching libMBIN template. The tool checks the original spelling in
`expected` and validates scalar syntax. Check enum membership with the
compiler. A file entry with an empty `edits` list copies that baseline file.

```sh
python3 tools/nms-globals/merge_overrides.py "$shipped" "$recipe" "$output"
```

Use a fresh output directory. An invalid or ambiguous edit rejects the batch
before that directory is created. Standard output records changes and no-ops.
Recompile and decode the candidate against the same schema to check that
each change survived serialization. Offline checks establish the contents
of the files. The game loader and performance require separate verification.

```sh
cd tools/nms-globals
python3 -m unittest -v
```

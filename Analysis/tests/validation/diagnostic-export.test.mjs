import test from "node:test";
import assert from "node:assert/strict";
import { decodeDORDiagnosticSave } from "../../src/diagnostics/export-savegame.mjs";

function fstring(value) {
  const ascii = /^[\x00-\x7f]*$/.test(value);
  if (ascii) {
    const text = Buffer.from(`${value}\0`, "latin1");
    const length = Buffer.alloc(4);
    length.writeInt32LE(value.length + 1);
    return Buffer.concat([length, text]);
  }
  const text = Buffer.from(`${value}\0`, "utf16le");
  const length = Buffer.alloc(4);
  length.writeInt32LE(-(value.length + 1));
  return Buffer.concat([length, text]);
}

function property(name, type, value) {
  const size = Buffer.alloc(4);
  size.writeInt32LE(value.length);
  const arrayIndexAndGuid = Buffer.alloc(5);
  return Buffer.concat([fstring(name), fstring(type), size, arrayIndexAndGuid, value]);
}

test("diagnostic fallback exporter decodes the isolated UE4 SaveGame properties", () => {
  const sequence = Buffer.alloc(4);
  sequence.writeInt32LE(17);
  const fixture = Buffer.concat([
    Buffer.from("GVAS", "ascii"),
    Buffer.alloc(48, 0x5a),
    property("Schema", "StrProperty", fstring("dor-diagnostic-v1")),
    property("Sequence", "IntProperty", sequence),
    property("Snapshot", "StrProperty", fstring("[DOR]|stage=重连|result=failed")),
    fstring("None"),
  ]);

  assert.deepEqual(decodeDORDiagnosticSave(fixture), {
    schema: "dor-diagnostic-v1",
    sequence: 17,
    snapshot: "[DOR]|stage=重连|result=failed",
  });
});

test("diagnostic fallback exporter rejects unrelated SaveGame schemas", () => {
  const sequence = Buffer.alloc(4);
  const fixture = Buffer.concat([
    Buffer.from("GVAS", "ascii"),
    Buffer.alloc(16),
    property("Schema", "StrProperty", fstring("other-schema")),
    property("Sequence", "IntProperty", sequence),
    property("Snapshot", "StrProperty", fstring("ignored")),
  ]);
  assert.throws(() => decodeDORDiagnosticSave(fixture), /Unsupported DOR diagnostic schema/);
});

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

function readFString(buffer, offset) {
  if (offset < 0 || offset + 4 > buffer.length) return null;
  const length = buffer.readInt32LE(offset);
  if (length === 0) return { value: "", next: offset + 4, bytes: 4 };
  const characterCount = Math.abs(length);
  const byteCount = length < 0 ? characterCount * 2 : characterCount;
  const start = offset + 4;
  const end = start + byteCount;
  if (characterCount > 1_000_000 || end > buffer.length) return null;
  const terminatorSize = length < 0 ? 2 : 1;
  const textEnd = Math.max(start, end - terminatorSize);
  const value = buffer.toString(length < 0 ? "utf16le" : "latin1", start, textEnd);
  return { value, next: end, bytes: 4 + byteCount };
}

function findTaggedProperty(buffer, propertyName, propertyType) {
  const marker = Buffer.alloc(4 + propertyName.length + 1);
  marker.writeInt32LE(propertyName.length + 1, 0);
  marker.write(propertyName, 4, "ascii");
  let offset = 0;
  while ((offset = buffer.indexOf(marker, offset)) >= 0) {
    const name = readFString(buffer, offset);
    const type = name && readFString(buffer, name.next);
    if (type?.value === propertyType && type.next + 8 <= buffer.length) {
      const size = buffer.readInt32LE(type.next);
      const arrayIndex = buffer.readInt32LE(type.next + 4);
      const guidFlagOffset = type.next + 8;
      const guidFlag = buffer[guidFlagOffset];
      if (size >= 0 && size <= buffer.length && arrayIndex === 0 && (guidFlag === 0 || guidFlag === 1)) {
        const valueOffset = guidFlagOffset + 1 + (guidFlag === 1 ? 16 : 0);
        return { size, valueOffset };
      }
    }
    offset += marker.length;
  }
  throw new Error(`Diagnostic SaveGame property not found: ${propertyName} (${propertyType})`);
}

function readStringProperty(buffer, name) {
  const property = findTaggedProperty(buffer, name, "StrProperty");
  const value = readFString(buffer, property.valueOffset);
  if (!value || value.bytes !== property.size) {
    throw new Error(`Diagnostic SaveGame string property is malformed: ${name}`);
  }
  return value.value;
}

function readIntProperty(buffer, name) {
  const property = findTaggedProperty(buffer, name, "IntProperty");
  if (property.size !== 4 || property.valueOffset + 4 > buffer.length) {
    throw new Error(`Diagnostic SaveGame integer property is malformed: ${name}`);
  }
  return buffer.readInt32LE(property.valueOffset);
}

export function decodeDORDiagnosticSave(buffer) {
  if (!Buffer.isBuffer(buffer) || buffer.length < 16 || buffer.toString("ascii", 0, 4) !== "GVAS") {
    throw new Error("Input is not an Unreal SaveGame (missing GVAS header). ");
  }
  const schema = readStringProperty(buffer, "Schema");
  if (schema !== "dor-diagnostic-v1") {
    throw new Error(`Unsupported DOR diagnostic schema: ${schema || "<empty>"}`);
  }
  const sequence = readIntProperty(buffer, "Sequence");
  const snapshot = readStringProperty(buffer, "Snapshot");
  return { schema, sequence, snapshot };
}

function optionValue(args, name) {
  const index = args.indexOf(name);
  if (index < 0) return undefined;
  if (!args[index + 1]) throw new Error(`${name} requires a path.`);
  return args[index + 1];
}

export function exportDORDiagnosticSave(args = process.argv.slice(2)) {
  const projectRoot = fileURLToPath(new URL("../../../", import.meta.url));
  const localAppData = process.env.LOCALAPPDATA;
  const defaultInput = localAppData
    ? path.join(localAppData, "FSD", "Saved", "SaveGames", "Mods", "DOR_Diagnostics_v1.sav")
    : "";
  const inputOption = optionValue(args, "--input") ?? defaultInput;
  if (!inputOption) throw new Error("LOCALAPPDATA is unavailable; pass --input <path>.");
  const input = path.resolve(inputOption);
  const decoded = decodeDORDiagnosticSave(fs.readFileSync(input));
  const text = [
    `schema=${decoded.schema}`,
    `sequence=${decoded.sequence}`,
    `source=${input}`,
    "",
    decoded.snapshot,
    "",
  ].join("\n");
  if (args.includes("--stdout")) {
    process.stdout.write(text);
    return { ...decoded, output: null };
  }
  const output = path.resolve(
    optionValue(args, "--output") ?? path.join(projectRoot, "log", "DOR_Diagnostics_v1.txt"),
  );
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, text, "utf8");
  console.log(`DOR diagnostic snapshot exported: ${output}`);
  return { ...decoded, output };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  exportDORDiagnosticSave();
}

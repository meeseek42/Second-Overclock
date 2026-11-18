import path from "node:path";
import { readJson, uniqueSorted } from "../shared/io.mjs";

export function packagePathFromObjectPath(objectPath) {
  return objectPath.split(".")[0];
}

export function typeName(value) {
  return String(value?.$type ?? "").split(",")[0].split(".").at(-1) ?? "";
}

export function scalar(value) {
  if (value === null || value === undefined) return value;
  if (typeof value !== "object") return value;
  if (Array.isArray(value)) return value.map(scalar);
  if (value.AssetPath?.AssetName) return value.AssetPath.AssetName;
  if (value.CultureInvariantString !== undefined) return value.CultureInvariantString;
  if (value.Value !== undefined && Object.keys(value).every((key) =>
    ["$type", "Value", "Name", "ArrayIndex", "PropertyGuid", "IsZero", "PropertyTagFlags",
      "PropertyTypeName", "PropertyTagExtensions", "EnumType", "InnerType", "ArrayType",
      "StructType", "StructGUID", "SerializationControl"].includes(key))) {
    return scalar(value.Value);
  }
  const result = {};
  for (const [key, item] of Object.entries(value)) {
    if (key !== "$type") result[key] = scalar(item);
  }
  return result;
}

export function propertyRows(document, packagePath) {
  const rows = [];
  function visit(value, trail = []) {
    if (Array.isArray(value)) {
      value.forEach((item, index) => visit(item, [...trail, index]));
      return;
    }
    if (!value || typeof value !== "object") return;
    if (typeof value.Name === "string" && value.$type?.includes("PropertyData")) {
      rows.push({
        evidenceRef: `asset:${packagePath}#${trail.join("/")}`,
        packagePath,
        name: value.Name,
        type: typeName(value),
        value: scalar(value.Value),
      });
    }
    for (const [key, item] of Object.entries(value)) {
      if (key !== "$type") visit(item, [...trail, key]);
    }
  }
  visit(document);
  return rows;
}

export function softObjectPaths(value, output = new Set()) {
  if (typeof value === "string") {
    if (value.startsWith("/Game/")) output.add(value);
  } else if (Array.isArray(value)) {
    value.forEach((item) => softObjectPaths(item, output));
  } else if (value && typeof value === "object") {
    Object.values(value).forEach((item) => softObjectPaths(item, output));
  }
  return uniqueSorted([...output]);
}

export function loadParsedIndex(index) {
  return new Map(index.rows.filter((row) => row.success).map((row) => [
    row.packagePath,
    { ...row, document: readJson(row.parsedJson) },
  ]));
}

export function resolveDependencyGraph(root, parsed) {
  const queue = [root];
  const visited = new Set();
  while (queue.length) {
    const current = queue.shift();
    if (visited.has(current)) continue;
    visited.add(current);
    const row = parsed.get(current);
    if (!row) continue;
    for (const reference of row.references ?? []) {
      if (!visited.has(reference) && parsed.has(reference)) queue.push(reference);
    }
  }
  return uniqueSorted([...visited]);
}

export function objectLeafName(objectPath) {
  return path.posix.basename(objectPath.split(".")[0]);
}

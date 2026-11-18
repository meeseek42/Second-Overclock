import { resolveInput } from "./paths.mjs";
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";

export function readJson(filename) {
  return JSON.parse(fs.readFileSync(resolveInput(filename), "utf8").replace(/^\uFEFF/, ""));
}

export function writeJson(filename, value) {
  fs.mkdirSync(path.dirname(filename), { recursive: true });
  fs.writeFileSync(filename, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

export function writeText(filename, value) {
  fs.mkdirSync(path.dirname(filename), { recursive: true });
  fs.writeFileSync(filename, value, "utf8");
}

export function exists(filename) {
  return fs.existsSync(filename);
}

export function sha256File(filename) {
  return new Promise((resolve, reject) => {
    const hash = crypto.createHash("sha256");
    const stream = fs.createReadStream(filename);
    stream.on("error", reject);
    stream.on("data", (chunk) => hash.update(chunk));
    stream.on("end", () => resolve(hash.digest("hex")));
  });
}

export function stableId(value) {
  return crypto.createHash("sha256").update(value).digest("hex").slice(0, 16);
}

export function uniqueSorted(values) {
  return [...new Set(values.filter((value) => value !== null && value !== undefined))].sort((a, b) =>
    String(a).localeCompare(String(b), "en"),
  );
}

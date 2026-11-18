import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

// Private transitional tool. The snapshot AND manifest are separately supplied;
// neither is part of the public source tree. It never extracts game packages.
const [snapshotArg, manifestArg, destinationArg] = process.argv.slice(2);
if (!snapshotArg || !manifestArg || !destinationArg) {
  throw Error('Usage: node Scripts/Import-DORInputSnapshot.mjs <private-snapshot-root> <private-manifest.json> <NEW-private-inputs-root>');
}
const snapshot = fs.realpathSync(snapshotArg);
const destination = path.resolve(destinationArg);
const manifest = JSON.parse(fs.readFileSync(manifestArg, 'utf8').replace(/^\uFEFF/, ''));
if (manifest.schema !== 1 || !Array.isArray(manifest.files) || !manifest.files.length) throw Error('Invalid private snapshot manifest');
if (fs.existsSync(destination)) throw Error('Destination must be new; no existing files will be overwritten');
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
function member(root, relative) {
  if (typeof relative !== 'string' || /[:\\]/.test(relative) || relative.startsWith('/') || relative.split('/').some(p => !p || p === '.' || p === '..')) throw Error('Unsafe snapshot member: ' + relative);
  const absolute = path.resolve(root, relative);
  if (!absolute.startsWith(root + path.sep)) throw Error('Snapshot member escapes root');
  return absolute;
}
const seen = new Set();
for (const row of manifest.files) {
  const key = row.path?.toLowerCase();
  if (seen.has(key)) throw Error('Duplicate snapshot member: ' + row.path);
  seen.add(key);
  const source = member(snapshot, row.path);
  if (!fs.realpathSync(source).startsWith(snapshot + path.sep)) throw Error('Snapshot source resolves outside root: ' + row.path);
  const bytes = fs.readFileSync(source);
  if (!Number.isSafeInteger(row.bytes) || bytes.length !== row.bytes || !/^[a-f0-9]{64}$/i.test(row.sha256 ?? '') || digest(bytes) !== row.sha256.toLowerCase()) throw Error('Snapshot identity mismatch: ' + row.path);
}
// Create only after all inputs pass. A partial copy on I/O failure remains private
// and is never treated as a prepared input set (receipt is written last).
fs.mkdirSync(destination, { recursive: true });
for (const row of manifest.files) {
  const target = member(destination, row.path);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.copyFileSync(member(snapshot, row.path), target, fs.constants.COPYFILE_EXCL);
  if (digest(fs.readFileSync(target)) !== row.sha256.toLowerCase()) throw Error('Copied snapshot identity mismatch: ' + row.path);
}
const receipt = { schema: 1, role: 'Private historical input snapshot; not fresh-game preparation', files: manifest.files.length, bytes: manifest.files.reduce((n,r)=>n+r.bytes,0), manifestSha256: digest(fs.readFileSync(manifestArg)), complete: true };
fs.writeFileSync(path.join(destination, 'dor-snapshot-receipt.json'), JSON.stringify(receipt, null, 2) + '\n', { flag: 'wx' });
console.log(JSON.stringify(receipt, null, 2));

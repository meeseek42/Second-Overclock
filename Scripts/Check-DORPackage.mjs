import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
const [pak,extracted,directory,build]=process.argv.slice(2);
const text=p=>fs.readFileSync(p,'utf8').replace(/^\uFEFF/,'');
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const list=text(path.join(directory,'pak-list.log')),verify=text(path.join(directory,'pak-verify.log'));
const members=[...list.matchAll(/Display: "([^"]+)" offset: (\d+), size: (\d+) bytes, sha1: ([A-Fa-f0-9]+), compression: ([^.]+)\./g)].map(m=>m[1]);
const sources=text(path.join(directory,'pak-response.txt')).trim().split(/\r?\n/).map(line=>{
  const m=/^"([^"]+)" "\.\.\/\.\.\/\.\.\/FSD\/([^"]+)"$/.exec(line);assert.ok(m,line);
  return {source:m[1],member:m[2]};
});
assert.ok(list.includes('Mount point ../../../FSD/'));
assert.equal(new Set(members).size,members.length);
assert.deepEqual([...members].sort(),sources.map(x=>x.member).sort());
assert.ok(members.every(m=>m==='AssetRegistry.bin'||m.startsWith('Content/DualOverclockReward/')));
assert.ok(verify.includes('healthy, '+members.length+' files checked'));
const identities=sources.map(({source,member})=>{const p=path.join(extracted,member),sha256=hash(source);assert.equal(hash(p),sha256,member);return {path:member,bytes:fs.statSync(p).size,sha256};});
const cooked=JSON.parse(text(path.join(directory,'cooked-containment-validation.json')));
assert.ok(cooked.passed&&cooked.build===build);
const semantic=JSON.parse(text(path.join(directory,'cooked-build-readback.json')));
assert.ok(semantic.passed&&semantic.build===build);
for(const input of [...cooked.inputs,...semantic.records.flatMap(x=>x.inputs)]){
  const relative=input.relative;assert.ok(relative&&!relative.startsWith('../'),input.input);
  assert.equal(identities.find(x=>x.path==='Content/DualOverclockReward/'+relative)?.sha256,input.sha256,input.input);
}
fs.writeFileSync(path.join(directory,'pak-member-identities.json'),JSON.stringify(identities,null,2)+'\n');
const result={schema:'dor.package-validation.v1',passed:true,build,path:pak,bytes:fs.statSync(pak).size,sha256:hash(pak),exactMembers:members.length,scope:'Exact packaged bytes and serialized readback; no gameplay claim'};
fs.writeFileSync(path.join(directory,'package-validation.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result,null,2));

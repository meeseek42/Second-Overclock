import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';


export const hash = value => crypto.createHash('sha256').update(value).digest('hex');
export const fileHash = filename => hash(fs.readFileSync(filename));
export const json = filename => JSON.parse(fs.readFileSync(filename, 'utf8').replace(/^\uFEFF/, ''));
export const canonical = value => Array.isArray(value) ? `[${value.map(canonical).join(',')}]` : value && typeof value === 'object' ? `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${canonical(value[k])}`).join(',')}}` : JSON.stringify(value);
export function requireFact(value, message) { if (!value) throw Error(message); }
export function writeJson(filename, value) { fs.mkdirSync(path.dirname(filename), {recursive:true}); fs.writeFileSync(filename, JSON.stringify(value,null,2)+'\n'); }
export function member(root, name) {
  requireFact(typeof name === 'string' && name && !name.includes('\\') && !name.split('/').includes('..') && !path.isAbsolute(name), `Unsafe prepared member: ${name}`);
  const target = path.resolve(root,name), realRoot = fs.realpathSync(root);
  requireFact(target.startsWith(path.resolve(root)+path.sep), `Member escapes root: ${name}`);
  requireFact(fs.realpathSync(target).startsWith(realRoot+path.sep), `Member link escapes root: ${name}`);
  return target;
}
export const fileSha1 = filename => crypto.createHash('sha1').update(fs.readFileSync(filename)).digest('hex');
export function inventory(root, names) { return names.map(name => { const f=member(root,name); return {path:name,bytes:fs.statSync(f).size,sha256:fileHash(f),sha1:fileSha1(f)}; }); }
export function verifyMembers(root, members) {
  requireFact(Array.isArray(members) && members.length>0,'Empty prepared closure');
  const seen=new Set();
  for(const m of members) { requireFact(!seen.has(m.path),'Duplicate prepared member '+m.path);seen.add(m.path); const f=member(root,m.path);requireFact(fs.statSync(f).size===m.bytes && fileHash(f)===m.sha256 && fileSha1(f)===m.sha1,'Prepared member changed: '+m.path); }
  return seen;
}
export function verifySourceReceipt(filename, profileFile) {
  const receipt=json(filename), root=path.dirname(path.resolve(filename)), profile=json(profileFile);
  requireFact(receipt.schema==='dor.native-source-receipt.v2' && receipt.complete===true,'Native preparation incomplete');
  requireFact(receipt.profileId===profile.profileId && receipt.profileSha256===fileHash(profileFile),'Native profile changed');
  requireFact(receipt.semanticValidation?.consumedTargetExports===true && receipt.semanticValidation?.shippingDefaultsCertified===false,'Missing truthful consumed-source validation');
  for(const key of ['pak','shippingExe']) requireFact(/^[a-f0-9]{64}$/.test(receipt.source[key]?.sha256??'') && receipt.source[key].bytes>0,'Missing original provenance '+key);
  const members=verifyMembers(root,receipt.members);
  requireFact(members.has('native-facts.json') && members.has('dependency-inventory.json'),'Native receipt lacks facts/inventory');
  const facts=json(member(root,'native-facts.json'));
  requireFact(facts.schema==='dor.prepared-native-facts.v2' && facts.profileSha256===receipt.profileSha256,'Native facts profile mismatch');
  requireFact(facts.roots.length===160 && facts.inputs.length===profile.inputs.length && facts.shippingDefaultsCertified===false,'Native fact count/stage');
  const actualInputs=new Map(facts.inputs.map(i=>[i.sourceObjectPath,i]));
  requireFact(actualInputs.size===facts.inputs.length,'Duplicate native input');
  for(const expected of profile.inputs) {
    const actual=actualInputs.get(expected.sourceObjectPath);
    requireFact(actual?.classPath===expected.classPath && canonical(actual.fields)===canonical(expected.fields),'Native consumed facts changed: '+expected.factId);
    for(const [field,value] of Object.entries(expected.fields)) {
      const observation=actual.fieldObservations?.[field];
      requireFact(observation && canonical(observation.expectedValue)===canonical(value),'Missing consumed field observation '+expected.factId+':'+field);
      if(observation.state==='observed-serialized')requireFact(canonical(observation.value)===canonical(value),'Serialized fact changed '+expected.factId+':'+field);
      else requireFact(observation.state==='established-recipe-expectation' && facts.pendingLoadedObjectChecks.some(p=>p.sourceObjectPath===expected.sourceObjectPath && p.field===field && canonical(p.expectedValue)===canonical(value)),'Missing deferred native equality check '+expected.factId+':'+field);
    }
  }
  for(const expected of profile.roots) {
    const actual=facts.roots.find(r=>r.sourceObjectPath===expected.sourceObjectPath);
    for(const field of ['nativeGuid','orderedLeaves','orderedLeafObjectPaths','category'])requireFact(canonical(actual?.[field])===canonical(expected[field]),'Native root changed '+expected.factId+':'+field);
  }
  for(const expected of profile.carrierSources)requireFact(facts.carrierSources.some(c=>c.sourceObjectPath===expected.sourceObjectPath && c.superClassPath===expected.superClassPath),'Carrier superclass changed '+expected.factId);
  requireFact(canonical(facts.carriers)===canonical(profile.carriers),'Carrier expectation changed');
  requireFact(receipt.semanticValidation.pendingLoadedObjectChecks===facts.pendingLoadedObjectChecks.length,'Pending loaded-object check count changed');
  const packages=json(member(root,'dependency-inventory.json')).packages, byPackage=new Map(packages.map(p=>[p.packagePath,p]));
  requireFact(byPackage.size===packages.length,'Duplicate native package');
  for(const p of packages) {
    requireFact(p.packagePath.startsWith('/Game/') && p.members.some(m=>m.path===p.packagePath.slice(6)+'.uasset'),'Invalid canonical native package');
    for(const m of p.members) {
      requireFact(members.has('Content/'+m.path),'Native package missing from closure: '+m.path);
      const bound=receipt.members.find(x=>x.path==='Content/'+m.path);
      requireFact(bound.sha256===m.sha256 && bound.bytes===m.bytes,'Native inventory hash changed: '+m.path);
    }
  }
  const packageIdentities=new Set([...byPackage.keys()].map(p=>p.toLowerCase()));
  for(const fact of [...profile.inputs,...profile.roots,...profile.carrierSources]) requireFact(packageIdentities.has(fact.sourceObjectPath.split('.')[0].toLowerCase()),'Missing native fact package: '+fact.factId);
  return {receipt,root,facts,profile};
}
export function verifyPreparedInputs(filename, publicRoot) {
  const receipt=json(filename), root=path.dirname(path.dirname(path.resolve(filename)));
  requireFact(receipt.schema==='dor.public-prepared.v1' && receipt.complete===true,'Authoring preparation incomplete');
  const names=verifyMembers(root,receipt.members);
  for(const m of receipt.authoringInputs) requireFact(fileHash(member(publicRoot,m.path))===m.sha256,'Authoring input changed: '+m.path);
  const native=verifySourceReceipt(member(root,'source-receipt.json'),path.join(publicRoot,'Authoring/native-compatibility.json'));
  requireFact(fileHash(member(root,'source-receipt.json'))===receipt.sourceReceiptSha256,'Native receipt changed');
  for(const name of ['gf01-contract.json',...['gf02','gf03','gf04','remaining'].flatMap(f=>[f+'-recipes.json',f+'-resolved-native-inputs.json']),'runtime-pairs.json','runtime-pairs.csv','pair-test-index.csv']) requireFact(names.has('authoring/'+name),'Missing authoring member '+name);
  return {receipt,root,...native};
}

export function loadPublicFamily(filename,family,publicRoot) {
  const verified=verifyPreparedInputs(filename,publicRoot), folder=path.join(verified.root,'authoring');
  const contractFile=path.join(folder,family+'-recipes.json'),evidenceFile=path.join(folder,family+'-resolved-native-inputs.json');
  const contract=json(contractFile),evidence=json(evidenceFile);
  requireFact(contract.provenance?.schema==='dor.public-prepared.v1' && evidence.provenance?.schema==='dor.public-prepared.v1','Public provenance missing');
  requireFact(evidence.contractSha256===fileHash(contractFile),'Prepared contract/evidence binding changed');
  return {contract,evidence,contractSha256:fileHash(contractFile),evidenceSha256:fileHash(evidenceFile)};
}

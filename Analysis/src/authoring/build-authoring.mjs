import fs from 'node:fs';
import path from 'node:path';
import {projectDir} from '../shared/paths.mjs';
import {buildRuntimeManifest} from '../runtime/build-runtime-manifest.mjs';
import {validateNativeSelection} from '../runtime/native-numeric-policy.mjs';
import {effectiveNumericResult} from '../runtime/effective-numeric-output.mjs';
import {validateGF02ResolvedInputs} from '../runtime/gf02-contract.mjs';
import {validateGF03ResolvedInputs} from '../runtime/gf03-contract.mjs';
import {validateGF04ResolvedInputs} from '../runtime/gf04-contract.mjs';
import {hash,fileHash,json,canonical,requireFact,writeJson,inventory,verifySourceReceipt,verifyPreparedInputs} from './prepared-inputs.mjs';

export const familyNames=['gf02','gf03','gf04','remaining'];
export function withoutOverrides(value) {
  if(Array.isArray(value))return value.map(withoutOverrides);
  if(value && typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([k])=>k!=='outputOverride').map(([k,v])=>[k,withoutOverrides(v)]));
  return value;
}
function outputPath(op) { return op.mergedObjectPath??op.adjustedObjectPath; }
export function compileRecipe(row) {
  const copies=[...row.operations,...(row.generatedNumericLeaves??[])], byPath=new Map();
  const owners=new Set(copies);
  function checkOverrideLocations(value) {
    if(!value || typeof value!=='object')return;
    if(Object.hasOwn(value,'outputOverride')) requireFact(owners.has(value),'Override is only supported on an existing numeric leaf operation');
    for(const child of Object.values(value))checkOverrideLocations(child);
  }
  checkOverrideLocations(row);
  for(const op of row.operations) validateNativeSelection(op);
  const emitted=new Map(copies.filter(op=>['emit-native-numeric-leaf','emit-native-flat-adjustment'].includes(op.operation)).map(op=>[outputPath(op),op]));
  const paths=op=>outputPath(op)?[outputPath(op)]:(op.outputLeafObjectPaths??[]).filter(p=>emitted.has(p));
  const preserved=op=>op.operation.startsWith('ordered-native-') || op.operation.startsWith('preserve-native-') || op.integerRule || op.consumer?.kind?.includes('integer');
  for(const op of copies) {
    if(op.outputOverride!==undefined) {
      requireFact(!preserved(op),'Override unsupported on preservation/integer/non-numeric operation');
      const candidates=paths(op);requireFact(candidates.length===1,'Override requires one generated numeric output');
      const name=candidates[0], leaf=emitted.get(name);
      requireFact(leaf && !row.operations.some(parent=>preserved(parent) && paths(parent).includes(name)),'Override unsupported on preservation/integer/non-numeric operation');
      effectiveNumericResult({...leaf,outputOverride:op.outputOverride});
      const prior=byPath.get(name);
      requireFact(!prior || canonical(prior)===canonical(op.outputOverride),'Conflicting duplicate output override '+name);
      byPath.set(name,op.outputOverride);
    }
  }
  for(const op of copies) {
    const candidates=paths(op), name=candidates.length===1?candidates[0]:null, override=byPath.get(name);
    if(override!==undefined)op.outputOverride=structuredClone(override);
    if(['emit-native-numeric-leaf','emit-native-flat-adjustment'].includes(op.operation)) {
      requireFact(Math.fround(op.selection.value)===op.serializedResult,'Recorded native formula/result changed: '+name);
      op.serializedResult=effectiveNumericResult(op);
    }
  }
  return row;
}
function maintainedFiles(root) {
  const out=[];
  function visit(relative){for(const item of fs.readdirSync(path.join(root,relative),{withFileTypes:true})) {
    const name=relative+'/'+item.name;if(item.isDirectory())visit(name);else out.push(name);
  }}
  visit('Authoring');
  for(const dir of ['Analysis/src/runtime','Analysis/src/authoring','Mod/Source/DOREditor'])visit(dir);
  for(const name of ['Analysis/data/manual/numeric-stack-policy.json','Analysis/data/manual/high-load-policy.json','Analysis/data/manual/pair-overrides.json','Analysis/data/manual/implementation-overrides.json','Analysis/config/weapon-mirrors.vertical.json','Mod/Generated/display-overrides.json','Scripts/Generate-DORAssets.ps1','Scripts/Cook-DORAssets.ps1','Scripts/Build-DORWeaponMirrors.ps1','Scripts/DOR-Prepared.ps1','Scripts/Build-DORNativeStagePlan.mjs','Scripts/Build-DORAuthoring.mjs','Scripts/Prepare-DORNativeInputs.mjs'])out.push(name);
  return [...new Set(out)].sort();
}
export function buildAuthoring({sourceReceipt,publicRoot=projectDir}) {
  publicRoot=path.resolve(publicRoot);
  const native=verifySourceReceipt(sourceReceipt,path.join(publicRoot,'Authoring/native-compatibility.json'));
  const {profile,facts,root}=native, read=name=>json(path.join(publicRoot,name));
  const inputs=maintainedFiles(publicRoot), families={};
  for(const [name,expected] of Object.entries(profile.authoringLocks??{})) {
    const value=read(name);
    requireFact(hash(canonical(name.startsWith('Authoring/recipes/')?withoutOverrides(value):value))===expected,'Identity/order/source or unsupported authoring edit: '+name);
  }
  requireFact(Object.keys(profile.authoringLocks??{}).length>=10,'Compatibility profile lacks reviewed authoring locks');
  const base=read('Authoring/runtime-base.json'), identities=read('Authoring/stable-identities.json').pairs;
  const overclocks=read('Authoring/overclock-identities.json').overclocks, admission=read('Authoring/admission.json');
  requireFact(base.pairs.length===456 && identities.length===456 && overclocks.length===160,'Public identity count changed');
  requireFact(admission.pairs.length===456 && admission.pairs.filter(x=>x.admitted).length===453,'Admission changed');
  base.pairs.forEach((row,i)=>{requireFact(row.pairId===identities[i].pairId && row.testIndex===identities[i].testIndex,'Ordered runtime identity changed');requireFact(admission.pairs[i].pairId===row.pairId && admission.pairs[i].runtimeCode===row.testIndex,'Admission identity changed');});
  const authoring=path.join(root,'authoring'), receiptPath=path.join(authoring,'prepared-inputs.json');
  fs.mkdirSync(authoring,{recursive:true}); if(fs.existsSync(receiptPath))fs.unlinkSync(receiptPath);
  const provenance={schema:'dor.public-prepared.v1',profileId:profile.profileId,profileSha256:fileHash(path.join(publicRoot,'Authoring/native-compatibility.json')),sourceReceiptSha256:fileHash(sourceReceipt),adapterVersion:1};
  const gf01=read('Authoring/gf01-pairs.json');
  const gf01Contract={...gf01,schema:gf01.legacySchema,provenance};delete gf01Contract.legacySchema;
  requireFact(gf01Contract.pairs.length===155,'GF01 count');
  writeJson(path.join(authoring,'gf01-contract.json'),gf01Contract);
  for(const family of familyNames) {
    const authored=read(`Authoring/recipes/${family}.json`), contract={...authored,schema:authored.legacySchema,provenance,recipes:authored.recipes.map(compileRecipe)};
    delete contract.legacySchema;delete contract.family;
    const contractFile=path.join(authoring,`${family}-recipes.json`);writeJson(contractFile,contract);
    const evidence={schema:`dor.${family}.resolved-native-inputs.v1`,contractSha256:fileHash(contractFile),shippingExeSha256:native.receipt.source.shippingExe.sha256.toUpperCase(),expectationShippingExeSha256:profile.source.shippingExeSha256.toUpperCase(),provenance,
      inputs:facts.inputs.map(input=>({...input,nativeEvidence:[{factId:input.factId,profileSha256:provenance.profileSha256,sourceIdentity:native.receipt.source.shippingExe.sha256,expectationBasis:profile.source.shippingExeSha256,validationStage:'loaded-UObject',shippingDefaultProved:false}]})),
      roots:facts.roots,carriers:facts.carriers,carrierSources:facts.carrierSources};
    if(family==='gf02')validateGF02ResolvedInputs(contract,evidence);
    if(family==='gf03')validateGF03ResolvedInputs(contract,evidence);
    if(family==='gf04')validateGF04ResolvedInputs(contract,evidence);
    const evidenceFile=path.join(authoring,`${family}-resolved-native-inputs.json`);writeJson(evidenceFile,evidence);
    families[family]={contract,evidence,contractSha256:fileHash(contractFile),evidenceSha256:fileHash(evidenceFile)};
  }
  requireFact(Object.values(families).reduce((n,f)=>n+f.contract.recipes.length,0)===294,'Recipe count');
  const manifest=buildRuntimeManifest({runtimeManifest:path.join(authoring,'runtime-pairs.json'),runtimePairsCsv:path.join(authoring,'runtime-pairs.csv'),runtimeTestIndexCsv:path.join(authoring,'pair-test-index.csv')},{base,overclocks,families,provenance});
  for(const row of gf01Contract.pairs) {
    const actual=manifest.pairs.find(p=>p.pairId===row.pairId);
    requireFact(actual.testIndex===row.testIndex && canonical(actual.finalLeafObjectPaths)===canonical(row.orderedLeafObjectPaths),'GF01 immutable baseline changed '+row.pairId);
  }
  // Both notice registration and presentation read this maintained input next
  // to ManifestFilename. Preserve exact bytes and bind it before UE launch.
  fs.copyFileSync(path.join(publicRoot,'Mod/Generated/display-overrides.json'),path.join(authoring,'display-overrides.json'));
  const names=['gf01-contract.json',...familyNames.flatMap(f=>[`${f}-recipes.json`,`${f}-resolved-native-inputs.json`]),'runtime-pairs.json','runtime-pairs.csv','pair-test-index.csv','display-overrides.json'];
  const receipt={...provenance,complete:true,sourceReceiptSha256:fileHash(sourceReceipt),authoringInputs:inventory(publicRoot,inputs),members:inventory(root,['source-receipt.json',...names.map(n=>'authoring/'+n)]),runtimeManifestHash:manifest.manifestHash};
  const pending=path.join(authoring,'prepared-inputs.pending.json');writeJson(pending,receipt);verifyPreparedInputs(pending,publicRoot);fs.renameSync(pending,receiptPath);
  return {receiptPath,manifestHash:manifest.manifestHash,pairs:manifest.pairs.length,admitted:453};
}

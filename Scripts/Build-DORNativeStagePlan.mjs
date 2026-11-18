import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {verifyPreparedInputs,json,fileHash,requireFact} from '../Analysis/src/authoring/prepared-inputs.mjs';
const [receipt,phase,templateContent,output]=process.argv.slice(2);
requireFact(['generate','cook'].includes(phase),'Explicit native staging phase required');
const publicRoot=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const verified=verifyPreparedInputs(receipt,publicRoot),inventory=json(path.join(verified.root,'dependency-inventory.json'));
const policyFile=path.join(publicRoot,'Authoring/native-editor-staging.json'),policy=json(policyFile);
requireFact(policy.schema==='dor.native-editor-staging.v1','Unsupported editor staging policy');
const packages=new Map(inventory.packages.map(p=>[p.packagePath.toLowerCase(),p]));
const selected=policy.packages.filter(p=>p.roles.includes(phase));
requireFact(new Set(policy.packages.map(p=>p.packagePath.toLowerCase())).size===policy.packages.length,'Duplicate staging identity');
const selectedNames=new Set(selected.map(p=>p.packagePath.toLowerCase()));
const mml='/Game/WeaponsNTools/MicroMissileLauncher/';
const mandatory=['PRJ_MicroMissile','PRJ_MicroMissile_Buck','PRJ_MicroMissile_Cluster','BP_ClusterFragment_MicroMissileLauncher','PRJ_MicroMissile_Mine_Head','PRJ_MicroMissile_PlasmaBurster','CRV_MML_Acceleration','CRV_MML_InstantAcceleration'].map(n=>mml+n);
mandatory.push('/Game/WeaponsNTools/Cryospray/PRJ_Cryocannon','/Game/WeaponsNTools/Cryospray/PRJ_Crye_SnowBall','/Game/WeaponsNTools/Cryospray/Particles/P_Snowball_Explosion_Cryocannon','/Game/Audio/SFX/WeaponsNTools/WPN_CryoSpray/SnowballExplosion_Cryocannon_Cue');
if(phase==='generate')for(const [name,rows] of [['native consumed source',verified.profile.inputs],['native root',verified.profile.roots],['native carrier source',verified.profile.carrierSources]])for(const row of rows){
  const pkg=row.sourceObjectPath.split('.')[0];requireFact(selectedNames.has(pkg.toLowerCase()),'Missing staged '+name+': '+pkg);
}
for(const pkg of mandatory)requireFact(selectedNames.has(pkg.toLowerCase()),'Missing explicit '+phase+' consumer: '+pkg);
requireFact(!selectedNames.has((mml+'Particles/NS_MicroMissile_ShotImpact').toLowerCase()),'Unconsumed Niagara must remain provenance-only');
const rows=selected.map(p=>{
  const original=packages.get(p.packagePath.toLowerCase());requireFact(original,'No receipt-backed '+phase+' package '+p.packagePath);
  requireFact(original.members.some(m=>m.path.endsWith('.uasset')),'Missing native uasset '+p.packagePath);
  return {packagePath:original.packagePath,roles:p.roles,consumers:p.consumers,members:original.members};
});
// Refuse stale incidental originals; never quietly load leftovers from a prior phase.
for(const pkg of inventory.packages)if(!selectedNames.has(pkg.packagePath.toLowerCase()))for(const member of pkg.members){
  requireFact(!fs.existsSync(path.join(templateContent,member.path)),'Unselected original remains mounted: '+member.path);
}
const result={schema:'dor.native-stage-plan.v1',phase,sourceReceiptSha256:fileHash(path.join(verified.root,'source-receipt.json')),preparedInputsSha256:fileHash(receipt),policySha256:fileHash(policyFile),packages:rows,counts:{packages:rows.length,members:rows.reduce((n,p)=>n+p.members.length,0),provenancePackages:inventory.packages.length},coverage:{nativeSourceRecords:phase==='generate'?verified.profile.inputs.length:0,nativeRoots:phase==='generate'?verified.profile.roots.length:0,explicitMMLSeeds:8,copiedCryoParticleAndAudio:true,hardImportsExpanded:false}};
fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
console.log(phase+': '+result.counts.packages+' native packages / '+result.counts.members+' members; full provenance cache retained');

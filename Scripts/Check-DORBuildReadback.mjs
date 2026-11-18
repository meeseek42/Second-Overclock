import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {assetSemantics,sha256} from './DOR-AssetSemantics.mjs';
import {verifyPreparedInputs,json} from '../Analysis/src/authoring/prepared-inputs.mjs';

const [kind,scratch,build,evidenceDirectory,preparedInputs='-',baselineDirectory='',baselineBuild='',baselineManifest='']=process.argv.slice(2);
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const batch=json(path.join(scratch,'batch-result.json')),offsets=json(path.join(scratch,'offsets.json'));
const assetRoot=path.dirname(batch.results.find(x=>path.basename(x.input)==='InitCave.uasset').input);
const verified=preparedInputs==='-'?null:verifyPreparedInputs(preparedInputs,root);
const runtime=verified?json(path.join(verified.root,'authoring/runtime-pairs.json')):null;
const hashTag=s=>crypto.createHash('sha1').update(s).digest('hex').toUpperCase();
const replacements=[[build,'<BUILD>'],[hashTag(build),'<BUILD-SHA1>']];
if(runtime)replacements.push([runtime.manifestHash,'<MANIFEST>']);
const baseReplacements=baselineBuild?[[baselineBuild,'<BUILD>'],[hashTag(baselineBuild),'<BUILD-SHA1>'],[baselineManifest,'<MANIFEST>']]:[];
const directory=path.join(evidenceDirectory,kind+'-semantics');fs.mkdirSync(directory,{recursive:true});
const overrides=new Map();
if(verified){
  for(const family of ['gf02','gf03','gf04','remaining']){
    const contract=json(path.join(verified.root,'authoring/'+family+'-recipes.json'));
    for(const row of contract.recipes)for(const op of row.operations??[]){
      if(!op.outputOverride)continue;
      const object=op.mergedObjectPath??op.adjustedObjectPath;
      assert.ok(object&&op.valueField,'Unsupported override readback');
      overrides.set(object.split('.').at(-1),{field:op.valueField,value:op.serializedResult??op.outputOverride.value,computed:op.selection.value,code:row.testIndex,sourceAmounts:op.nativeSources?.map(s=>s.amount)});
    }
  }
}
const records=[],changes=[],overrideChecks=[];
if(baselineDirectory){
  const baseline=json(path.join(baselineDirectory,kind+'-build-readback.json'));
  assert.deepEqual(batch.results.map(r=>path.basename(r.input,'.uasset')).sort(),baseline.records.map(r=>r.owner).sort(),'Complete same-kind asset membership');
}
for(const result of batch.results){
  assert.ok(result.success,'Inspector failed: '+result.input);
  const asset=json(result.output),owner=path.basename(result.input,'.uasset');
  const row=offsets.find(x=>x.input===result.input);assert.ok(row,owner+' offsets');
  const sem=assetSemantics(asset,row,replacements);
  const override=overrides.get(owner);
  if(override){
    const target=sem.defaults.find(x=>x.name===owner);
    const field=target?.data.find(x=>x.Name?.toLowerCase()===override.field.toLowerCase());
    assert.equal(field?.Value,override.value,owner+' authored emitted value');
    overrideChecks.push({owner,...override,actual:field.Value});
  }
  const filename=path.join(directory,owner+'.json');
  assert.ok(!records.some(x=>x.owner===owner),'Ambiguous asset basename '+owner);
  fs.writeFileSync(filename,JSON.stringify(sem)+'\n');
  const inputs=[{input:row.input,sha256:row.sha256},...row.companions].map(x=>({...x,relative:path.relative(assetRoot,x.input).replaceAll('\\','/')}));
  records.push({owner,inputs,semantics:filename,sha256:sha256(fs.readFileSync(filename)),functions:sem.functions.length,defaults:sem.defaults.length});
  if(baselineDirectory){
    const oldFile=path.join(baselineDirectory,kind+'-semantics',owner+'.json');assert.ok(fs.existsSync(oldFile),'Missing same-kind baseline '+owner);
    // Baseline semantic files may have been captured without replacement tags.
    let old=JSON.stringify(json(oldFile));
    for(const [from,to] of baseReplacements)if(from)old=old.replaceAll(from,to);
    const previous=JSON.parse(old),current=structuredClone(sem);
    if(override){
      const a=previous.defaults.find(x=>x.name===owner)?.data.find(x=>x.Name?.toLowerCase()===override.field.toLowerCase());
      const b=current.defaults.find(x=>x.name===owner)?.data.find(x=>x.Name?.toLowerCase()===override.field.toLowerCase());
      assert.equal(a?.Value,override.computed,owner+' baseline computed amount');
      a.Value=b.Value='<DECLARED-OUTPUT-OVERRIDE>';
    }
    if(JSON.stringify(previous)!==JSON.stringify(current)){
      const oldFunctions=new Map(previous.functions.map(x=>[x.name,x]));
      const affected=current.functions.filter(x=>JSON.stringify(x)!==JSON.stringify(oldFunctions.get(x.name))).map(x=>x.name);
      changes.push({owner,functions:affected,defaultsChanged:JSON.stringify(previous.defaults)!==JSON.stringify(current.defaults),previous:oldFile,current:filename});
    }
  }
}
if(verified){assert.equal(runtime.pairs.length,456);assert.equal(overrideChecks.length,overrides.size,'Every authored override inspected');}
const report={schema:'dor.serialized-build-readback.v1',kind,build,assetRoot,passed:changes.length===0,scope:'Emitted function signatures, flags, resolved calls, statement control flow and CDO/native properties; no gameplay execution.',assets:records.length,functions:records.reduce((n,r)=>n+r.functions,0),defaults:records.reduce((n,r)=>n+r.defaults,0),overrideChecks,baselineDirectory:baselineDirectory||null,unexplainedChanges:changes,records};
fs.writeFileSync(path.join(evidenceDirectory,kind+'-build-readback.json'),JSON.stringify(report,null,2)+'\n');
assert.equal(changes.length,0,'Unexplained same-kind serialized semantic changes; see '+kind+'-build-readback.json');
console.log(kind+': '+report.assets+' assets / '+report.functions+' functions / '+overrideChecks.length+' authored overrides read back');

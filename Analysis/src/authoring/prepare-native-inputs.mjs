import fs from 'node:fs';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {runInspector} from '../parse/parse-assets.mjs';
import {deriveNativeFacts,validateOriginalRoot} from './native-semantics.mjs';
export {validateOriginalRoot} from './native-semantics.mjs';
import {json,fileHash,canonical,requireFact,writeJson,inventory,verifySourceReceipt} from './prepared-inputs.mjs';

async function streamHash(filename) {
  const {createHash}=await import('node:crypto'), digest=createHash('sha256');
  for await(const chunk of fs.createReadStream(filename))digest.update(chunk);
  return digest.digest('hex');
}
export async function checkOriginals(pak,shippingExe,profile) {
  const source={};
  // Whole-game fingerprints are provenance only. Consumed semantics admit sources.
  for(const [key,filename,pin] of [['shippingExe',shippingExe,'shippingExeSha256'],['pak',pak,'pakSha256']]) {
    const actual=await streamHash(filename);
    source[key]={bytes:fs.statSync(filename).size,sha256:actual,historicalSha256:profile.source[pin],matchesHistorical:actual===profile.source[pin]};
  }
  return source;
}
function run(executable,args) { return new Promise((resolve,reject)=>{
  const child=spawn(executable,args,{stdio:'inherit',windowsHide:true});child.on('error',reject);child.on('exit',code=>code===0?resolve():reject(Error(`Native extraction exited ${code}`)));
}); }
function walk(root) {
  const files=[];for(const item of fs.readdirSync(root,{withFileTypes:true})) { const f=path.join(root,item.name);if(item.isDirectory())files.push(...walk(f));else files.push(f); }return files;
}
function collectSelectors(value,consumer,selected,ownership) {
  if(typeof value==='string' && value.startsWith('/Game/')) {
    const pkg=value.split('.')[0];
    if(pkg==='/Game/DualOverclockReward' || pkg.startsWith('/Game/DualOverclockReward/') || ownership.has(pkg))return;
    requireFact(!pkg.includes('%') && !pkg.includes('..'),'Invalid native selector '+pkg);
    const identity=[...selected.keys()].find(p=>p.toLowerCase()===pkg.toLowerCase())??pkg;
    if(!selected.has(identity))selected.set(identity,new Set());selected.get(identity).add(consumer);
  }else if(Array.isArray(value))for(const x of value)collectSelectors(x,consumer,selected,ownership);
  else if(value && typeof value==='object')for(const x of Object.values(value))collectSelectors(x,consumer,selected,ownership);
}
export async function prepareNativeInputs({pak,shippingExe,unrealPak,dotnet,inspector,outputDirectory,publicRoot,reuseReceipt}) {
  const profileFile=path.join(publicRoot,'Authoring/native-compatibility.json'),profile=json(profileFile);
  const source=await checkOriginals(pak,shippingExe,profile);
  const output=path.resolve(outputDirectory);
  requireFact(!fs.existsSync(output) || fs.readdirSync(output).length===0,'Native output must be empty/private: '+output);
  requireFact(fs.existsSync(inspector),'Built UAssetInspector required');
  fs.mkdirSync(output,{recursive:true});
  const profileSha256=fileHash(profileFile), selected=new Map();
  const dependencies=json(path.join(publicRoot,'Authoring/native-dependencies.json')),ownership=new Map(dependencies.ownership.map(x=>[x.packagePath,x]));
  for(const name of ['gf01-pairs.json','overclock-identities.json','runtime-base.json',...['gf02','gf03','gf04','remaining'].map(f=>'recipes/'+f+'.json')])collectSelectors(json(path.join(publicRoot,'Authoring',name)),name,selected,ownership);
  for(const selector of dependencies.selectors)collectSelectors(selector.packagePath,selector.consumer,selected,ownership);
  collectSelectors(json(path.join(publicRoot,'Analysis/config/weapon-mirrors.vertical.json')),'weapon mirror source triple',selected,ownership);
  const known=new Map(profile.knownPackages.map(p=>[p.packagePath,p])), preparedContent=path.join(output,'Content');
  const packages=[], parsed=new Map(), expected=new Map(), names=[],historicalMemberChanges=[];
  const mirror=json(path.join(publicRoot,'Analysis/config/weapon-mirrors.vertical.json')).weapons[0];
  const mirrorPins=new Map(['Weapon','Bank','ItemId'].flatMap(kind=>[['.uasset','Uasset'],['.uexp','Uexp']].map(([ext,key])=>[mirror['source'+kind+'Package'].slice(6)+ext,mirror['source'+kind+key+'Sha256']])));
  const validatorSources=['Analysis/src/parse/parse-assets.mjs',...['prepare-native-inputs.mjs','prepared-inputs.mjs','native-semantics.mjs'].map(n=>'Analysis/src/authoring/'+n)].map(name=>({path:name,sha256:fileHash(path.join(publicRoot,name))}));
  let sourceContent, extraction,cached;
  if(reuseReceipt) {
    cached=verifySourceReceipt(reuseReceipt,profileFile);sourceContent=path.join(cached.root,'Content');
    for(const key of ['pak','shippingExe'])requireFact(cached.receipt.source[key].sha256===source[key].sha256,'Cache source changed; run fresh preparation for '+key);
    requireFact(canonical(cached.receipt.validatorSources)===canonical(validatorSources),'Native semantic validator changed; run fresh preparation');
    requireFact(cached.receipt.inspector.binarySha256===fileHash(inspector),'Verified cache inspector changed');
    for(const p of json(path.join(cached.root,'dependency-inventory.json')).packages)expected.set(p.packagePath,p);
  }else {
    // One disposable copy is the only PAK ever passed to the extractor.
    extraction=path.join(output,'extraction');fs.mkdirSync(extraction);const copy=path.join(extraction,'source.pak');
    fs.copyFileSync(pak,copy);requireFact(await streamHash(copy)===source.pak.sha256,'Disposable PAK copy changed');
    await run(unrealPak,[copy,'-Extract',path.join(extraction,'unpacked')]);
    sourceContent=path.join(extraction,'unpacked','FSD','Content');
    requireFact(fs.existsSync(sourceContent),'Extractor did not preserve FSD/Content identity');
  }
  const mappingByIdentity=new Map(walk(sourceContent).filter(f=>f.endsWith('.uasset')).map(f=>[('/Game/'+path.relative(sourceContent,f).replaceAll('\\','/').slice(0,-7)).toLowerCase(),f]));
  const mapping={get:pkg=>mappingByIdentity.get(pkg.toLowerCase())};
  const inspectorSource=['Program.cs','UAssetInspector.csproj'].map(n=>({path:n,sha256:fileHash(path.join(publicRoot,'Analysis/tools/UAssetInspector',n))}));
  while([...selected.keys()].some(p=>!parsed.has(p))) {
    const batch=[...selected.keys()].filter(p=>!parsed.has(p)).slice(0,128);
    for(const pkg of batch) {
      const input=mapping.get(pkg);requireFact(input,'Missing native package '+pkg+' requested by '+[...selected.get(pkg)].join(', '));
      const members=[];
      for(const ext of ['.uasset','.uexp','.ubulk']) {
        const f=input.slice(0,-7)+ext;if(!fs.existsSync(f))continue;
        const relative=pkg.slice('/Game/'.length)+ext, actual={path:relative,bytes:fs.statSync(f).size,sha256:fileHash(f)};
        const historical=known.get(pkg)?.members.find(m=>m.path===relative);
        if(historical && (historical.bytes!==actual.bytes || historical.sha256!==actual.sha256))historicalMemberChanges.push({packagePath:pkg,path:relative,historicalSha256:historical.sha256,actualSha256:actual.sha256,validation:'consumed-semantics'});
        if(expected.has(pkg)){const m=expected.get(pkg).members.find(m=>m.path===relative);requireFact(m && m.bytes===actual.bytes && m.sha256===actual.sha256,'Verified cache member changed '+relative);}
        if(mirrorPins.has(relative))requireFact(actual.sha256===mirrorPins.get(relative),'Pinned mirror serialization changed '+relative+' requested by weapon mirror source triple');
        const target=path.join(preparedContent,relative);fs.mkdirSync(path.dirname(target),{recursive:true});fs.copyFileSync(f,target);members.push(actual);names.push('Content/'+relative);
      }
      if(expected.has(pkg))requireFact(expected.get(pkg).members.length===members.length,'Missing verified-cache companion '+pkg);
      if(reuseReceipt)requireFact(expected.has(pkg),'Verified cache lacks required dependency '+pkg);
      packages.push({packagePath:pkg,consumers:[...selected.get(pkg)].sort(),usage:selected.get(pkg).has('weapon mirror source triple')?'mirror-source':'editor-authoring-and-cook',members});
    }
    if(reuseReceipt){
      for(const pkg of batch){parsed.set(pkg,null);const p=expected.get(pkg);packages.find(x=>x.packagePath===pkg).hardImports=p.hardImports??[];for(const dep of p.hardImports??[])collectSelectors(dep,'hard import of '+pkg,selected,ownership);}
      continue;
    }
    const dir=path.join(output,'inspection',String(parsed.size));fs.mkdirSync(dir,{recursive:true});
    const result=runInspector(dotnet,batch.map(p=>mapping.get(p)),dir,{inspectorPath:inspector,readResult:json});
    const byInput=new Map(batch.map(p=>[path.resolve(mapping.get(p)).toLowerCase(),p]));
    for(const row of result.results) {
      requireFact(row.success,'Inspector failed '+row.input+': '+row.error);const pkg=byInput.get(path.resolve(row.input).toLowerCase()), doc=json(row.output);parsed.set(pkg,doc);
      // Package imports are the hard object/class/default dependency closure.
      const imports=(doc.Imports??[]).filter(imp=>imp.ClassName==='Package' && imp.ObjectName.startsWith('/Game/') && imp.ObjectName!==pkg).map(imp=>imp.ObjectName);
      packages.find(x=>x.packagePath===pkg).hardImports=[...new Set(imports)].sort();
      for(const dep of imports)collectSelectors(dep,'hard import of '+pkg,selected,ownership);
    }
  }
  const facts=reuseReceipt?cached.facts:deriveNativeFacts(parsed,profile,profileSha256,packages);
  writeJson(path.join(output,'native-facts.json'),facts);names.push('native-facts.json');
  writeJson(path.join(output,'dependency-inventory.json'),{schema:'dor.native-dependency-inventory.v1',packages:packages.sort((a,b)=>a.packagePath.localeCompare(b.packagePath)),runtimeOnlyExpectations:dependencies.runtimeOnlyExpectations??[],excludedOwnership:dependencies.ownership,historicalMemberChanges});names.push('dependency-inventory.json');
  // Inspector diagnostics and disposable originals are private work material;
  // the completion receipt binds only canonical facts and selected originals.
  const receipt={schema:'dor.native-source-receipt.v2',profileId:profile.profileId,profileSha256,source,validatorSources,semanticValidation:{consumedTargetExports:true,shippingDefaultsCertified:false,pendingLoadedObjectChecks:facts.pendingLoadedObjectChecks.length},inspector:{binarySha256:fileHash(inspector),source:inspectorSource,engine:'UE4.27'},members:inventory(output,names.sort()),complete:true};
  const receiptFile=path.join(output,'source-receipt.json'),pending=path.join(output,'source-receipt.pending.json');writeJson(pending,receipt);verifySourceReceipt(pending,profileFile);fs.renameSync(pending,receiptFile);
  return {receiptFile,packages:packages.length,members:names.length};
}

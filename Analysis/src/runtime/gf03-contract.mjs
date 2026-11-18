import {loadPublicFamily} from '../authoring/prepared-inputs.mjs';
import { resolveInput } from "../shared/paths.mjs";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { projectDir } from "../shared/paths.mjs";
import { placeNumericOccurrences, validateNativeSelection } from "./native-numeric-policy.mjs";
export const gf03ContractPath=path.join(projectDir,"Analysis/data/manual/gf03-recipes.json");
export const gf03ResolvedInputsPath=path.join(projectDir,"Analysis/data/manual/gf03-resolved-native-inputs.json");
const hash=bytes=>crypto.createHash("sha256").update(bytes).digest("hex");
export function loadGF03Inputs(preparedReceipt = null){
  if(preparedReceipt){const result=loadPublicFamily(preparedReceipt,"gf03",projectDir);validateGF03ResolvedInputs(result.contract,result.evidence);return result;}
 const bytes=fs.readFileSync(resolveInput(gf03ContractPath)),contract=JSON.parse(bytes),evidenceBytes=fs.readFileSync(resolveInput(gf03ResolvedInputsPath)),evidence=JSON.parse(evidenceBytes);
 for(const name of ["classification","implementations","catalog","parsedIndex","gf02","gf04","numericPolicy"])requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir,contract.authority.paths[name]))))===contract.authority.sourceHashes[name],"pinned input "+name);
 requireFact(evidence.contractSha256===hash(bytes),"resolved contract hash");
 for(const ref of [contract.authority.researchContract,contract.authority.orderRebase])requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir,ref.path))))===ref.sha256,"order provenance "+ref.path);
 for(const input of [...evidence.inputs,...evidence.roots,...evidence.carrierSources])for(const ref of [input.exportEvidence,input.packageEvidence,input.payloadEvidence])requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir,ref.path))))===ref.sha256,"evidence "+ref.path);
 requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir,evidence.carrierGateEvidence.path))))===evidence.carrierGateEvidence.sha256,"carrier gate provenance");
 validateGF03ResolvedInputs(contract,evidence);
 return {contract,evidence,contractSha256:hash(bytes),evidenceSha256:hash(evidenceBytes)};
}
// Research reference overlay. UObject parsing and asset readback stay in the existing generator.
const equal=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
const requireFact=(v,m)=>{if(!v)throw Error(m);};
const bag=xs=>JSON.stringify([...xs].sort());
const key=o=>`${o.side}|${o.sourceObjectPath}|${o.pathOrdinal}`;
function numbered(xs){const counts=new Map();return xs.map(o=>{const k=o.side+'|'+o.sourceObjectPath,n=counts.get(k)??0;counts.set(k,n+1);return {...o,pathOrdinal:n};});}

export function validateGF03ResolvedInputs(contract,evidence){
  requireFact(evidence.schema==='dor.gf03.resolved-native-inputs.v1','current input schema');
  const inputs=new Map((evidence.inputs??[]).map(i=>[i.sourceObjectPath,i]));
  requireFact(inputs.size===(evidence.inputs??[]).length,'duplicate current input');
  for(const r of contract.recipes)for(const op of r.operations)for(const s of op.sourceOccurrences){
    const input=inputs.get(s.sourceObjectPath);
    requireFact(input?.classPath===s.sourceClassPath,'current source class '+s.sourceObjectPath);
    for(const f of Object.values(s))if(f&&typeof f==='object'&&f.field)requireFact(equal(input.fields?.[f.field],f.value),'current effective field '+s.sourceObjectPath+' '+f.field);
  }
  const facts=new Map((evidence.carriers??[]).map(c=>[c.testIndex,c]));
  for(const r of contract.recipes)if(r.requiredCarrierFacts){
    const c=facts.get(r.testIndex),expected=r.requiredCarrierFacts;
    requireFact(c&&c.finalChargedClass===expected.finalChargedClass,'actual charged carrier '+r.testIndex);
    for(const f of expected.classRelations)requireFact(c.classRelations?.some(x=>equal(x,f)),'current IsA relation '+r.testIndex);
    for(const f of expected.declaredSwitchFields??[])requireFact(c.declaredSwitchFields?.some(x=>equal(x,f)),'current declared switch '+r.testIndex);
  }
}

export function emitterLeaf(leaf){
  requireFact(['new-native-standard','duplicate-native-template'].includes(leaf.assetCreation),'native emitter primitive');
  const adjusted=leaf.operation==='emit-native-flat-adjustment';
  return {...leaf,sourceObjectPath:leaf.templateObjectPath,sourceObjectPaths:leaf.sourceOccurrences.map(s=>s.sourceObjectPath),sourceAmounts:leaf.sourceOccurrences.map(s=>s.amount.value),sourceAmount:leaf.sourceOccurrences[0].amount.value,upgradeType:leaf.identity.target,
    ...(adjusted?{adjustedAmount:leaf.serializedResult,contributionCount:1,adjustmentMode:leaf.selection.mode}:{mergedAmount:leaf.serializedResult,contributionCount:leaf.sourceOccurrences.length,mergeMode:leaf.selection.mode}),utilityDirection:leaf.selection.utility,selectionRule:leaf.selection.rule};
}

export function overlayGF03Recipes(pairs,contract,evidence=null){
  requireFact(contract.schema==='dor.gf03.batch-recipes.v1'&&contract.recipes.length===79,'GF03 schema/count');
  if(evidence)validateGF03ResolvedInputs(contract,evidence);
  const byId=new Map(contract.recipes.map(r=>[r.pairId,r]));requireFact(byId.size===79,'duplicate recipe');
  const roots=evidence?new Map((evidence.roots??[]).map(r=>[r.sourceObjectPath,r.orderedLeafObjectPaths])):null;
  let matched=0;
  const result=pairs.map(row=>{
    const recipe=byId.get(row.pairId);if(!recipe)return row;++matched;
    for(const k of ['testIndex','classKey','slot','weaponFolder','weaponIdObjectPath','pairUpgradeObjectPath','overclockAId','overclockBId'])requireFact(row[k]===recipe[k],'stable join '+k);
    const current=roots?['A','B'].flatMap(side=>{const root=recipe['overclock'+side+'ObjectPath'],paths=roots.get(root);requireFact(paths,'missing current original root');const old=recipe.orderedLeafOccurrences.filter(o=>o.side===side);requireFact(bag(paths)===bag(old.map(o=>o.sourceObjectPath)),'changed original dependency membership '+root);return paths.map((sourceObjectPath,sideIndex)=>({side,sideIndex,sourceObjectPath}));}):recipe.orderedLeafOccurrences;
    const numberedCurrent=numbered(current).map((o,index)=>({...o,index})),currentByKey=new Map(numberedCurrent.map(o=>[key(o),o.index])),oldNumbered=numbered(recipe.orderedLeafOccurrences);
    requireFact(equal(row.orderedLeafObjectPaths,recipe.catalogOrderedLeafObjectPaths),'catalog occurrence join');
    requireFact(equal(recipe.orderedLeafObjectPaths,current.map(o=>o.sourceObjectPath)),'current pair original order');
    const remap=i=>{const j=currentByKey.get(key(oldNumbered[i]));requireFact(j!==undefined,'missing source occurrence');return j;};
    const operations=recipe.operations.map(op=>{
      validateNativeSelection(op);
      requireFact(op.executionGate.productionEmissionAllowed,'unresolved operation');
      const indices=op.consumedSourceIndices.map(remap).sort((a,b)=>a-b);
      return {...op,consumedSourceIndices:indices};
    });
    const order=current.map(o=>o.sourceObjectPath),final=placeNumericOccurrences(order,operations);
    if(!evidence)requireFact(equal(final,recipe.finalLeafObjectPaths),'reference final order');
    const {rowHash,...base}=row;
    return {...base,orderedLeafObjectPaths:current.map(o=>o.sourceObjectPath),finalLeafObjectPaths:final,mergedSharedLeaves:recipe.generatedNumericLeaves.filter(l=>l.operation==='emit-native-numeric-leaf').map(emitterLeaf),adjustedLeaves:recipe.generatedNumericLeaves.filter(l=>l.operation==='emit-native-flat-adjustment').map(emitterLeaf),numericStackDecisions:recipe.operations.map(op=>({target:op.identity?.target??op.consumer.id,sourceObjectPaths:op.sourceOccurrences.map(s=>s.sourceObjectPath),mode:op.selection.mode,utility:op.selection.utility??op.consumer.utility,runtimeMerge:op.operation==='same-target-native-recomposition',runtimeAdjustment:op.operation.startsWith('canonical-native')||op.operation.startsWith('native-filtered'),formula:op.selection.formula??null})),gf03:true};
  });
  requireFact(matched===79,'missing selected runtime join');return result;
}

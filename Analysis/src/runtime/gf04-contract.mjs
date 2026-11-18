import {effectiveNumericResult} from './effective-numeric-output.mjs';
import {loadPublicFamily} from '../authoring/prepared-inputs.mjs';
import { resolveInput } from "../shared/paths.mjs";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { projectDir } from "../shared/paths.mjs";
import { validateNativeSelection } from "./native-numeric-policy.mjs";
export const gf04ContractPath = path.join(projectDir, "Analysis/data/manual/gf04-recipes.json");
export const gf04ResolvedInputsPath = path.join(projectDir, "Analysis/data/manual/gf04-resolved-native-inputs.json");
const hash = bytes => crypto.createHash("sha256").update(bytes).digest("hex");
export function loadGF04Inputs(preparedReceipt = null) {
  if(preparedReceipt){const result=loadPublicFamily(preparedReceipt,"gf04",projectDir);validateGF04ResolvedInputs(result.contract,result.evidence);return result;}
  const bytes = fs.readFileSync(resolveInput(gf04ContractPath)), contract = JSON.parse(bytes);
  for (const name of ["classification", "implementations", "catalog", "parsedIndex", "gf02", "gf02Resolved", "numericPolicy"])
    requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir, contract.authority.paths[name])))) === contract.authority.sourceHashes[name], "pinned input " + name);
  const evidenceBytes = fs.readFileSync(resolveInput(gf04ResolvedInputsPath)), evidence = JSON.parse(evidenceBytes);
  requireFact(evidence.contractSha256 === hash(bytes), "resolved contract hash");
  for (const ref of [contract.authority.researchContract, contract.authority.orderRebase])
    requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir, ref.path)))) === ref.sha256, "order provenance " + ref.path);
  for (const input of [...evidence.inputs, ...evidence.roots])
    for (const ref of [input.exportEvidence, input.packageEvidence])
      requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir, ref.path)))) === ref.sha256, "evidence " + ref.path);
  validateGF04ResolvedInputs(contract, evidence);
  return {contract, evidence, contractSha256:hash(bytes), evidenceSha256:hash(evidenceBytes)};
}
// Pure overlay and emitter mapping; current UObject input validation stays in the proven generator.
const equal=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
const requireFact=(x,m)=>{if(!x)throw Error(m);};

export function validateGF04ResolvedInputs(contract,evidence){
  requireFact(evidence.schema==='dor.gf04.resolved-native-inputs.v1','current input schema');
  const inputs=new Map();
  for(const input of evidence.inputs??[]){requireFact(!inputs.has(input.sourceObjectPath),'duplicate current source');inputs.set(input.sourceObjectPath,input);}
  for(const row of contract.recipes)for(const op of row.operations)for(const source of op.sourceOccurrences){
    const input=inputs.get(source.sourceObjectPath);
    requireFact(input?.classPath===op.sourceClassPath,'current source class');
    for(const fact of Object.values(source))if(fact&&typeof fact==='object'&&fact.field)requireFact(equal(input.fields?.[fact.field],fact.value),'current field '+fact.field);
  }
}

export function emitterLeaf(op, adjusted=false){
  requireFact(['new-native-standard','duplicate-native-template'].includes(op.assetCreation),'native emitter primitive required');
  return {...op,sourceObjectPath:op.templateObjectPath,
    sourceObjectPaths:op.sourceOccurrences.map(s=>s.sourceObjectPath),
    sourceAmounts:op.sourceOccurrences.map(s=>s.amount.value),
    sourceAmount:op.sourceOccurrences.find(s=>s.sourceObjectPath===op.templateObjectPath).amount.value,
    upgradeType:op.identity.target,
    ...(adjusted?{adjustedAmount:op.serializedResult,contributionCount:1,adjustmentMode:op.selection.mode}
      :{mergedAmount:op.serializedResult,contributionCount:op.sourceOccurrences.length,mergeMode:op.selection.mode}),
    utilityDirection:op.selection.utility,selectionRule:op.selection.rule};
}

export function overlayGF04Recipes(pairs,contract,evidence=null){
  requireFact(contract.schema==='dor.gf04.batch-recipes.v1'&&contract.recipes.length===63,'GF04 schema/count');
  if(evidence)validateGF04ResolvedInputs(contract,evidence);
  const byId=new Map(contract.recipes.map(r=>[r.pairId,r]));let matched=0;
  requireFact(byId.size===63,'duplicate GF04 identity');
  const result=pairs.map(row=>{
    const recipe=byId.get(row.pairId);if(!recipe)return row;++matched;
    for(const key of ['testIndex','classKey','slot','weaponFolder','weaponIdObjectPath','pairUpgradeObjectPath','overclockAId','overclockBId'])requireFact(row[key]===recipe[key],'stable join '+key);
    requireFact(equal(row.orderedLeafObjectPaths,recipe.catalogOrderedLeafObjectPaths),'catalog occurrence join');
    const ordered=recipe.orderedLeafObjectPaths;
    const consumed=new Map(),merged=[],adjusted=[];
    for(const op of recipe.operations){
      validateNativeSelection(op);
      requireFact(op.executionGate.productionEmissionAllowed,'unresolved operation');
      for(const s of op.sourceOccurrences)requireFact(ordered[s.index]===s.sourceObjectPath,'source dependency index');
      if(op.operation==='ordered-native-integer-contributions'){
        requireFact(equal(op.outputLeafObjectPaths,op.sourceOccurrences.map(s=>s.sourceObjectPath)),'integer native order');
        continue;
      }
      if(op.operation==='preserve-native-affine-order'){
        requireFact(row.testIndex===390&&op.assetCreation==='none'&&op.consumedSourceIndices.length===0,'preservation mode');
        requireFact(equal(op.retainedSourceIndices.map(i=>ordered[i]),op.outputLeafObjectPaths),'preserved affine order');
        requireFact(op.retainedSourceIndices[0]<op.retainedSourceIndices[1]&&!op.sourceOccurrences[0].percentage.value&&op.sourceOccurrences[1].percentage.value,'390 ADD then MUL');
        continue;
      }
      const isAdjusted=op.operation==='emit-native-flat-adjustment';
      requireFact(isAdjusted||op.operation==='emit-native-numeric-leaf','unsupported operation');
      const indices=op.consumedSourceIndices??op.sourceOccurrences.map(s=>s.index);
      const path=isAdjusted?op.adjustedObjectPath:op.mergedObjectPath;
      requireFact(Number.isFinite(op.serializedResult)&&effectiveNumericResult(op)===op.serializedResult,'float32 value');
      for(const index of indices){requireFact(!consumed.has(index),'double consumption');consumed.set(index,{first:indices[0],path});}
      if(isAdjusted){
        requireFact(row.testIndex===393&&indices.length===1&&op.retainedSourceIndices.length===1&&!op.isPercentage&&indices[0]<op.retainedSourceIndices[0],'single-flat adjustment boundary');
        const d=op.sourceOccurrences.find(s=>s.index===indices[0]),m=op.sourceOccurrences.find(s=>s.index===op.retainedSourceIndices[0]);
        requireFact(!d.percentage.value&&m.percentage.value&&op.selection.value===d.amount.value/m.amount.value,'affine inputs/formula');
        adjusted.push(emitterLeaf(op,true));
      }else merged.push(emitterLeaf(op));
    }
    const final=ordered.flatMap((p,i)=>{const r=consumed.get(i);return r?(r.first===i?[r.path]:[]):[p];});
    requireFact(equal(final,recipe.finalLeafObjectPaths),'final indexed order');
    const {rowHash,...base}=row;
    return {...base,orderedLeafObjectPaths:ordered,finalLeafObjectPaths:final,mergedSharedLeaves:merged,adjustedLeaves:adjusted,
      numericStackDecisions:recipe.operations.map(op=>({target:op.identity.target,sourceObjectPaths:op.sourceOccurrences.map(s=>s.sourceObjectPath),mode:op.selection.mode,utility:op.selection.utility,runtimeMerge:op.operation==='emit-native-numeric-leaf',runtimeAdjustment:op.operation==='emit-native-flat-adjustment',formula:op.selection.formula??null})),gf04:true};
  });
  requireFact(matched===63,'missing selected runtime join');return result;
}

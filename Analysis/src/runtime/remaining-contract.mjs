import {loadPublicFamily} from '../authoring/prepared-inputs.mjs';
import { resolveInput, resolveEvidence } from "../shared/paths.mjs";
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {projectDir} from '../shared/paths.mjs';
import {emitterLeaf} from './gf04-contract.mjs';
import {placeNumericOccurrences, validateNativeSelection} from './native-numeric-policy.mjs';

export const remainingContractPath=path.join(projectDir,'Analysis/data/manual/remaining-recipes.json');
export const remainingResolvedInputsPath=path.join(projectDir,'Analysis/data/manual/remaining-resolved-native-inputs.json');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
export function loadRemainingInputs(preparedReceipt=null){
  if(preparedReceipt){const result=loadPublicFamily(preparedReceipt,"remaining",projectDir);assert.equal(result.contract.schema,"dor.remaining.batch-recipes.v1");assert.equal(result.evidence.schema,"dor.remaining.resolved-native-inputs.v1");return result;}
  const bytes=fs.readFileSync(resolveInput(remainingContractPath)),contract=JSON.parse(bytes);
  const evidenceBytes=fs.readFileSync(resolveInput(remainingResolvedInputsPath)),evidence=JSON.parse(evidenceBytes);
  assert.equal(contract.schema,'dor.remaining.batch-recipes.v1');
  assert.equal(evidence.schema,'dor.remaining.resolved-native-inputs.v1');
  assert.equal(evidence.contractSha256,hash(bytes));
  assert.ok(contract.releaseScope && contract.releaseScope.label==='本批新增');
  assert.equal(contract.releaseScope.pairIds.length,contract.releaseScope.codes.length);
  const releaseRows=new Map(contract.recipes.map(r=>[r.pairId,r]));
  contract.releaseScope.pairIds.forEach((pairId,index)=>assert.equal(releaseRows.get(pairId)?.testIndex,contract.releaseScope.codes[index]));
  for(const ref of [...contract.authority.sources,...evidence.inputs.flatMap(i=>[i.exportEvidence,i.packageEvidence,i.payloadEvidence]),...evidence.roots.flatMap(i=>[i.exportEvidence,i.packageEvidence,i.payloadEvidence])])
    assert.equal(hash(fs.readFileSync(resolveEvidence(path.resolve(projectDir,ref.path)))),ref.sha256,ref.path);
  return {contract,evidence,contractSha256:hash(bytes),evidenceSha256:hash(evidenceBytes)};
}

export function overlayRemainingRecipes(pairs,contract,evidence){
  const byId=new Map(contract.recipes.map(r=>[r.pairId,r]));
  assert.equal(byId.size,contract.recipes.length);
  assert.ok(contract.recipes.length > 0);
  assert.ok(!contract.recipes.some(row=>row.testIndex===300));
  const inputs=new Map(evidence.inputs.map(i=>[i.sourceObjectPath,i]));
  const roots=new Map(evidence.roots.map(i=>[i.sourceObjectPath,i]));
  let matched=0;
  const result=pairs.map(row=>{
    const recipe=byId.get(row.pairId);if(!recipe)return row;++matched;
    for(const key of ['testIndex','classKey','slot','weaponFolder','weaponIdObjectPath','pairUpgradeObjectPath','overclockAId','overclockBId'])assert.equal(row[key],recipe[key],key);
    assert.deepEqual(row.orderedLeafObjectPaths,recipe.catalogOrderedLeafObjectPaths);
    assert.deepEqual(['A','B'].flatMap(s=>roots.get(recipe['overclock'+s+'ObjectPath']).orderedLeaves),recipe.orderedLeafObjectPaths);
    for(const op of recipe.operations){
      validateNativeSelection(op);
      assert.ok(op.executionGate.productionEmissionAllowed);
      for(const source of op.sourceOccurrences){
        const input=inputs.get(source.sourceObjectPath);assert.equal(input.classPath,op.sourceClassPath);
        assert.equal(recipe.orderedLeafObjectPaths[source.index],source.sourceObjectPath);
        for(const fact of Object.values(source))if(fact&&typeof fact==='object'&&fact.field)assert.deepEqual(input.fields[fact.field],fact.value);
      }
    }
    for (const edit of recipe.sourceEdits ?? []) {
      const input=inputs.get(edit.sourceObjectPath);
      assert.equal(input?.classPath,edit.sourceClassPath);
      for (const fact of Object.values(edit.sourceOccurrences[0])) if(fact&&typeof fact==='object'&&fact.field) assert.deepEqual(input.fields[fact.field],fact.value);
      if (edit.outputObjectPath) assert.ok((recipe.templateLeafClones ?? []).some(c=>c.objectPath===edit.outputObjectPath && c.sourceObjectPath===edit.sourceObjectPath));
    }
    const final=placeNumericOccurrences(recipe.orderedLeafObjectPaths,recipe.operations,recipe.sourceEdits);
    if(recipe.bulldogRicochet){
      assert.equal(recipe.testIndex,406);assert.equal(recipe.pairId,'0b1ee9ac4b262a5d');
      final.push(recipe.bulldogRicochet.leafObjectPath);
    }
    if(recipe.specialPowderDouble){
      assert.equal(recipe.testIndex,421);assert.equal(recipe.pairId,'43043a5f02b4b525');
      final.push(recipe.specialPowderDouble.leafObjectPath);
    }
    assert.deepEqual(final,recipe.finalLeafObjectPaths);
    const {rowHash,...base}=row;
    return {...base,orderedLeafObjectPaths:recipe.orderedLeafObjectPaths,finalLeafObjectPaths:final,
      mergedSharedLeaves:recipe.operations.filter(op=>op.operation==='emit-native-numeric-leaf').map(op=>emitterLeaf(op,false)),
      adjustedLeaves:recipe.operations.filter(op=>op.operation==='emit-native-flat-adjustment').map(op=>emitterLeaf(op,true)),
      numericStackDecisions:recipe.operations.map(op=>({target:op.identity.target,sourceObjectPaths:op.sourceOccurrences.map(s=>s.sourceObjectPath),mode:op.selection.mode,utility:op.selection.utility,runtimeMerge:op.operation==='emit-native-numeric-leaf',runtimeAdjustment:op.operation==='emit-native-flat-adjustment',formula:op.selection.formula??null})),
      remainingPrepared:true,formalFamilyId:recipe.formalFamilyId,executionGroupId:recipe.executionGroupId,
      ...(recipe.templateLeafClones?.length ? {templateLeafClones:recipe.templateLeafClones} : {}),
      ...(recipe.hurricaneChargedCarrier ? {hurricaneChargedCarrier:recipe.hurricaneChargedCarrier} : {}),
      ...(recipe.bulldogRicochet ? {bulldogRicochet:recipe.bulldogRicochet} : {}),
      ...(recipe.cryoSnowballCarrier ? {cryoSnowballCarrier:recipe.cryoSnowballCarrier} : {}),
      ...(recipe.fullLockAuto ? {fullLockAuto:recipe.fullLockAuto} : {}),
      ...(recipe.specialPowderDouble ? {specialPowderDouble:recipe.specialPowderDouble} : {}),
      ...(recipe.rootElementReferences?.length ? {rootElementReferences:recipe.rootElementReferences} : {}),
      implementationSummaryZh:recipe.implementationSummaryZh,protectionBoundaryZh:recipe.protectionBoundaryZh};
  });
  assert.equal(matched,byId.size);return result;
}

export function appendRemainingCatalogRows(pairs,contract,overclocks,assessments){
  const result=[...pairs], byOc=new Map(overclocks.map(row=>[row.overclockId,row]));
  for(const recipe of contract.recipes.filter(row=>row.appendRuntimeRow).sort((a,b)=>a.testIndex-b.testIndex)){
    const declaration=recipe.appendRuntimeRow;
    assert.ok([452,455].includes(declaration.baselineMaxTestIndex));
    assert.equal(recipe.testIndex,result.length+1);
    assert.ok(!result.some(row=>row.pairId===recipe.pairId));
    const assessment=assessments.assessments.find(row=>row.pairId===recipe.pairId);
    const a=byOc.get(recipe.overclockAId),b=byOc.get(recipe.overclockBId);
    assert.equal(assessment.ocA.overclockId,a.overclockId);assert.equal(assessment.ocB.overclockId,b.overclockId);
    result.push({pairId:recipe.pairId,testIndex:recipe.testIndex,classKey:a.classKey,classNameZh:assessment.className,
      slot:recipe.slot,weaponFolder:recipe.weaponFolder,weaponNameZh:assessment.weaponName,weaponObjectPath:a.weaponObjectPath,weaponIdObjectPath:recipe.weaponIdObjectPath,
      pairUpgradeObjectPath:recipe.pairUpgradeObjectPath,weaponMirrorClassPath:'',weaponMirrorSavegameId:'',weaponMirrorItemIdPath:'',nativeItemIdPath:'',fusionRootPath:recipe.pairUpgradeObjectPath,pairSpecificWeaponClassPath:'',
      overclockAId:a.overclockId,overclockANameZh:a.nameZh,overclockANameEn:a.nameEn,overclockAEffectZh:a.effectZh,overclockAObjectPath:a.objectPath,
      overclockBId:b.overclockId,overclockBNameZh:b.nameZh,overclockBNameEn:b.nameEn,overclockBEffectZh:b.effectZh,overclockBObjectPath:b.objectPath,
      orderedLeafObjectPaths:recipe.catalogOrderedLeafObjectPaths,finalLeafObjectPaths:recipe.catalogOrderedLeafObjectPaths,
      duplicateLeafObjectPaths:[],mergedSharedLeaves:[],adjustedLeaves:[],numericStackDecisions:[],sharedWriteTargets:assessment.collision.sharedWrites??[],
      route:assessment.route,feasibility:assessment.feasibility,implementationComplexity:assessment.scores.implementationComplexity,bridgeHandlerId:'',
      implementationNameZh:assessment.recommendation.nameZh,implementationSummaryZh:assessment.recommendation.funChangeZh,protectionBoundaryZh:assessment.recommendation.protectionBoundaryZh});
  }
  return result;
}

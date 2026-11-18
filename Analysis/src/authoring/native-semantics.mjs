import {canonical,requireFact} from './prepared-inputs.mjs';

export function objectReference(document,packagePath,index,seen=new Set()) {
  if(!index)return null;
  requireFact(Number.isInteger(index) && !seen.has(index),'Invalid/cyclic object index '+packagePath+':'+index);
  seen.add(index);
  const item=index<0?document.Imports[-index-1]:document.Exports[index-1];
  requireFact(item,'Missing object index '+packagePath+':'+index);
  const name=item.ObjectName;
  if(name.startsWith('/'))return name;
  const outer=item.OuterIndex?objectReference(document,packagePath,item.OuterIndex,seen):null;
  return outer?outer+(outer.includes('.')?':':'.')+name:packagePath+'.'+name;
}
export function targetExport(parsed,objectPath) {
  const pkg=objectPath.split('.')[0],document=parsed.get(pkg)??[...parsed].find(([key])=>key.toLowerCase()===pkg.toLowerCase())?.[1];
  requireFact(document,'Missing parsed package for '+objectPath);
  // UObject/FName lookup is case-insensitive; keep the maintained public path.
  const matches=document.Exports.map((value,index)=>({value,index:index+1})).filter(x=>objectReference(document,pkg,x.index).toLowerCase()===objectPath.toLowerCase());
  requireFact(matches.length===1,'Missing/ambiguous target export '+objectPath);
  return {packagePath:pkg,document,export:matches[0].value,objectPath};
}
function propertyValue(property,context) {
  const value=property.Value,type=property.$type??'';
  if(type.includes('SoftObjectPropertyData') || type.includes('SoftClassPropertyData'))return value?.AssetPath?.AssetName && value.AssetPath.AssetName!=='None'?value.AssetPath.AssetName:null;
  if(type.includes('ObjectPropertyData') || type.includes('ClassPropertyData'))return objectReference(context.document,context.packagePath,value);
  if(type.includes('StructPropertyData'))return Object.fromEntries((value??[]).map(p=>[p.Name,propertyValue(p,context)]));
  if(type.includes('ArrayPropertyData'))return (value??[]).map(p=>propertyValue(p,context));
  return value==='None'?null:value;
}
export function consumedField(parsed,context,field,visited=new Set()) {
  requireFact(!visited.has(context.objectPath),'Cyclic archetype '+context.objectPath);visited.add(context.objectPath);
  const parts=field.split('.');let properties=context.export.Data??[],property;
  for(let i=0;i<parts.length;i++) {
    const matches=properties.filter(p=>p.Name===parts[i]);requireFact(matches.length<=1,'Duplicate target-export field '+context.objectPath+'.'+field);
    property=matches[0];if(!property)break;
    if(i<parts.length-1)properties=Array.isArray(property.Value)?property.Value:[];
  }
  if(property)return {state:'observed-serialized',value:propertyValue(property,context),sourceObjectPath:context.objectPath,field};
  const archetype=objectReference(context.document,context.packagePath,context.export.TemplateIndex);
  if(archetype?.startsWith('/Game/'))return consumedField(parsed,targetExport(parsed,archetype),field,visited);
  return {state:'established-recipe-expectation',archetypePath:archetype,sourceObjectPath:context.objectPath,field,validationStage:'loaded-UObject',shippingDefaultProved:false};
}
function expectField(parsed,context,field,expected,basis,pending) {
  const observation=consumedField(parsed,context,field);
  if(observation.state==='observed-serialized')requireFact(canonical(observation.value)===canonical(expected),`Consumed source mismatch ${context.objectPath}.${field}: expected ${canonical(expected)}, observed ${canonical(observation.value)} at ${observation.sourceObjectPath}`);
  else pending.push({sourceObjectPath:context.objectPath,classPath:objectReference(context.document,context.packagePath,context.export.ClassIndex),field,expectedValue:expected,basis,archetypePath:observation.archetypePath,validationStage:'loaded-UObject',shippingDefaultProved:false});
  return {...observation,expectedValue:expected,basis};
}
export function validateOriginalRoot(document,root) {
  const parsed=new Map([[root.sourceObjectPath.split('.')[0],document]]),context=targetExport(parsed,root.sourceObjectPath);
  const classPath=objectReference(document,context.packagePath,context.export.ClassIndex);
  requireFact(classPath==='/Script/FSD.OverclockUpgrade','Original root class changed '+root.sourceObjectPath+': '+classPath);
  const props=new Map((context.export.Data??[]).map(p=>[p.Name,p]));
  const leaves=(props.get('CombinedUpgrades')?.Value??[]).map(p=>p.Value.AssetPath.AssetName);
  requireFact(canonical(leaves)===canonical(root.orderedLeaves),'Original root leaf order changed '+root.sourceObjectPath);
  const guid=props.get('SaveGameID')?.Value?.[0]?.Value;
  requireFact(typeof guid==='string' && guid.replace(/[^a-f0-9]/gi,'').toLowerCase()===root.nativeGuid.replace(/[^a-f0-9]/gi,'').toLowerCase(),'Original root GUID changed '+root.sourceObjectPath);
  const category=consumedField(parsed,context,'SchematicCategory').value;
  const expected='/Game/GameElements/Schematics/Categories/SCAT_OC_'+root.category[0].toUpperCase()+root.category.slice(1);
  requireFact(category?.split('.')[0]===expected,'Original carrier category changed '+root.sourceObjectPath+': '+category);
  return {...root,observedClassPath:classPath,archetypePath:objectReference(document,context.packagePath,context.export.TemplateIndex),validationStage:'parsed-original-target-export'};
}
export function deriveNativeFacts(parsed,profile,profileSha256,packages) {
  const pending=[],observed=[];
  const inputs=profile.inputs.map(expected=>{
    const context=targetExport(parsed,expected.sourceObjectPath),classPath=objectReference(context.document,context.packagePath,context.export.ClassIndex);
    requireFact(classPath===expected.classPath,'Original source class changed '+expected.sourceObjectPath+': expected '+expected.classPath+', observed '+classPath);
    const fieldObservations={},fields={};
    for(const [field,value] of Object.entries(expected.fields)) {
      fieldObservations[field]=expectField(parsed,context,field,value,{profileFactId:expected.factId,recordedFieldState:expected.fieldStates[field]},pending);
      fields[field]=fieldObservations[field].state==='observed-serialized'?fieldObservations[field].value:value;
    }
    const archetypePath=objectReference(context.document,context.packagePath,context.export.TemplateIndex);
    if(expected.componentOwner) {
      fieldObservations.ComponentClass=expectField(parsed,context,'ComponentClass',expected.componentOwner.componentClassPath,{profileFactId:expected.factId,role:'component-owner'},pending);
      if(Object.hasOwn(expected.componentOwner,'serverOnly'))fieldObservations.ServerOnly=expectField(parsed,context,'ServerOnly',expected.componentOwner.serverOnly,{profileFactId:expected.factId,role:'component-server-only'},pending);
    }
    const amount=fieldObservations.Amount;
    return {...expected,classPath,fields,fieldStates:Object.fromEntries(Object.entries(fieldObservations).map(([k,v])=>[k,v.state])),fieldObservations,archetypePath,
      ...(amount?{amountState:amount.state==='observed-serialized'?(amount.value===0?'explicit-numeric-zero':'serialized-numeric'):'omitted-inherit-native-CDO',archetype:archetypePath?.startsWith('/Script/')?'native-class-CDO':'serialized-export'}:{}),
      sourceIdentity:{packagePath:context.packagePath,members:packages.find(p=>p.packagePath.toLowerCase()===context.packagePath.toLowerCase()).members}};
  });
  const roots=profile.roots.map(root=>validateOriginalRoot(targetExport(parsed,root.sourceObjectPath).document,root));
  const carrierSources=profile.carrierSources.map(expected=>{
    const context=targetExport(parsed,expected.sourceObjectPath),superClassPath=objectReference(context.document,context.packagePath,context.export.SuperIndex);
    requireFact(superClassPath===expected.superClassPath,'Carrier superclass changed '+expected.sourceObjectPath+': '+superClassPath);
    return {...expected,superClassPath,validationStage:'parsed-original-target-export'};
  });
  const parents=new Map(carrierSources.map(x=>[x.sourceObjectPath,x.superClassPath]));
  for(const carrier of profile.carriers) {
    for(const field of carrier.declaredSwitchFields)observed.push(expectField(parsed,targetExport(parsed,field.sourceObjectPath),field.field,field.value,{testIndex:carrier.testIndex,role:'carrier-switch'},pending));
    for(const relation of carrier.classRelations) {
      let current=relation.classPath;const visited=new Set();while(current && current!==relation.baseClassPath && parents.has(current) && !visited.has(current)){visited.add(current);current=parents.get(current);}
      requireFact((current===relation.baseClassPath)===relation.isA,'Carrier class relation changed '+relation.classPath+' -> '+relation.baseClassPath);
    }
  }
  return {schema:'dor.prepared-native-facts.v2',profileSha256,inputs,roots,carrierSources,carriers:profile.carriers,carrierFieldObservations:observed,pendingLoadedObjectChecks:pending,shippingDefaultsCertified:false};
}

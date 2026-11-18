import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';

const here=path.dirname(fileURLToPath(import.meta.url));
const [kind,scratch,build,evidenceDirectory]=process.argv.slice(2);
const read=p=>JSON.parse(fs.readFileSync(p,'utf8').replace(/^\uFEFF/,''));
const expectations=read(path.resolve(here,"../Analysis/config/build-readback-expectations.json"));
const offsets=read(path.join(scratch,'offsets.json'));
const batch=read(path.join(scratch,'batch-result.json'));
const assets=new Map();
for(const r of batch.results) {
  assert.ok(r.success,r.input);
  const a=read(r.output);
  a.offsets=offsets.find(x=>x.input===r.input);
  assert.ok(a.offsets,r.input);
  assets.set(path.basename(r.input,'.uasset'),a);
}
const safety=assets.get('BP_DOR_SaveSafety');
const master=assets.get('BP_DOR_Master');
const guard=assets.get('BP_DOR_PresetRecoveryGuard');
const proxy=assets.get('BP_DOR_PlayerProxy');
const sink=assets.get('BP_DOR_NormalDiagnostics');
const type=x=>x?.$type?.split('.').pop().split(',')[0];
const prop=x=>x?.Variable?.New?.Path?.join('.');
const assignment=x=>x.AssignmentExpression??x.Expression;
const destination=x=>prop(x.VariableExpression??x.Variable);
const object=(a,i)=>i>0?a.Exports[i-1]:a.Imports[-i-1];
const call=(a,x)=>x.VirtualFunctionName??(x.StackNode?object(a,x.StackNode)?.ObjectName:undefined);
function walk(x,callback) {
  if(!x||typeof x!=='object')return;
  callback(x);
  for(const v of Object.values(x))walk(v,callback);
}
function functions(a,name) {
  const e=a.Exports.find(x=>x.ObjectName===name&&x.ScriptBytecode?.length);
  const sizes=a.offsets.functions.find(x=>x.name===name);
  assert.ok(e&&sizes,name);
  assert.equal(sizes.computedSize,e.ScriptBytecodeSize,name);
  assert.equal(e.ScriptBytecode.length,sizes.statements.length,name);
  const rows=e.ScriptBytecode.map((op,i)=>{
    const calls=[];
    walk(op,x=>{if(call(a,x))calls.push({name:call(a,x),expression:x});});
    return {op,offset:sizes.statements[i].offset,calls};
  });
  return {a,name,e,rows,offsets:new Map(rows.map((r,i)=>[r.offset,i]))};
}
const calls=f=>f.rows.flatMap(r=>r.calls.map(c=>c.name));
function has(f,...names){return names.every(n=>calls(f).includes(n));}
function at(f,name){return f.rows.findIndex(r=>r.calls.some(c=>c.name===name));}
function checkOrder(f,...names){
  let prev=-1;
  for(const n of names){
    const i=at(f,n);
    assert.ok(i>prev,f.name+' expected serialized order '+names.join(' → '));
    prev=i;
  }
}
function guardFor(f,name) {
  const row=f.rows[at(f,name)];
  assert.ok(row,name);
  const variable=destination(row.op);
  const i=f.rows.findIndex((r,j)=>j>at(f,name)&&type(r.op)==='EX_JumpIfNot'&&prop(r.op.BooleanExpression)===variable);
  assert.ok(variable&&i>=0,f.name+' Boolean result guard '+name);
  return i;
}
function follow(f,start,visit) {
  const pending=[{i:start,stack:[]}],seen=new Set();
  while(pending.length){
    const {i,stack}=pending.pop(),key=i+':'+stack.join(',');
    if(seen.has(key))continue;
    seen.add(key);
    const r=f.rows[i];
    assert.ok(r,f.name+' instruction '+i);
    if(visit(r,i)===false)continue;
    const op=r.op,t=type(op);
    const target=offset=>{assert.ok(f.offsets.has(offset),f.name+' byte offset '+offset);return f.offsets.get(offset);};
    const push=(j,s=stack)=>pending.push({i:j,stack:s});
    const pop=()=>{if(stack.length)push(stack.at(-1),stack.slice(0,-1));};
    const truth=type(op.BooleanExpression);
    switch(t){
      case 'EX_Return': case 'EX_EndOfScript': break;
      case 'EX_Jump': push(target(op.CodeOffset)); break;
      case 'EX_JumpIfNot':
        if(truth!=='EX_False')push(i+1);
        if(truth!=='EX_True')push(target(op.CodeOffset));
        break;
      case 'EX_PushExecutionFlow': push(i+1,[...stack,target(op.PushingAddress)]); break;
      case 'EX_PopExecutionFlow': pop(); break;
      case 'EX_PopExecutionFlowIfNot':
        if(truth!=='EX_False')push(i+1);
        if(truth!=='EX_True')pop();
        break;
      case 'EX_ComputedJump': throw new Error(f.name+' unexpected computed jump');
      default: push(i+1);
    }
  }
}
function denial(f,name,forbidden){
  const i=guardFor(f,name),off=f.rows[i].op.CodeOffset,visited=[];
  follow(f,f.offsets.get(off),(r,j)=>{
    visited.push(j);
    for(const n of forbidden)assert.ok(!r.calls.some(c=>c.name===n),f.name+' denied path reached '+n);
  });
  assert.ok(visited.some(j=>type(f.rows[j].op)==='EX_Return'),f.name+' denial path reaches a return');
  return {condition:name,offset:f.rows[i].offset,deniedOffset:off,instructions:visited};
}
function ownerAddress(f,name,field,owner) {
  const matches=[];
  for(const r of f.rows)for(const c of r.calls)if(c.name===name){
    const target=c.expression.Parameters?.[0];
    const member=target?.ContextExpression;
    if(type(member)!=='EX_InstanceVariable'||prop(member)!==field)continue;
    const resolved=object(f.a,member.Variable.New.ResolvedOwner)?.ObjectName;
    assert.equal(resolved,owner);
    assert.equal(type(target),'EX_Context');
    assert.ok(prop(target.ObjectExpression),field+' uses a live object');
    matches.push({offset:r.offset,field,owner:resolved,object:prop(target.ObjectExpression)});
  }
  assert.equal(matches.length,1,f.name+' exact '+field+' by-reference mutation');
  return matches[0];
}
function mapRemovalGuard(f,removal,independentSet) {
  const remove=f.offsets.get(removal.offset);
  let find=-1,out;
  for(let i=0;i<remove;i++)for(const c of f.rows[i].calls) {
    if(c.name==='Map_Find'&&prop(c.expression.Parameters?.[0]?.ContextExpression)===removal.field) {
      find=i;
      out=prop(c.expression.Parameters[2]);
    }
  }
  assert.ok(find>=0&&out,removal.field+' exact find value');
  const equality=f.rows.findIndex((r,i)=>i>find&&i<remove&&r.calls.some(c=>
    c.name==='EqualEqual_ObjectObject'&&c.expression.Parameters.some(p=>prop(p)===out)));
  assert.ok(equality>find,removal.field+' compares found schematic pointer');
  const same=destination(f.rows[equality].op),found=destination(f.rows[find].op);
  const both=f.rows.findIndex((r,i)=>i>equality&&i<remove&&r.calls.some(c=>
    c.name==='BooleanAND'&&[same,found].every(p=>c.expression.Parameters.some(x=>prop(x)===p))));
  assert.ok(both>equality,removal.field+' combines hit and exact value');
  const exact=destination(f.rows[both].op);
  const branch=f.rows.findIndex((r,i)=>i>both&&i<remove&&type(r.op)==='EX_JumpIfNot'&&prop(r.op.BooleanExpression)===exact);
  assert.ok(branch>both,removal.field+' guards removal');
  const denied=f.rows[branch].op.CodeOffset;
  let setReached=false;
  follow(f,f.offsets.get(denied),(r,i)=>{
    if(i<=find||r.calls.some(c=>c.name==='Array_Length'))return false;
    assert.notEqual(i,remove,removal.field+' foreign or absent map value must skip removal');
    if(r.calls.some(c=>c.name==='Set_Remove'))setReached=true;
  });
  if(independentSet)assert.ok(setReached,'foreign GUID replacement does not skip object-set removal');
  return {field:removal.field,find:f.rows[find].offset,guard:f.rows[branch].offset,denied,remove:removal.offset,setReached};
}
const assetRoot=path.dirname(batch.results.find(x=>path.basename(x.input)==='InitCave.uasset').input);
const evidence={build,kind,assetRoot,scope:'Serialized Blueprint bytecode and exact CDO tuple readback. Static build evidence; no Shipping behavior claim.',checks:[]};
const add=(name,detail)=>evidence.checks.push({name,detail});

const cdo=safety.Exports.find(e=>e.ObjectName==='Default__BP_DOR_SaveSafety_C');
const cdoArray=name=>{
  const p=cdo.Data.find(d=>d.Name===name);
  assert.ok(p?.Value?.length,name);
  return p.Value.map(x=>x.Value);
};
function objectPath(a,index) {
  const o=object(a,index);
  assert.ok(o,index);
  if(!o.OuterIndex)return o.ObjectName;
  return objectPath(a,o.OuterIndex)+'.'+o.ObjectName;
}
const id=x=>x[0].Value.replace(/[{}-]/g,'').toUpperCase();
const expected=expectations.registrationRows;
const rows=new Map(expected.map(x=>[x.rootGuid,x]));
const roots=cdoArray('DOR_RewardRoots'),schematics=cdoArray('DOR_RewardSchematics');
const weapons=cdoArray('DOR_RewardWeaponPaths'),indices=cdoArray('DOR_RewardWeaponIndices');
const pairs=cdoArray('DOR_RewardPairIds').map(id),schemaIds=cdoArray('DOR_RewardSchematicIds').map(id);
const legacy=cdoArray('DOR_RewardShellPaths');
const codes=cdoArray('DOR_RewardTestIndices');
const desiredCategories=cdoArray('DOR_RewardSchematicCategoryPaths');
const desiredUpgrades=cdoArray('DOR_RewardUpgradeCategoryPaths');
const carrierOverrides=cdoArray('DOR_RewardCarrierCategoryOverrides');
for(const col of [roots,schematics,indices,pairs,schemaIds,legacy,codes,desiredCategories,desiredUpgrades,carrierOverrides])
  assert.equal(col.length,453);
const checked=[];
for(let i=0;i<453;++i){
  const row=rows.get(pairs[i]);
  assert.ok(row,pairs[i]);
  rows.delete(pairs[i]);
  assert.equal(objectPath(safety,roots[i]),row.root);
  assert.equal(objectPath(safety,schematics[i]),row.schematic);
  assert.equal(weapons[indices[i]],row.weapon);
  assert.equal(schemaIds[i],row.schematicGuid);
  assert.equal(codes[i],row.testIndex);
  if(!legacy[i])checked.push(row.testIndex);
}
assert.equal(rows.size,0);
assert.deepEqual(checked.sort((a,b)=>a-b),[277,359,435,438,444]);
add('453 exact root, schematic, weapon, and GUID tuples preserve the legacy and common identities',{rows:pairs.length,legacy:checked,distinctWeapons:weapons.length});

const preflight=functions(safety,'DOR_RewardPreflight');
const publish=functions(safety,'DOR_RewardPublish');
const qualify=functions(safety,'DOR_RewardQualifyAndPublish');
const register=functions(safety,'DOR_RewardRegister');
const commit=functions(safety,'DOR_CommitRegistration');
const nativeSave=functions(safety,'DOR_CommitNativeSave');
for(const [name,values] of Object.entries(expectations.descriptorColumns))assert.deepEqual(cdoArray(name),values,name+' exact maintained descriptors');
const carrierAudit={mismatches:expectations.carrierOverrides};
assert.deepEqual(codes.filter((_,i)=>carrierOverrides[i]),[309,406,455]);
for(const m of carrierAudit.mismatches){
  const i=codes.indexOf(m.testIndex);
  assert.equal(i,m.offset);
  assert.equal(legacy[i],m.shell);
  assert.equal(desiredCategories[i],m.desired.category);
  assert.equal(desiredUpgrades[i],m.actual.upgrade);
  assert.equal(carrierOverrides[i],m.actual.category);
}
const carrierCheck=preflight.rows.find(r=>r.calls.some(c=>c.name==='EqualEqual_ObjectObject'&&
  c.expression.Parameters?.some(p=>prop(p.ContextExpression)==='SchematicCategory')&&
  c.expression.Parameters?.some(p=>prop(p)?.startsWith('DOR_RewardPreflight_CarrierCategory_'))));
assert.ok(carrierCheck,'compare live stock carrier to the resolved expected category pointer');
assert.ok(JSON.stringify(preflight.e.ScriptBytecode).includes('DOR_RewardCarrierCategoryOverrides'));
assert.ok(JSON.stringify(publish.e.ScriptBytecode).includes('DOR_RewardSchematicCategories'));
assert.ok(!JSON.stringify(publish.e.ScriptBytecode).includes('DOR_RewardCarrierCategoryOverrides'));
add('three exact source category overrides preserve all 453 fusion presentation descriptors',
  {pairs:[309,406,455],carrierCompareOffset:carrierCheck.offset});

assert.ok(has(preflight,'GetSchematicItem','EqualEqual_GuidGuid','Map_Find','Array_Contains','Array_AddUnique'));
assert.ok(!calls(preflight).some(n=>['Map_Add','SaveSlotToDisk','DOR_PrepareRegistration'].includes(n)));
for(const n of ['DOR_RewardPurchased','DOR_RewardOwned','DOR_RewardForged'])
  assert.ok(JSON.stringify(preflight.e.ScriptBytecode).includes(n));
checkOrder(qualify,'DOR_Initialize','DOR_RewardPreflight','DOR_RewardWithdraw','DOR_RewardRegister','DOR_RewardContextCurrent','DOR_RewardPublish');
checkOrder(register,'DOR_PrepareRegistration','DOR_RewardSameProfile','DOR_CommitRegistration');
checkOrder(commit,'DOR_ValidateJournal','DOR_CommitNativeSave','DeleteGameInSlot');
checkOrder(nativeSave,'SaveSlotToDisk','LoadGameFromSlot');
for(const n of ['DOR_SourceLiveObject','DOR_SourceSlot','Index','AnonymousID','PurchasedItemUpgrades','OwnedSchematics','ForgedSchematics'])
  assert.ok(JSON.stringify(nativeSave.e.ScriptBytecode).includes(n),n);
const failures=[
  denial(qualify,'DOR_RewardPreflight',['DOR_RewardRegister','DOR_RewardPublish']),
  denial(qualify,'DOR_RewardRegister',['DOR_RewardPublish']),
  denial(qualify,'DOR_RewardContextCurrent',['DOR_RewardPublish'])
];
const missing=qualify.rows.findIndex(r=>type(r.op)==='EX_JumpIfNot'&&prop(r.op.BooleanExpression)==='DOR_RewardMissing');
assert.ok(missing>=0);
const noChanges=[];
follow(qualify,qualify.offsets.get(qualify.rows[missing].op.CodeOffset),(r,i)=>{
  assert.ok(!r.calls.some(x=>x.name==='DOR_RewardRegister'));
  if(r.calls.some(x=>x.name==='DOR_RewardPublish'))noChanges.push(i);
});
assert.ok(noChanges.length);
const rigGates=qualify.rows.filter(r=>type(r.op)==='EX_JumpIfNot'&&prop(r.op.BooleanExpression)==='RigEntry');
assert.equal(rigGates.length,2);
for(const gate of rigGates)follow(qualify,qualify.offsets.get(gate.op.CodeOffset),r=>{
  assert.ok(!r.calls.some(c=>['DOR_RewardRegister','DOR_RewardPublish'].includes(c.name)),
    'non-rig branch must skip registration and publication');
});
add('all-row preflight and checked save/readback precede publication, and the complete profile bypasses the WAL',failures);
const mapAdd=ownerAddress(publish,'Map_Add','Overclocks','OverclockBank');
const hasMapping=guardFor(publish,'Map_Find');
const firstFind=at(publish,'Map_Find');
let addWhenMissing=false;
follow(publish,publish.offsets.get(publish.rows[hasMapping].op.CodeOffset),(r,i)=>{
  if(i<=firstFind)return false;
  if(r.offset===mapAdd.offset){addWhenMissing=true;return false;}
});
assert.ok(addWhenMissing);
follow(publish,hasMapping+1,(r,i)=>{
  if(i<=firstFind||r.calls.some(c=>c.name==='Array_Length'))return false;
  assert.notEqual(r.offset,mapAdd.offset,'already present mapping bypasses Map_Add');
});
add('publication fills missing exact live bank entries and bypasses a repeat write',
  {mapAdd,mappingGuard:publish.rows[hasMapping].offset});

const evict=functions(safety,'DOR_RewardEvict');
const withdraw=functions(safety,'DOR_RewardWithdraw');
const refresh=functions(safety,'DOR_RewardRefresh');
const context=functions(safety,'DOR_RewardContextCurrent');
const profile=functions(safety,'DOR_RewardSameProfile');
assert.ok(has(profile,'DOR_ResolveSourceSlot','EqualEqual_ObjectObject','EqualEqual_StrStr','EqualEqual_IntInt'));
for(const n of ['DOR_RewardSave','DOR_RewardSlot','DOR_RewardIndex','DOR_RewardAnon','Index','AnonymousID'])
  assert.ok(JSON.stringify(profile.e.ScriptBytecode).includes(n),n);
assert.ok(has(context,'DOR_RewardSameProfile','GetFSDGameData','GetOverclocksForItem','EqualEqual_ObjectObject'));
for(const n of ['DOR_RewardData','DOR_RewardSettings','DOR_RewardBanks'])
  assert.ok(JSON.stringify(context.e.ScriptBytecode).includes(n),n);
const callback=functions(safety,'DOR_RewardSettingsChanged');
const bind=functions(safety,'DOR_RewardBindSettings');
assert.equal(callback.e.LoadedProperties?.filter(x=>String(x.PropertyFlags).includes('CPF_Parm')).length??0,0);
assert.ok(has(callback,'DOR_RewardContextCurrent','DOR_RewardWithdraw'));
assert.ok(JSON.stringify(bind.e.ScriptBytecode).includes('OnSettingsChanged'));
assert.ok(JSON.stringify(bind.e.ScriptBytecode).includes('DOR_RewardSettingsChanged'));
assert.ok(has(functions(safety,'DOR_BackupTick'),'DOR_RewardRefresh'));
assert.ok(has(functions(guard,'ExecuteUbergraph_BP_DOR_PresetRecoveryGuard'),'DOR_RewardRefresh'));
assert.ok(has(refresh,'DOR_RewardQualifyAndPublish','DOR_RewardWithdraw'));
assert.ok(calls(withdraw).filter(x=>x==='DOR_RewardEvict').length>=2);
const failedRefresh=guardFor(refresh,'DOR_RewardQualifyAndPublish');
let failureWithdraw=false;
follow(refresh,refresh.offsets.get(refresh.rows[failedRefresh].op.CodeOffset),r=>{
  if(r.calls.some(c=>c.name==='DOR_RewardWithdraw'))failureWithdraw=true;
  if(destination(r.op)==='DOR_RewardReady')assert.notEqual(type(assignment(r.op)),'EX_True');
});
assert.ok(failureWithdraw,'failed qualification or publication retracts the active and tracked state');
assert.ok(!calls(evict).some(n=>['Map_Clear','Set_Clear','GetSchematics'].includes(n)));
const addresses=[
  ownerAddress(evict,'Map_Remove','Overclocks','OverclockBank'),
  ownerAddress(evict,'Map_Remove','AllSchematicsMap','SchematicSettings'),
  ownerAddress(evict,'Set_Remove','AllSchematics','SchematicSettings')
];
const exactGuards=[mapRemovalGuard(evict,addresses[0],false),mapRemovalGuard(evict,addresses[1],true)];
add('settings callback and lifecycle retract exact live bank, catalog GUID map, and catalog object set',
  {addresses,exactGuards,failedRefreshOffset:refresh.rows[failedRefresh].offset});

const receipt=functions(safety,'DOR_RewardReceipt');
const recorder=functions(safety,'DOR_NormalRecord');
const acquire=functions(safety,'DOR_NormalGetSink');
const serviceFlush=functions(safety,'DOR_NormalFlush');
const fileFlush=functions(sink,'DOR_NormalFlush');
const fileRead=functions(sink,'DOR_NormalRead');
const fileName=functions(sink,'DOR_NormalFilename');
assert.ok(JSON.stringify(receipt.e.ScriptBytecode).includes('[DOR]|event=REWARD_REGISTRATION|stage='));
checkOrder(receipt,'EqualEqual_StrStr','Not_PreBool','DOR_NormalRecord','DOR_NormalFlush');
const duplicate=denial(receipt,'Not_PreBool',['DOR_NormalRecord','DOR_NormalFlush']);
assert.ok(JSON.stringify(receipt.rows[at(receipt,'EqualEqual_StrStr')].op).includes('DOR_RewardLastReceipt'));
assert.ok(receipt.rows[at(receipt,'DOR_NormalFlush')].calls.some(c=>
  c.name==='DOR_NormalFlush'&&c.expression.Parameters?.some(p=>type(p)==='EX_True')));
checkOrder(recorder,'DOR_NormalGetSink','DOR_NormalEnqueue');
checkOrder(serviceFlush,'DOR_NormalGetSink','DOR_NormalFlush');
assert.ok(has(acquire,'DOR_NormalContext'));
for(const f of [receipt,recorder,acquire,serviceFlush]){
  assert.ok(!calls(f).some(n=>['DOR_Initialize','DOR_CanWrite','DOR_Prepare','PrintString','DOR_DebugNotify'].includes(n)),f.name);
}
assert.ok(has(fileFlush,'WriteToPlainText','DOR_NormalRead'));
assert.ok(has(fileRead,'ReadFromPlainText'));
assert.ok(JSON.stringify(fileName.e.ScriptBytecode).includes('normal-'));

const receiptStages=[
  [refresh,['resolve:bind-user-settings','resolve:live-save','resolve:source-slot','resolve:game-data','resolve:schematic-settings','publish:verified','adopt:verified']],
  [qualify,['qualify:release-old-source','qualify:safety-initialize','qualify:disabled','qualify:transaction-active','adopt:membership-missing','qualify:context-changed']],
  [preflight,['preflight:asset-invalid','preflight:identity-mismatch','preflight:bank-missing','preflight:bank-weapon-mismatch',
    'preflight:schematic-item-invalid','preflight:schematic-root-mismatch','preflight:category-invalid','preflight:carrier-invalid',
    'preflight:carrier-category-mismatch','preflight:foreign-bank-value','preflight:installed-weapon-mismatch','adopt:mapping-missing']],
  [register,['prepare:registration-rejected','prepare:source-profile-changed','commit:checked-save-rejected']],
  [publish,['publish:foreign-bank-value','publish:map-readback-mismatch']],
];
for(const [f,expected] of receiptStages){
  const entries=f.rows.filter(r=>r.calls.some(c=>c.name==='DOR_RewardReceipt'));
  assert.equal(entries.length,expected.length,f.name);
  for(const stage of expected){
    const matching=entries.filter(r=>JSON.stringify(r.op).includes('"'+stage+'"'));
    assert.equal(matching.length,1,f.name+' '+stage);
    if(f===refresh)continue;
    let returned=false;
    follow(f,f.offsets.get(matching[0].offset)+1,r=>{
      assert.ok(!r.calls.some(c=>c.name==='DOR_RewardReceipt'),f.name+' repeated terminal receipt '+stage);
      if(type(r.op)==='EX_Return')returned=true;
    });
    assert.ok(returned,f.name+' terminal receipt '+stage);
  }
}
for(const f of [preflight,qualify]){
  for(const field of ['PurchasedItemUpgrades','OwnedSchematics','ForgedSchematics','DOR_RewardSettings','DOR_RewardTestIndices'])
    assert.ok(JSON.stringify(f.e.ScriptBytecode).includes(field),f.name+' '+field);
}
add('bounded Shipping file receipts carry the first registration or adoption gate and exact runtime pair code',
  {duplicate,stages:receiptStages.flatMap(([,names])=>names),file:'normal-*.txt'});

const open=functions(master,'DOR_NativeOpenWindow');
const verify=functions(safety,'DOR_RewardVerifyRoots');
const nativeGuard=functions(master,'DOR_NativeRequireRegistered');
const ready=functions(master,'DOR_NativeBankReady');
checkOrder(open,'DOR_NativeCollectRoots','DOR_NativeRequireRegistered','DOR_Prepare','DOR_SourceEnroll','DOR_SourceBindRequest','DOR_SourceEquipmentOnly');
for(const n of ['DOR_SourceGrantQualification','DOR_SourceExtendQualificationScope','DOR_NativeBankCapture','DOR_NativeBankStage'])assert.ok(!has(open,n),n);
assert.ok(has(verify,'DOR_RewardContextCurrent','EqualEqual_GuidGuid','Map_Find','Array_Contains'));
assert.ok(JSON.stringify(verify.e.ScriptBytecode).includes('PurchasedItemUpgrades'));
assert.ok(JSON.stringify(verify.e.ScriptBytecode).includes('OwnedSchematics'));
assert.ok(JSON.stringify(verify.e.ScriptBytecode).includes('ForgedSchematics'));
assert.ok(has(nativeGuard,'DOR_RewardVerifyRoots','DOR_RewardWithdraw','DOR_DebugNotify'));
assert.ok(JSON.stringify(nativeGuard.e.ScriptBytecode).includes('Fusion overclocks are not registered'));
const notice=functions(master,'DOR_DebugNotify');
assert.ok(has(notice,'DOR_DiagnosticRetain'));
checkOrder(nativeGuard,'DOR_RewardVerifyRoots','DOR_DebugNotify','PostGameMessage');
const post= nativeGuard.rows[at(nativeGuard,'PostGameMessage')];
assert.ok(JSON.stringify(post.op).includes('Fusion overclocks are not registered'));
const verificationGate=guardFor(nativeGuard,'DOR_RewardVerifyRoots');
follow(nativeGuard,verificationGate+1,r=>assert.notEqual(r.offset,post.offset,'registered path bypasses notice'));
let reachedNotice=false;
follow(nativeGuard,nativeGuard.offsets.get(nativeGuard.rows[verificationGate].op.CodeOffset),r=>{
  if(r.offset===post.offset)reachedNotice=true;
});
assert.ok(reachedNotice,'failed verification reaches native HUD notice when GameState is valid');
assert.ok(has(ready,'EqualEqual_GuidGuid','DOR_NativeRequireRegistered'));
assert.ok(['NativeScopeRequest','NativeScopeSafety','NativeScopeRequired','NativeScopeEquipmentOnly']
  .every(n=>JSON.stringify(ready.e.ScriptBytecode).includes(n)));
add('mission original/accepted qualification gate denies before request WAL or source preparation',
  denial(open,'DOR_NativeRequireRegistered',['DOR_Prepare','DOR_SourceEnroll','DOR_SourceBindRequest']));

const rearm=functions(master,'DOR_NativeBeginLocalRecovery');
const restore=functions(master,'DOR_NativeRestoreLocalFields');
const finalize=functions(master,'DOR_NativeFinalizeLocal');
const equipment=functions(safety,'DOR_SourceEquipmentOnly');
checkOrder(rearm,'DOR_NativeRequireRegistered','DOR_SourceRebindProjection','DOR_SourceEquipmentOnly');
assert.ok(!has(rearm,'DOR_SourceRegrantRecordedQualification'));
assert.ok(!has(rearm,'DOR_NativeBankRearmOriginal'));
assert.ok(has(restore,'DOR_SourceEquipmentOnly','DOR_SourceRestoreFields'));
assert.ok(has(finalize,'DOR_SourceEquipmentOnly','DOR_SourceAcceptTerminal','DOR_SourceFinalize'));
assert.ok(has(equipment,'DOR_SourceBankClosed','DOR_SourceValidateRecord','DOR_SourceJournalReadback'));
assert.ok(['NativeBankSchema','QualificationScopePrepared','NativeProtocolPhase']
  .every(n=>JSON.stringify(equipment.e.ScriptBytecode).includes(n)));
assert.ok(!has(finalize,'DOR_NativeRequireRegistered')&&!has(restore,'DOR_NativeRequireRegistered'));
add('equipment-only closure and original recovery preserve checked receipts without temporary qualification',{functions:[rearm.name,restore.name,finalize.name,equipment.name]});

assert.ok(has(functions(safety,'DOR_SourceFinalizeRestoredWAL'),'DOR_SourceBankRecover','DeleteGameInSlot'));
const old=functions(safety,'DOR_SourceBankRecover');
assert.ok(has(old,'DOR_CheckIdentity','DOR_SourceBankValidate','DOR_SourceBankEntrySafe','Map_Add','Map_Remove','DOR_SourceJournalReadback'));
assert.deepEqual(cdoArray('DOR_BankKnownRootIds').map(id),pairs);
assert.deepEqual(cdoArray('DOR_BankKnownStagedIds').map(id),schemaIds);
assert.deepEqual(cdoArray('DOR_BankKnownRootPaths'),roots.map(i=>objectPath(safety,i)));
assert.deepEqual(cdoArray('DOR_BankKnownStagedPaths'),schematics.map(i=>objectPath(safety,i)));
assert.deepEqual(cdoArray('DOR_BankKnownWeaponPaths'),indices.map(i=>weapons[i]));
assert.ok(has(functions(safety,'DOR_Initialize'),'DOR_RestoreJournal'));
assert.ok(has(functions(safety,'DOR_RestoreJournal'),'DOR_SourceFinalizeRestoredWAL','DOR_SourceRestoreMembership'));
checkOrder(functions(safety,'DOR_RestoreJournal'),'DOR_RewardWithdraw','DOR_WriteDisabledMarker','DOR_SourceRestoreMembership');
const native=functions(master,'DOR_NativeSourceCalls');
checkOrder(native,'DOR_NativeRegisterClassRoot','EquipUpgrade');
const cache=functions(master,'DOR_NativeRegisterClassRoot');
assert.ok(JSON.stringify(cache.e.ScriptBytecode).includes('AllOverclocks'));
add('startup old WAL and native weapon-class cache paths still survive independently',{recovery:old.name,cache:cache.name});

const oldOwner=functions(proxy,'DOR91_BeginOwner');
assert.ok(has(oldOwner,'DOR_RegisterFusionMissionBank','DOR_BeginFusionTransientEligibility'));
const oldBank=functions(master,'DOR_RegisterFusionMissionBank');
const oldEligibility=functions(master,'DOR_BeginFusionTransientEligibility');
const legacyGuards=[];
for(const [f,mutation] of [[oldBank,'Map_Add'],[oldEligibility,'Array_AddUnique']]){
  const logic=f.rows.findIndex(r=>r.calls.some(c=>c.name==='BooleanOR'&&
    ['NativeDriverEnabled','NativeLegacyExecuting'].every(n=>c.expression.Parameters?.some(p=>prop(p)===n))));
  assert.ok(logic>=0,f.name+' common-mode predicate');
  const output=destination(f.rows[logic].op);
  const gate=f.rows.findIndex((r,i)=>i>logic&&type(r.op)==='EX_JumpIfNot'&&prop(r.op.BooleanExpression)===output);
  assert.ok(gate>logic&&at(f,mutation)>gate,f.name+' mutation follows common-mode gate');
  follow(f,gate+1,r=>assert.ok(!r.calls.some(c=>c.name===mutation),f.name+' common mode reached '+mutation));
  legacyGuards.push({function:f.name,branch:f.rows[gate].offset,skips:mutation});
}
// The saved Master CDO is opaque to UAssetAPI. UE4.27 still serializes this
// scalar as one exact tagged BoolProperty with zero payload and array index.
function cdoBool(a,owner,property){
  const e=a.Exports.find(x=>x.ObjectName==='Default__'+owner+'_C');
  assert.ok(e,owner);
  if(Array.isArray(e.Data))return e.Data.find(p=>p.Name===property)?.Value;
  assert.equal(type(e),'RawExport');
  const name=a.NameMap.indexOf(property),bool=a.NameMap.indexOf('BoolProperty');
  assert.ok(name>=0&&bool>=0,property);
  const header=Buffer.alloc(24);
  header.writeInt32LE(name,0);
  header.writeInt32LE(bool,8);
  const data=Buffer.from(e.Data,'base64'),index=data.indexOf(header);
  assert.ok(index>=0&&data.indexOf(header,index+1)===-1,property+' unique tagged property');
  assert.ok(data[index+24]===0||data[index+24]===1,property+' serialized bool');
  return data[index+24]===1;
}
assert.equal(cdoBool(master,'BP_DOR_Master','NativeDriverEnabled'),true);
assert.equal(cdoBool(proxy,'BP_DOR_PlayerProxy','NativeCommonMode'),true);
add('historical mission registration and eligibility are behind the enabled common-mode guards',
  {guards:legacyGuards,legacyCaller:oldOwner.name});
evidence.passed=true;
evidence.inputs=offsets.flatMap(({input,sha256,companions})=>[{input,sha256},...companions]).map(x=>({...x,relative:path.relative(assetRoot,x.input).replaceAll('\\','/')}));
fs.writeFileSync(path.join(evidenceDirectory,kind+'-containment-validation.json'),JSON.stringify(evidence,null,2)+'\n');
console.log(kind+': '+evidence.checks.length+' containment readback groups passed');


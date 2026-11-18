import assert from 'node:assert/strict';
import crypto from 'node:crypto';

export const sha256=b=>crypto.createHash('sha256').update(b).digest('hex');
export function objectPath(asset,index){
  if(!index)return null;
  const item=index>0?asset.Exports[index-1]:asset.Imports[-index-1];
  assert.ok(item,'Unresolved package index '+index);
  return (item.OuterIndex?objectPath(asset,item.OuterIndex)+'.':'')+item.ObjectName;
}
const indexKeys=new Set(['StackNode','ObjectPointer','ClassPointer','ClassPtr','InterfaceClass','PropertyClass','MetaClass','Enum','Struct','SignatureFunction','SuperStruct','Owner','Old','Interface','ClassIndex','OuterIndex','TemplateIndex','ResolvedOwner']);
export function normalize(asset,value,replacements=[],parent=null,key=''){
  if(typeof value==='string'){
    for(const [from,to] of replacements)if(from)value=value.replaceAll(from,to);
    return value;
  }
  if(typeof value==='number'){
    if(indexKeys.has(key)||(key==='Value'&&/(?:ObjectPropertyData|ClassPropertyData|EX_ObjectConst|EX_SoftObjectConst)/.test(parent?.$type??'')))return objectPath(asset,value);
    return value;
  }
  if(Array.isArray(value))return value.map(v=>normalize(asset,v,replacements,parent,key));
  if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([k])=>!['PropertyGuid','PropertyTagFlags','PropertyTypeName','PropertyTagExtensions','IsZero'].includes(k)).map(([k,v])=>[k,normalize(asset,v,replacements,value,k)]));
  return value;
}

// A bounded tagged-property decoder for the saved Master CDO, which UAssetAPI
// returns as RawExport. Unknown encodings fail; raw bytes are never accepted as
// an assertion about defaults. All payload and terminal boundaries are checked.
export function rawCDO(asset,base64,replacements=[]){
  const d=Buffer.from(base64,'base64');let p=0;
  const i32=()=>{const v=d.readInt32LE(p);p+=4;return v;};
  const name=()=>{const i=i32(),n=i32();assert.ok(i>=0&&i<asset.NameMap.length,'CDO FName '+i);return asset.NameMap[i]+(n?'_'+(n-1):'');};
  const str=()=>{const n=i32();assert.ok(Math.abs(n)<d.length,'CDO FString size');if(!n)return '';const bytes=Math.abs(n)*(n<0?2:1);const s=d.toString(n<0?'utf16le':'utf8',p,p+bytes-(n<0?2:1));p+=bytes;return normalize(asset,s,replacements);};
  const bytes=n=>{const s=d.subarray(p,p+n).toString('hex');p+=n;return s;};
  const text=()=>{const flags=d.readUInt32LE(p);p+=4;const history=d.readInt8(p++);if(history===-1)return {flags,history};assert.equal(history,0,'Unsupported CDO FText history');return {flags,history,namespace:str(),key:str(),source:str()};};
  function scalar(type,extra,field){
    switch(type){
      case 'StrProperty':return str();
      case 'NameProperty':return name();
      case 'ObjectProperty':case 'ClassProperty':return objectPath(asset,i32());
      case 'IntProperty':return i32();
      case 'UInt32Property':{const v=d.readUInt32LE(p);p+=4;return v;}
      case 'FloatProperty':{const v=d.readFloatLE(p);p+=4;return v;}
      case 'BoolProperty':return !!d[p++];
      case 'ByteProperty':return extra&&extra!=='None'?name():d[p++];
      case 'EnumProperty':return name();
      case 'TextProperty':return text();
      case 'StructProperty':
        if(extra==='Guid')return bytes(16);
        if(extra==='PointerToUberGraphFrame')return bytes(8);
        return tags();
      default:throw Error('Unsupported CDO type '+type+' in '+field);
    }
  }
  function tags(){
    const out=[];
    while(p<d.length){
      const n=name();if(n==='None')return out;
      const t=name(),size=i32(),arrayIndex=i32();let extra;
      if(t==='StructProperty'){extra=name();bytes(16);}
      else if(['ByteProperty','EnumProperty','ArrayProperty','SetProperty'].includes(t))extra=name();
      else if(t==='MapProperty')extra=[name(),name()];
      else if(t==='BoolProperty')extra=!!d[p++];
      if(d[p++])bytes(16);
      const end=p+size;assert.ok(size>=0&&end<=d.length,n+' CDO property boundary');
      let value;
      if(t==='BoolProperty')value=extra;
      else if(t==='ArrayProperty'){
        const count=i32();assert.ok(count>=0&&count<100000,n+' CDO array count');
        if(extra==='StructProperty'){
          const innerName=name(),innerType=name(),innerSize=i32(),innerIndex=i32(),structName=name();bytes(16);if(d[p++])bytes(16);
          assert.equal(innerType,'StructProperty',innerName);
          value=Array.from({length:count},()=>scalar(extra,structName,n));
        }else value=Array.from({length:count},()=>scalar(extra,null,n));
      }else if(t==='MapProperty'){
        const removed=i32();assert.equal(removed,0,n+' unexpected removed CDO map keys');
        const count=i32();assert.ok(count>=0&&count<100000,n+' CDO map count');
        // The only populated struct-key Master map is the Guid root catalog.
        if(count&&extra[0]==='StructProperty')assert.equal(n,'NativeKnownRoots','Unreviewed struct map key');
        value=Array.from({length:count},()=>[scalar(extra[0],extra[0]==='StructProperty'?'Guid':null,n),scalar(extra[1],null,n)]);
        value.sort((a,b)=>JSON.stringify(a[0]).localeCompare(JSON.stringify(b[0])));
      }else value=scalar(t,extra,n);
      assert.equal(p,end,n+' consumed CDO payload');
      out.push({name:n,type:t,arrayIndex,value});
    }
    throw Error('Missing CDO None terminator');
  }
  const result=tags();assert.equal(p,d.length,'Trailing raw CDO bytes');return result;
}

// Generated identities are decoded by role, never by a general GUID/number filter.
export function assetSemantics(asset,offsets,replacements=[]){
  const local=[...replacements],struct=asset.Exports.find(e=>e.ObjectName==='ST_DOR_RuntimePair'&&/UserDefinedStructExport/.test(e.$type));
  if(struct){
    const guid=struct.Data.find(p=>p.Name==='Guid')?.Value?.[0]?.Value;
    assert.match(guid??'',/^\{[A-F0-9-]{36}\}$/);local.push([guid,'<GENERATED-STRUCT-GUID>']);
    // Serialized fields exist in saved and cooked structures. Each generated
    // suffix remains tied to its actual ordered field; all types/flags/defaults compare.
    const seen=new Set();
    for(const field of struct.LoadedProperties){
      const match=field.Name.match(/^(.*_\d+)_([A-F0-9]{32})$/);assert.ok(match,'Generated struct field '+field.Name);
      assert.ok(!seen.has(match[2]),'Duplicate struct variable GUID');seen.add(match[2]);
      const formatted='{'+[match[2].slice(0,8),match[2].slice(8,12),match[2].slice(12,16),match[2].slice(16,20),match[2].slice(20)].join('-')+'}';
      local.push([field.Name,match[1]+'_<GENERATED-FIELD-GUID>'],[formatted,'<FIELD-GUID:'+match[1]+'>']);
    }
  }
  const functionOffsets=new Map(offsets.functions.map(f=>[f.name,new Map(f.statements.map((s,i)=>[s.offset,i]))]));
  const target=(name,offset)=>{const i=functionOffsets.get(name)?.get(offset);assert.ok(i!==undefined,name+' target '+offset);return {function:name,statement:i};};
  const latentIds=new Map();
  const functions=asset.Exports.filter(e=>e.ScriptBytecode?.length).map(e=>{
    const sizes=offsets.functions.find(f=>f.name===e.ObjectName);assert.ok(sizes,e.ObjectName+' offsets');
    assert.equal(sizes.computedSize,e.ScriptBytecodeSize,e.ObjectName+' size');
    const contexts=new Map((sizes.contexts??[]).map(c=>[c.path,c]));
    function code(v,k='',p=''){
      if(typeof v==='number'&&['CodeOffset','PushingAddress','NextOffset'].includes(k))return target(e.ObjectName,v);
      if(Array.isArray(v))return v.map((x,i)=>code(x,k,p?p+'.'+i:String(i)));
      if(v&&typeof v==='object'){
        const out=Object.fromEntries(Object.entries(v).map(([a,b])=>[a,code(b,a,p?p+'.'+a:a)]));
        if(/\.EX_(?:Context|Context_FailSilent|ClassContext),/.test(v.$type??'')){
          const c=contexts.get(p);assert.ok(c,e.ObjectName+' context evidence '+p);
          assert.equal(c.offset,v.Offset);assert.equal(c.computedSize,v.Offset);
          out.Offset='<VERIFIED-CONTEXT-EXPRESSION-SIZE>';
        }
        if(/\.EX_TextConst,/.test(v.$type??'')&&['GetModInfo','GetModPages'].includes(e.ObjectName)&&asset.Exports.some(x=>x.ObjectName==='BP_DOR_ModHub_C')){
          const t=v.Value;
          if(t.TextLiteralType==='LocalizedText'&&t.LocalizedNamespace?.Value===''){
            assert.match(t.LocalizedKey?.$type??'',/\.EX_StringConst,/);assert.match(t.LocalizedKey.Value,/^[A-F0-9]{32}$/);
            out.Value.LocalizedKey.Value='<GENERATED-LOCALIZATION-KEY>';
          }
        }
        if(/\.EX_LocalFinalFunction,/.test(v.$type??'')&&v.StackNode?.split('.').at(-1).startsWith('ExecuteUbergraph_')){
          assert.equal(v.Parameters.length,1);assert.match(v.Parameters[0].$type,/\.EX_IntConst,/);
          out.Parameters[0].Value=target(v.StackNode.split('.').at(-1),v.Parameters[0].Value);
        }
        if(/\.EX_StructConst,/.test(v.$type??'')&&v.Struct==='/Script/Engine.LatentActionInfo'){
          assert.equal(v.Value.length,4);const [link,id,fn,self]=v.Value;
          assert.match(link.$type,/\.EX_SkipOffsetConst,/);assert.match(id.$type,/\.EX_IntConst,/);assert.match(fn.$type,/\.EX_NameConst,/);assert.match(self.$type,/\.EX_Self,/);
          out.Value[0].Value=target(fn.Value,link.Value);
          // Preserve reuse/collision relationships while comparing compiler IDs by site.
          if(!latentIds.has(id.Value))latentIds.set(id.Value,latentIds.size);
          out.Value[1].Value={compilerLatentIdentity:latentIds.get(id.Value)};
        }
        return out;
      }
      return v;
    }
    // RightChop consumes the exact checked handshake prefix; its length changes
    // with the explicitly supplied build identity and is not an independent value.
    const bytecode=structuredClone(e.ScriptBytecode);
    if(e.ObjectName==='DOR_ConnectionValidateHello'){
      const calls=[];const visit=v=>{if(!v||typeof v!=='object')return;if(v.StackNode)calls.push(v);for(const x of Object.values(v))if(Array.isArray(x))x.forEach(visit);else visit(x);};visit(bytecode);
      const starts=calls.filter(c=>objectPath(asset,c.StackNode)==='/Script/Engine.KismetStringLibrary.StartsWith');
      const chops=calls.filter(c=>objectPath(asset,c.StackNode)==='/Script/Engine.KismetStringLibrary.RightChop');
      assert.equal(starts.length,1);assert.equal(chops.length,1);
      const prefix=starts[0].Parameters[1];assert.match(prefix.$type,/\.EX_StringConst,/);
      assert.equal(chops[0].Parameters[1].Value,prefix.Value.length,'Exact checked handshake prefix length');
      chops[0].Parameters[1].Value={lengthOfCheckedPrefix:normalize(asset,prefix.Value,local)};
    }
    return {name:e.ObjectName,flags:e.FunctionFlags,super:objectPath(asset,e.SuperStruct),properties:normalize(asset,e.LoadedProperties,local),code:code(normalize(asset,bytecode,local))};
  }).sort((a,b)=>a.name.localeCompare(b.name));
  const blueprint=functions.length>0||asset.Exports.some(e=>/BlueprintGeneratedClass$/.test(objectPath(asset,e.ClassIndex)??''));
  const defaults=asset.Exports.filter(e=>blueprint?e.ObjectName.startsWith('Default__'):Array.isArray(e.Data)).map(e=>{
    const data=typeof e.Data==='string'?rawCDO(asset,e.Data,local):normalize(asset,e.Data,local);
    if(/^DORPair_/.test(e.ObjectName)&&objectPath(asset,e.ClassIndex)==='/Script/FSD.OverclockUpgrade')for(const p of data){
      if(['Name','Description'].includes(p.Name)&&/\.TextPropertyData,/.test(p.$type??'')&&p.HistoryType==='Base'&&p.Namespace===''){
        assert.match(p.Value,/^[A-F0-9]{32}$/,'Generated FText key');p.Value='<GENERATED-LOCALIZATION-KEY>';
      }
    }
    return {name:e.ObjectName,class:objectPath(asset,e.ClassIndex),outer:objectPath(asset,e.OuterIndex),flags:e.ObjectFlags,data};
  }).sort((a,b)=>a.name.localeCompare(b.name));
  const result={functions,defaults};
  if(struct)result.structSchema={flags:struct.StructFlags,super:objectPath(asset,struct.SuperStruct),properties:normalize(asset,struct.LoadedProperties,local),defaults:normalize(asset,struct.StructData,local)};
  const names=['PRJ_DOR338_ChargedMine','PRJ_DOR339_ChargedPlasma','PRJ_DOR343_ChargedCluster','PRJ_DOR_IcicleSnowball','DORMerged_9324d3bdfd67de57_01'];
  if(asset.Exports.some(e=>names.includes(e.ObjectName)||names.some(n=>e.ObjectName===n+'_C'))){
    const classes=asset.Exports.filter(e=>/BlueprintGeneratedClass$/.test(objectPath(asset,e.ClassIndex)??'')).map(e=>({name:e.ObjectName,super:objectPath(asset,e.SuperStruct??e.SuperIndex),exportMapSuper:objectPath(asset,e.SuperIndex),template:objectPath(asset,e.TemplateIndex),parentCDO:objectPath(asset,e.ClassDefaultObject)})).sort((a,b)=>a.name.localeCompare(b.name));
    const templates=asset.Exports.filter(e=>{const c=objectPath(asset,e.ClassIndex);return Array.isArray(e.Data)&&e.OuterIndex&&(c?.startsWith('/Script/FSD.')||c?.endsWith('Component'));}).map(e=>({name:e.ObjectName,class:objectPath(asset,e.ClassIndex),outer:objectPath(asset,e.OuterIndex),template:objectPath(asset,e.TemplateIndex),data:normalize(asset,e.Data,local)})).sort((a,b)=>a.name.localeCompare(b.name));
    const parentImports=asset.Imports.map((e,i)=>objectPath(asset,-i-1)).filter(n=>/PRJ_MicroMissile|PRJ_Cryocannon|PRJ_Crye_SnowBall|CRV_MML_InstantAcceleration|SnowballExplosion_Cryocannon_Cue|P_Snowball_Explosion_Cryocannon/.test(n)).sort();
    assert.ok(!asset.Imports.some(e=>/NS_MicroMissile_ShotImpact/.test(e.ObjectName)),'Generated carrier must retain original class inheritance, not add Niagara imports');
    result.focusedInheritance={classes,templates,parentImports};
  }
  return result;
}

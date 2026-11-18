import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {prepareNativeInputs} from '../Analysis/src/authoring/prepare-native-inputs.mjs';
const args=process.argv.slice(2),options={publicRoot:path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..')};
const keys={'--pak':'pak','--shipping-exe':'shippingExe','--unreal-pak':'unrealPak','--dotnet':'dotnet','--inspector':'inspector','--output':'outputDirectory','--reuse-receipt':'reuseReceipt'};
for(let i=0;i<args.length;i+=2){if(!keys[args[i]] || !args[i+1])throw Error('Unknown/missing option '+args[i]);options[keys[args[i]]]=path.resolve(args[i+1]);}
for(const k of ['pak','shippingExe','unrealPak','dotnet','inspector','outputDirectory'])if(!options[k])throw Error('Required option missing: '+k);
console.log(JSON.stringify(await prepareNativeInputs(options),null,2));

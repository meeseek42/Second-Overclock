import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {buildAuthoring} from '../Analysis/src/authoring/build-authoring.mjs';
import {verifyPreparedInputs} from '../Analysis/src/authoring/prepared-inputs.mjs';
const args=process.argv.slice(2), root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
if(args.length!==2 || !['--source-receipt','--verify'].includes(args[0]))throw Error('Usage: node Scripts/Build-DORAuthoring.mjs --source-receipt <source-receipt.json> | --verify <authoring/prepared-inputs.json>');
if(args[0]==='--verify'){verifyPreparedInputs(path.resolve(args[1]),root);console.log('Prepared closure verified');}
else console.log(JSON.stringify(buildAuthoring({sourceReceipt:path.resolve(args[1]),publicRoot:root}),null,2));

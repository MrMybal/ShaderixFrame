import {createServer} from 'node:http';
import {readFile,stat,lstat} from 'node:fs/promises';
import {resolve,dirname,relative,isAbsolute,extname,sep} from 'node:path';
import {fileURLToPath} from 'node:url';
const source=dirname(fileURLToPath(import.meta.url));
const root=resolve(source,'../dist/website');
const args=process.argv.slice(2);
if(args.some(arg=>!/^--(?:port=\d+|prefix=\/[\w/-]*\/)$/.test(arg))) throw new Error('Usage: node website/preview.mjs [--port=0] [--prefix=/showcase/]');
const port=Number(args.find(arg=>arg.startsWith('--port='))?.slice(7)||0);
const prefix=args.find(arg=>arg.startsWith('--prefix='))?.slice(9)||'/showcase/';
if(port<0||port>65535||!Number.isInteger(port)||!prefix.endsWith('/')||prefix.includes('..')) throw new Error('Invalid preview options');
await stat(resolve(root,'index.html'));
const mime={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.svg':'image/svg+xml','.png':'image/png','.jpg':'image/jpeg','.webp':'image/webp','.json':'application/json; charset=utf-8','.md':'text/plain; charset=utf-8','.txt':'text/plain; charset=utf-8','.woff2':'font/woff2'};
const server=createServer(async(req,res)=>{
  try {
    if(!['GET','HEAD'].includes(req.method)) { res.writeHead(405).end(); return; }
    const pathname=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
    if(pathname==='/'||pathname===prefix.slice(0,-1)) { res.writeHead(302,{Location:prefix}).end(); return; }
    if(!pathname.startsWith(prefix)) { res.writeHead(404).end(); return; }
    let path=resolve(root,pathname.slice(prefix.length)||'index.html');
    const delta=relative(root,path);
    if(isAbsolute(delta)||delta.startsWith('..')) { res.writeHead(403).end(); return; }
    let current=root;
    for(const part of delta.split(sep).filter(Boolean)) { current=resolve(current,part); if((await lstat(current)).isSymbolicLink()) { res.writeHead(403).end(); return; } }
    if((await stat(path)).isDirectory()) path=resolve(path,'index.html');
    const data=await readFile(path);
    res.writeHead(200,{'Content-Type':mime[extname(path)]||'text/plain; charset=utf-8','Content-Length':data.length,'Cache-Control':'no-cache','X-Content-Type-Options':'nosniff'});
    res.end(req.method==='HEAD'?undefined:data);
  } catch { res.writeHead(404,{'Content-Type':'text/plain; charset=utf-8'}).end('Not found'); }
});
await new Promise((done,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',done);});
console.log(`ShaderixFrame showcase: http://127.0.0.1:${server.address().port}${prefix}`);
for(const signal of ['SIGINT','SIGTERM']) process.on(signal,()=>server.close(()=>process.exit(0)));

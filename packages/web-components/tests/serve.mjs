/**
 * Minimal static file server for Playwright tests.
 * Serves the repo root on port 7373 so test HTML fixtures can import
 * web-component bundles via relative paths.
 */
import { createServer } from 'node:http';
import { createReadStream, statSync } from 'node:fs';
import { join, extname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(fileURLToPath(import.meta.url), '../../');
const PORT = 7373;

const MIME = {
  '.html': 'text/html',
  '.js':   'application/javascript',
  '.mjs':  'application/javascript',
  '.ts':   'application/javascript',
  '.css':  'text/css',
  '.json': 'application/json',
  '.map':  'application/json',
};

createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const filePath = join(ROOT, url.pathname);

  try {
    const stat = statSync(filePath);
    if (stat.isDirectory()) {
      const index = join(filePath, 'index.html');
      statSync(index);
      res.writeHead(200, { 'Content-Type': 'text/html' });
      createReadStream(index).pipe(res);
      return;
    }
  } catch {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
    return;
  }

  const mime = MIME[extname(filePath)] ?? 'application/octet-stream';
  res.writeHead(200, { 'Content-Type': mime });
  createReadStream(filePath).pipe(res);
}).listen(PORT, () => {
  console.log(`Test server listening on http://localhost:${PORT}`);
});

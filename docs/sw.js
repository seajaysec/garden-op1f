// The browser version moved to https://seajaysec.github.io/garden-web/: this worker clears what the old one kept,
// takes itself away and reloads its pages, which then forward there.
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', e => e.waitUntil((async () => {
  for (const k of await caches.keys()) await caches.delete(k);
  await self.registration.unregister();
  for (const c of await self.clients.matchAll({type: 'window'})) c.navigate(c.url);
})()));

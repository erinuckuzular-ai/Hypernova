// Shared GitHub release lookup for the Pages Functions.
//
// Unauthenticated GitHub API calls are limited to 60 per hour per IP, so every
// response is cached for five minutes, first in this isolate's memory and then
// in the Cloudflare edge cache (caches.default). If GitHub fails, a stale copy
// is served when one exists. No secrets are required; if a GITHUB_TOKEN
// environment variable happens to be set it is used to raise the rate limit.

export const REPO = "erinuckuzular-ai/Hypernova";
export const RELEASES_PAGE = `https://github.com/${REPO}/releases/latest`;

const API = `https://api.github.com/repos/${REPO}`;
const TTL_SECONDS = 300;

// Isolate-level cache: { [path]: { at: ms, data } }
const memory = new Map();

function cacheKey(path, origin) {
  // A synthetic GET key on the site's own host; it is never routed, only used
  // as the Cache API lookup key.
  const base = origin || "https://hypernova.pages.dev";
  return new Request(`${base}/__cache/github${path}`);
}

async function fetchFromGitHub(path, env) {
  const headers = {
    "User-Agent": "hypernova-site",
    Accept: "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
  };
  if (env && env.GITHUB_TOKEN) headers.Authorization = `Bearer ${env.GITHUB_TOKEN}`;

  const res = await fetch(`${API}${path}`, { headers });
  if (!res.ok) throw new Error(`GitHub API ${res.status} for ${path}`);
  return res.json();
}

/**
 * GET a GitHub API path for the repo (e.g. "/releases/latest"), cached ~5 min.
 * @param {string} path
 * @param {{ env?: any, origin?: string, waitUntil?: (p: Promise<any>) => void }} ctx
 */
export async function githubJson(path, ctx = {}) {
  const now = Date.now();
  const hit = memory.get(path);
  if (hit && now - hit.at < TTL_SECONDS * 1000) return hit.data;

  let edge = null;
  try {
    edge = typeof caches !== "undefined" ? caches.default : null;
  } catch {
    edge = null;
  }

  if (edge) {
    try {
      const cached = await edge.match(cacheKey(path, ctx.origin));
      if (cached) {
        const data = await cached.json();
        memory.set(path, { at: now, data });
        return data;
      }
    } catch {
      // Cache miss or unavailable: fall through to GitHub.
    }
  }

  try {
    const data = await fetchFromGitHub(path, ctx.env);
    memory.set(path, { at: now, data });
    if (edge) {
      const store = edge
        .put(
          cacheKey(path, ctx.origin),
          new Response(JSON.stringify(data), {
            headers: {
              "Content-Type": "application/json",
              "Cache-Control": `public, max-age=${TTL_SECONDS}`,
            },
          }),
        )
        .catch(() => {});
      if (ctx.waitUntil) ctx.waitUntil(store);
      else await store;
    }
    return data;
  } catch (err) {
    // Serve a stale in-memory copy rather than failing outright.
    if (hit) return hit.data;
    throw err;
  }
}

/** The first asset whose name ends in .dmg, or null. */
export function findDmg(release) {
  const assets = (release && Array.isArray(release.assets)) ? release.assets : [];
  return assets.find((a) => typeof a.name === "string" && a.name.toLowerCase().endsWith(".dmg")) || null;
}

export function json(data, { status = 200, maxAge = 300 } = {}) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "Content-Type": "application/json; charset=utf-8",
      "Cache-Control": status === 200 ? `public, max-age=${maxAge}` : "no-store",
      "Access-Control-Allow-Origin": "*",
    },
  });
}

/** Pull env / waitUntil / origin out of a Pages Function context. */
export function ctxFrom(context) {
  let origin;
  try {
    origin = new URL(context.request.url).origin;
  } catch {
    origin = undefined;
  }
  return {
    env: context.env,
    origin,
    waitUntil: context.waitUntil ? context.waitUntil.bind(context) : undefined,
  };
}

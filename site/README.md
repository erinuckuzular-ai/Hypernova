# Hypernova download site

Static site plus three Cloudflare Pages Functions. No build step, no frameworks, no secrets.

```
site/
  index.html, style.css, app.js, favicon.svg
  assets/                 screenshots (full-size PNGs from docs/ and 1400px JPEGs used on the page)
  _headers                security headers + caching for static files
  _routes.json            which paths invoke Functions (everything else is served free as static)
  wrangler.toml           Pages project name, output dir (.) and compatibility date
  functions/
    _middleware.js        404s /functions/*, /wrangler.toml and /README.md so they aren't served
    download.js           GET /download      302 to the newest release's .dmg
    api/latest.js         GET /api/latest    { version, name, size, fileName, publishedAt, url, notes }
    api/releases.js       GET /api/releases  every release, newest first, with markdown notes
    _lib/github.js        shared GitHub lookup + 5-minute cache (not a route)
```

## How it works

- The **Download for Mac** button links to `/download`. That function asks the GitHub API for
  `repos/erinuckuzular-ai/Hypernova/releases/latest`, picks the asset whose name ends in `.dmg` and redirects
  to it. If anything fails it redirects to the GitHub releases page instead, so the button always goes somewhere.
- Publishing a new GitHub release (`./scripts/release.sh`) is all it takes to update the site: the version, size
  and changelog are read live. Keep the release asset name ending in `.dmg`.
- GitHub responses are cached for 5 minutes (in the isolate's memory and in the Cloudflare edge cache via
  `caches.default`) so visitors don't use up GitHub's 60-requests-per-hour unauthenticated limit.
- Release notes are rendered in the browser by a small markdown renderer in `app.js` that escapes all HTML first
  and only allows `http(s)`, `mailto` and same-site links.

## Deploy (Cloudflare Pages, free `*.pages.dev` URL)

Run wrangler **from inside `site/`**. Pages Functions are only picked up from a `functions/` folder in the
current directory; `npx wrangler pages deploy site` run from `~/Hypernova` would upload the static files
but silently skip the functions, so `/download` and `/api/*` would not exist.

```bash
cd ~/Hypernova/site
npx wrangler login                                      # once, opens the browser
npx wrangler pages deploy . --project-name hypernova    # first run offers to create the project
```

The site is then live at `https://hypernova.pages.dev` (or the next free name Cloudflare assigns).
Run the same deploy command again after editing the site. New GitHub releases do not need a redeploy.

The functions need **no secrets or environment variables**. Optionally, if you ever hit GitHub's rate limit,
add a `GITHUB_TOKEN` (a fine-grained token with read-only access to public repos is enough)
under Pages project > Settings > Variables and Secrets; the functions use it automatically when present.

If you add another function, add its path to `include` in `_routes.json`.

## Run locally

```bash
cd ~/Hypernova/site
npx wrangler pages dev --port 8788
open http://localhost:8788
```

`/download` should answer `302` with a `Location` pointing at the `.dmg`; `/api/latest` returns JSON.

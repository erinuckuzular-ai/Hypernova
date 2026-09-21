// GET /download -> 302 to the newest release's .dmg (falls back to the releases page).
import { githubJson, findDmg, ctxFrom, RELEASES_PAGE } from "./_lib/github.js";

function redirect(url) {
  return new Response(null, {
    status: 302,
    headers: { Location: url, "Cache-Control": "no-store" },
  });
}

export async function onRequest(context) {
  try {
    const release = await githubJson("/releases/latest", ctxFrom(context));
    const dmg = findDmg(release);
    if (dmg && typeof dmg.browser_download_url === "string" &&
        dmg.browser_download_url.startsWith("https://github.com/")) {
      return redirect(dmg.browser_download_url);
    }
  } catch {
    // Fall through to the releases page.
  }
  return redirect(RELEASES_PAGE);
}

// GET /api/releases -> [{ version, name, publishedAt, prerelease, url, notes, dmg }] newest first
import { githubJson, findDmg, ctxFrom, json } from "../_lib/github.js";

export async function onRequestGet(context) {
  try {
    const list = await githubJson("/releases?per_page=100", ctxFrom(context));
    const releases = (Array.isArray(list) ? list : [])
      .filter((r) => !r.draft)
      .sort((a, b) => Date.parse(b.published_at || 0) - Date.parse(a.published_at || 0))
      .map((r) => {
        const dmg = findDmg(r);
        return {
          version: r.tag_name,
          name: r.name || r.tag_name,
          publishedAt: r.published_at,
          prerelease: !!r.prerelease,
          url: r.html_url,
          notes: r.body || "",
          dmg: dmg ? { name: dmg.name, size: dmg.size, url: dmg.browser_download_url } : null,
        };
      });
    return json(releases);
  } catch (err) {
    return json({ error: "Could not reach GitHub releases." }, { status: 502 });
  }
}

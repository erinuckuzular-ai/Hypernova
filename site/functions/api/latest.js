// GET /api/latest -> { version, size, publishedAt, name, fileName, url, notes }
import { githubJson, findDmg, ctxFrom, json } from "../_lib/github.js";

export async function onRequestGet(context) {
  try {
    const release = await githubJson("/releases/latest", ctxFrom(context));
    const dmg = findDmg(release);
    return json({
      version: release.tag_name,
      name: release.name || release.tag_name,
      size: dmg ? dmg.size : null,
      fileName: dmg ? dmg.name : null,
      publishedAt: release.published_at,
      url: release.html_url,
      notes: release.body || "",
    });
  } catch (err) {
    return json({ error: "Could not reach GitHub releases." }, { status: 502 });
  }
}

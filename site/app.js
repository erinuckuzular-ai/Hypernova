/* Hypernova site: starfield, latest-release meta and the changelog. No dependencies. */
(function () {
  "use strict";

  var RELEASES_URL = "https://github.com/erinuckuzular-ai/Hypernova/releases";
  var reduceMotion = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)");

  /* ---------------- Starfield ---------------- */

  function starfield() {
    var canvas = document.getElementById("stars");
    if (!canvas || !canvas.getContext) return;
    var ctx = canvas.getContext("2d");
    var stars = [];
    var w = 0, h = 0, dpr = 1, raf = 0, last = 0;
    var COLORS = ["240,241,255", "70,232,255", "138,92,255", "255,169,74"];

    function seed() {
      var count = Math.min(260, Math.round((w * h) / 7000));
      stars = [];
      for (var i = 0; i < count; i++) {
        var r = Math.random();
        stars.push({
          x: Math.random() * w,
          y: Math.random() * h,
          z: 0.2 + Math.random() * 0.8,                 // depth: size, speed, brightness
          c: r < 0.8 ? COLORS[0] : r < 0.9 ? COLORS[1] : r < 0.96 ? COLORS[2] : COLORS[3],
          p: Math.random() * Math.PI * 2,               // twinkle phase
          s: 0.4 + Math.random() * 1.4                  // twinkle speed
        });
      }
    }

    function resize() {
      dpr = Math.min(window.devicePixelRatio || 1, 1.5);   // keep the canvas small
      w = window.innerWidth;
      h = window.innerHeight;
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(h * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      seed();
      draw(0, 0);
    }

    function draw(t, dt) {
      ctx.clearRect(0, 0, w, h);
      for (var i = 0; i < stars.length; i++) {
        var s = stars[i];
        if (dt) {
          s.x -= dt * 0.006 * s.z;                      // slow drift
          s.y += dt * 0.0015 * s.z;
          if (s.x < -2) s.x = w + 2;
          if (s.y > h + 2) s.y = -2;
        }
        var tw = t ? 0.55 + 0.45 * Math.sin(s.p + t * 0.001 * s.s) : 0.8;
        var a = (0.25 + 0.75 * s.z) * tw;
        var size = 0.4 + s.z * 1.1;
        ctx.fillStyle = "rgba(" + s.c + "," + a.toFixed(3) + ")";
        ctx.beginPath();
        ctx.arc(s.x, s.y, size, 0, 6.2832);
        ctx.fill();
        if (s.z > 0.93) {                               // a soft halo on the nearest stars
          ctx.fillStyle = "rgba(" + s.c + "," + (a * 0.12).toFixed(3) + ")";
          ctx.beginPath();
          ctx.arc(s.x, s.y, size * 4, 0, 6.2832);
          ctx.fill();
        }
      }
    }

    function frame(t) {
      var dt = last ? Math.min(t - last, 50) : 16;
      last = t;
      draw(t, dt);
      raf = requestAnimationFrame(frame);
    }

    function start() {
      stop();
      if (reduceMotion && reduceMotion.matches) { draw(0, 0); return; }
      last = 0;
      raf = requestAnimationFrame(frame);
    }

    function stop() {
      if (raf) cancelAnimationFrame(raf);
      raf = 0;
    }

    var resizeTimer = 0;
    window.addEventListener("resize", function () {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(resize, 150);
    });
    document.addEventListener("visibilitychange", function () {
      if (document.hidden) stop(); else start();
    });
    if (reduceMotion) {
      var onChange = function () { start(); };
      if (reduceMotion.addEventListener) reduceMotion.addEventListener("change", onChange);
      else if (reduceMotion.addListener) reduceMotion.addListener(onChange);
    }

    resize();
    start();
  }

  /* ---------------- Tiny safe markdown ---------------- */

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  // Only allow web links, mail links and same-site paths/anchors.
  function safeHref(raw) {
    var url = raw.trim();
    if (/^(https?:\/\/|mailto:|\/(?!\/)|#)/i.test(url)) return url;
    return null;
  }

  function emphasis(text) {
    text = text.replace(/\*\*([^*]+?)\*\*/g, "<strong>$1</strong>");
    text = text.replace(/__([^_]+?)__/g, "<strong>$1</strong>");
    text = text.replace(/(^|[^*\w])\*([^*\s][^*]*?)\*(?!\*)/g, "$1<em>$2</em>");
    text = text.replace(/(^|[^_\w])_([^_\s][^_]*?)_(?!\w)/g, "$1<em>$2</em>");
    return text;
  }

  // Inline formatting on text that has ALREADY been HTML-escaped. Code spans
  // and links are swapped for placeholders first so emphasis never reaches
  // inside a URL or a code span.
  function inline(text) {
    var tokens = [];
    function hold(html) {
      tokens.push(html);
      return "\u0000" + (tokens.length - 1) + "\u0000";
    }

    text = text.replace(/\u0000/g, "");
    text = text.replace(/`([^`]+)`/g, function (_, c) { return hold("<code>" + c + "</code>"); });

    // [label](url)
    text = text.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, function (m, label, url) {
      var href = safeHref(url);
      if (!href) return label;
      return hold('<a href="' + href + '" rel="noopener nofollow" target="_blank">' + emphasis(label) + "</a>");
    });

    // Bare https:// autolinks.
    text = text.replace(/(^|[\s(])(https?:\/\/[^\s<)]+[^\s<).,;:!?])/g, function (m, pre, url) {
      return pre + hold('<a href="' + url + '" rel="noopener nofollow" target="_blank">' + url + "</a>");
    });

    text = emphasis(text);
    return text.replace(/\u0000(\d+)\u0000/g, function (_, i) { return tokens[+i]; });
  }

  function renderMarkdown(md) {
    var lines = escapeHtml(String(md || "").replace(/\r\n?/g, "\n")).split("\n");
    var out = [];
    var para = [];
    var list = null; // { type: "ul" | "ol", items: [] }

    function flushPara() {
      if (para.length) out.push("<p>" + inline(para.join(" ")) + "</p>");
      para = [];
    }
    function flushList() {
      if (list) {
        out.push("<" + list.type + ">" + list.items.map(function (it) {
          return "<li>" + inline(it) + "</li>";
        }).join("") + "</" + list.type + ">");
      }
      list = null;
    }

    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      var m;

      if (/^\s*$/.test(line)) { flushPara(); flushList(); continue; }

      if ((m = /^\s{0,3}(#{1,6})\s+(.*?)\s*#*\s*$/.exec(line))) {
        flushPara(); flushList();
        // Release headings sit under an h3 on the page, so shift them down.
        var level = Math.min(6, m[1].length + 3);
        out.push("<h" + level + ">" + inline(m[2]) + "</h" + level + ">");
        continue;
      }

      if (/^\s{0,3}([-*_])(\s*\1){2,}\s*$/.test(line)) { flushPara(); flushList(); out.push("<hr>"); continue; }

      if ((m = /^\s*[-*+]\s+(.*)$/.exec(line))) {
        flushPara();
        if (!list || list.type !== "ul") { flushList(); list = { type: "ul", items: [] }; }
        list.items.push(m[1]);
        continue;
      }

      if ((m = /^\s*\d+[.)]\s+(.*)$/.exec(line))) {
        flushPara();
        if (!list || list.type !== "ol") { flushList(); list = { type: "ol", items: [] }; }
        list.items.push(m[1]);
        continue;
      }

      // Indented continuation of a list item.
      if (list && /^\s{2,}\S/.test(line)) {
        list.items[list.items.length - 1] += " " + line.trim();
        continue;
      }

      flushList();
      para.push(line.trim());
    }
    flushPara();
    flushList();
    return out.join("\n");
  }

  window.HypernovaMarkdown = renderMarkdown; // handy for testing in the console

  /* ---------------- Formatting ---------------- */

  function formatSize(bytes) {
    if (typeof bytes !== "number" || !isFinite(bytes) || bytes <= 0) return "";
    var mb = bytes / 1e6; // macOS Finder uses decimal megabytes
    return (mb >= 100 ? Math.round(mb) : mb.toFixed(1)) + " MB";
  }

  function formatDate(iso) {
    var d = new Date(iso);
    if (isNaN(d.getTime())) return "";
    try {
      return d.toLocaleDateString("en-US", { year: "numeric", month: "short", day: "numeric", timeZone: "UTC" });
    } catch (e) {
      return d.toISOString().slice(0, 10);
    }
  }

  function versionLabel(tag) {
    return String(tag || "").replace(/^v(?=\d)/i, "");
  }

  function el(tag, cls, html) {
    var n = document.createElement(tag);
    if (cls) n.className = cls;
    if (html != null) n.innerHTML = html;
    return n;
  }

  function getJson(url) {
    return fetch(url, { headers: { Accept: "application/json" } }).then(function (r) {
      if (!r.ok) throw new Error(url + " " + r.status);
      return r.json();
    });
  }

  /* ---------------- Latest release ---------------- */

  function latest() {
    var meta = document.getElementById("release-meta");
    if (!meta) return;
    getJson("/api/latest").then(function (d) {
      var parts = [];
      if (d.version) parts.push('<span class="ver">Version ' + escapeHtml(versionLabel(d.version)) + "</span>");
      var size = formatSize(d.size);
      if (size) parts.push(escapeHtml(size));
      var date = formatDate(d.publishedAt);
      if (date) parts.push("Released " + escapeHtml(date));
      if (!parts.length) throw new Error("empty");
      parts = parts.map(function (p) { return '<span class="item">' + p + "</span>"; });
      meta.innerHTML = parts.join('<span class="sep" aria-hidden="true">&middot;</span>');
    }).catch(function () {
      meta.innerHTML = '<a href="' + RELEASES_URL + '/latest">See the latest release on GitHub</a>';
    });
  }

  /* ---------------- Changelog ---------------- */

  var EXPANDED = 5;

  function releaseHead(r, isLatest) {
    var head = el("div", "release-head");
    head.appendChild(el("h3", null, escapeHtml(versionLabel(r.version))));
    if (r.publishedAt) {
      var t = el("time", null, escapeHtml(formatDate(r.publishedAt)));
      t.setAttribute("datetime", r.publishedAt);
      head.appendChild(t);
    }
    if (isLatest) head.appendChild(el("span", "badge", "Latest"));
    if (r.prerelease) head.appendChild(el("span", "badge pre", "Pre-release"));
    if (r.dmg && r.dmg.url && /^https:\/\/github\.com\//.test(r.dmg.url)) {
      var links = el("span", "links");
      var a = el("a", null, "Download " + escapeHtml(formatSize(r.dmg.size)));
      a.href = r.dmg.url;
      links.appendChild(a);
      head.appendChild(links);
    }
    return head;
  }

  function releases() {
    var root = document.getElementById("releases");
    if (!root) return;
    getJson("/api/releases").then(function (list) {
      if (!Array.isArray(list) || !list.length) throw new Error("empty");
      root.innerHTML = "";
      var latestTag = null;
      for (var k = 0; k < list.length; k++) {
        if (!list[k].prerelease) { latestTag = list[k].version; break; }
      }

      list.forEach(function (r, i) {
        var isLatest = r.version === latestTag;
        var notes = el("div", "notes", renderMarkdown(r.notes) || '<p class="muted">No notes for this release.</p>');

        if (i < EXPANDED) {
          var art = el("article", "release" + (isLatest ? " latest" : ""));
          art.appendChild(releaseHead(r, isLatest));
          art.appendChild(notes);
          root.appendChild(art);
        } else {
          if (i === EXPANDED) root.appendChild(el("p", "older-title", "Older releases"));
          var det = el("details", "release");
          var sum = document.createElement("summary");
          sum.appendChild(releaseHead(r, false));
          det.appendChild(sum);
          det.appendChild(notes);
          root.appendChild(det);
        }
      });
    }).catch(function () {
      root.innerHTML = '<p class="muted">Release notes could not be loaded right now. ' +
        '<a href="' + RELEASES_URL + '">Read them on GitHub</a>.</p>';
    });
  }

  /* ---------------- Boot ---------------- */

  starfield();
  latest();
  releases();
})();

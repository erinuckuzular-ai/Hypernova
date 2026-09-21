/* hypernova site: a playable wavetable in the browser.
   A 3D stack of 32 frames you can spin, a position slider that morphs through them, and a small
   WebAudio voice (PeriodicWave oscillators, low-pass filter, envelope) played from the keyboard or
   by clicking the keys. No dependencies. */
(function () {
  "use strict";

  var FRAMES = 32, POINTS = 128, HARMONICS = 48;

  /* ---------------- the tables ---------------- */
  // Harmonic amplitudes for frame f (0..1) of each table. Kept simple and recognisable.
  var TABLES = {
    analog: function (t, k) { // sine -> saw -> square
      var saw = 1 / k, sq = k % 2 ? 1 / k : 0;
      if (t < 0.5) { var a = t * 2; return (k === 1 ? 1 - a : 0) + saw * a; }
      var b = (t - 0.5) * 2; return saw * (1 - b) + sq * b;
    },
    formant: function (t, k) { // a moving vowel-like hump
      var centre = 2 + t * 14, width = 2.2 + t * 2;
      return Math.exp(-Math.pow((k - centre) / width, 2)) + (k === 1 ? 0.35 : 0);
    },
    metal: function (t, k) { // sparse, inharmonic-sounding partials that bloom
      var on = (k * 7 + Math.floor(t * 9)) % 5 === 0 || k === 1;
      return on ? Math.pow(0.9, k) * (0.4 + t) : 0;
    },
    fold: function (t, k) { // sine folding into brightness
      var drive = 1 + t * 12;
      return k % 2 ? Math.pow(Math.min(1, drive / (k * 1.2)), 1.5) / Math.sqrt(k) : 0;
    }
  };

  function harmonics(table, pos) {
    var h = new Float32Array(HARMONICS + 1);
    for (var k = 1; k <= HARMONICS; k++) h[k] = TABLES[table](pos, k);
    return h;
  }

  function frameShape(table, pos) {
    var h = harmonics(table, pos), out = new Float32Array(POINTS), peak = 0;
    for (var i = 0; i < POINTS; i++) {
      var x = i / POINTS, v = 0;
      for (var k = 1; k <= HARMONICS; k++) if (h[k]) v += h[k] * Math.sin(2 * Math.PI * k * x);
      out[i] = v;
      peak = Math.max(peak, Math.abs(v));
    }
    for (var j = 0; j < POINTS; j++) out[j] /= peak || 1;
    return out;
  }

  /* ---------------- 3D view ---------------- */
  function View(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");
    this.yaw = -0.55; this.pitch = 0.42; this.spin = 0.0025;
    this.table = "analog"; this.pos = 0.3; this.level = 0;
    this.frames = [];
    this.rebuild();
    var self = this, dragging = false, lx = 0, ly = 0;
    canvas.addEventListener("pointerdown", function (e) { dragging = true; lx = e.clientX; ly = e.clientY; self.spin = 0; canvas.setPointerCapture(e.pointerId); });
    canvas.addEventListener("pointermove", function (e) {
      if (!dragging) return;
      self.yaw += (e.clientX - lx) * 0.008;
      self.pitch = Math.max(0.05, Math.min(1.2, self.pitch + (e.clientY - ly) * 0.006));
      lx = e.clientX; ly = e.clientY;
    });
    canvas.addEventListener("pointerup", function () { dragging = false; });
    canvas.addEventListener("dblclick", function () { self.yaw = -0.55; self.pitch = 0.42; self.spin = 0.0025; });
  }
  View.prototype.rebuild = function () {
    this.frames = [];
    for (var f = 0; f < FRAMES; f++) this.frames.push(frameShape(this.table, f / (FRAMES - 1)));
  };
  View.prototype.project = function (x, y, z, w, h) {
    var cy = Math.cos(this.yaw), sy = Math.sin(this.yaw), cp = Math.cos(this.pitch), sp = Math.sin(this.pitch);
    var x1 = x * cy - z * sy, z1 = x * sy + z * cy;
    var y1 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
    var persp = 2.6 / (2.6 + z2), s = Math.min(w * 0.36, h * 0.62);
    return [w / 2 + x1 * s * persp, h * 0.56 - y1 * s * persp];
  };
  View.prototype.draw = function () {
    var c = this.canvas, dpr = Math.min(2, window.devicePixelRatio || 1);
    var w = c.clientWidth, h = c.clientHeight;
    if (c.width !== Math.round(w * dpr)) { c.width = Math.round(w * dpr); c.height = Math.round(h * dpr); }
    var g = this.ctx;
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    this.yaw += this.spin;

    // floor grid
    g.strokeStyle = "rgba(0,0,0,0.08)"; g.lineWidth = 1;
    for (var i = 0; i <= 8; i++) {
      var z = -0.8 + 1.6 * i / 8, x = -1 + 2 * i / 8, a, b;
      a = this.project(-1, -0.45, z, w, h); b = this.project(1, -0.45, z, w, h);
      g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.stroke();
      a = this.project(x, -0.45, -0.8, w, h); b = this.project(x, -0.45, 0.8, w, h);
      g.beginPath(); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.stroke();
    }

    // every frame, back to front
    var live = Math.round(this.pos * (FRAMES - 1));
    for (var f = FRAMES - 1; f >= 0; f--) {
      var zf = -0.8 + 1.6 * f / (FRAMES - 1), d = this.frames[f], isLive = f === live;
      g.beginPath();
      for (var p = 0; p < POINTS; p++) {
        var pt = this.project(-1 + 2 * p / (POINTS - 1), d[p] * (isLive ? 0.42 + this.level * 0.12 : 0.4), zf, w, h);
        if (p === 0) g.moveTo(pt[0], pt[1]); else g.lineTo(pt[0], pt[1]);
      }
      if (isLive) {
        g.strokeStyle = "#ff5a1f"; g.lineWidth = 2.6;
        g.shadowColor = "rgba(255,90,31,0.45)"; g.shadowBlur = 14;
      } else {
        g.strokeStyle = "rgba(17,17,17," + (0.06 + 0.22 * (1 - f / FRAMES)) + ")"; g.lineWidth = 1;
        g.shadowBlur = 0;
      }
      g.stroke();
    }
    g.shadowBlur = 0;
    this.level *= 0.9;
  };

  /* ---------------- sound ---------------- */
  var audio = null, master = null, filter = null, waves = {}, voices = {};

  function ensureAudio() {
    if (audio) return;
    var AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    audio = new AC();
    filter = audio.createBiquadFilter();
    filter.type = "lowpass"; filter.frequency.value = 3200; filter.Q.value = 2;
    master = audio.createGain(); master.gain.value = 0.22;
    filter.connect(master); master.connect(audio.destination);
  }

  function wave(table, pos) {
    var key = table + ":" + Math.round(pos * (FRAMES - 1));
    if (waves[key]) return waves[key];
    var h = harmonics(table, Math.round(pos * (FRAMES - 1)) / (FRAMES - 1));
    var real = new Float32Array(HARMONICS + 1), imag = new Float32Array(h);
    return (waves[key] = audio.createPeriodicWave(real, imag));
  }

  function noteOn(midi, view) {
    ensureAudio();
    if (!audio || voices[midi]) return;
    if (audio.state === "suspended") audio.resume();
    var t = audio.currentTime, freq = 440 * Math.pow(2, (midi - 69) / 12);
    var amp = audio.createGain();
    amp.gain.setValueAtTime(0, t);
    amp.gain.linearRampToValueAtTime(1, t + 0.008);
    amp.gain.setTargetAtTime(0.55, t + 0.01, 0.35);
    amp.connect(filter);
    // two slightly detuned oscillators for width
    var oscs = [-6, 6].map(function (cents) {
      var o = audio.createOscillator();
      o.setPeriodicWave(wave(view.table, view.pos));
      o.frequency.value = freq; o.detune.value = cents;
      o.connect(amp); o.start(t);
      return o;
    });
    voices[midi] = { amp: amp, oscs: oscs };
    view.level = 1;
  }

  function noteOff(midi) {
    var v = voices[midi];
    if (!v || !audio) return;
    var t = audio.currentTime;
    v.amp.gain.cancelScheduledValues(t);
    v.amp.gain.setTargetAtTime(0, t, 0.12);
    v.oscs.forEach(function (o) { o.stop(t + 0.8); });
    delete voices[midi];
  }

  function retune(view) {
    if (!audio) return;
    Object.keys(voices).forEach(function (m) {
      voices[m].oscs.forEach(function (o) { o.setPeriodicWave(wave(view.table, view.pos)); });
    });
  }

  /* ---------------- wiring ---------------- */
  function init() {
    var canvas = document.getElementById("wt3d");
    if (!canvas || !canvas.getContext) return;
    var view = new View(canvas);
    window.hypernovaWavetable = view; // handy for poking at from the console
    var pos = document.getElementById("wt-pos"), cut = document.getElementById("wt-cut");
    var tables = document.querySelectorAll("[data-table]");

    if (pos) pos.addEventListener("input", function () { view.pos = pos.value / 100; retune(view); });
    if (cut) cut.addEventListener("input", function () {
      ensureAudio();
      if (filter) filter.frequency.setTargetAtTime(80 * Math.pow(250, cut.value / 100), audio.currentTime, 0.02);
    });
    Array.prototype.forEach.call(tables, function (b) {
      b.addEventListener("click", function () {
        view.table = b.getAttribute("data-table");
        view.rebuild(); retune(view);
        Array.prototype.forEach.call(tables, function (x) { x.setAttribute("aria-pressed", x === b ? "true" : "false"); });
      });
    });

    // on-screen keys
    var keys = document.querySelectorAll("[data-note]");
    Array.prototype.forEach.call(keys, function (k) {
      var n = +k.getAttribute("data-note");
      k.addEventListener("pointerdown", function (e) { e.preventDefault(); k.classList.add("down"); noteOn(n, view); });
      ["pointerup", "pointerleave", "pointercancel"].forEach(function (ev) {
        k.addEventListener(ev, function () { k.classList.remove("down"); noteOff(n); });
      });
    });

    // computer keyboard: a w s e d f t g y h u j k, like most DAWs
    var map = { a: 48, w: 49, s: 50, e: 51, d: 52, f: 53, t: 54, g: 55, y: 56, h: 57, u: 58, j: 59, k: 60 };
    var section = document.getElementById("play");
    function active() {
      if (!section) return false;
      var r = section.getBoundingClientRect();
      return r.top < window.innerHeight * 0.8 && r.bottom > window.innerHeight * 0.2;
    }
    document.addEventListener("keydown", function (e) {
      var n = map[e.key && e.key.toLowerCase()];
      if (n === undefined || e.repeat || e.metaKey || e.ctrlKey || !active()) return;
      if (document.activeElement && /input|textarea|select/i.test(document.activeElement.tagName) && document.activeElement.type !== "range") return;
      noteOn(n, view);
      var el = document.querySelector('[data-note="' + n + '"]'); if (el) el.classList.add("down");
    });
    document.addEventListener("keyup", function (e) {
      var n = map[e.key && e.key.toLowerCase()];
      if (n === undefined) return;
      noteOff(n);
      var el = document.querySelector('[data-note="' + n + '"]'); if (el) el.classList.remove("down");
    });

    var reduce = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    if (reduce) view.spin = 0;
    var visible = true;
    if ("IntersectionObserver" in window) {
      new IntersectionObserver(function (es) { visible = es[0].isIntersecting; }).observe(canvas);
    }
    (function loop() {
      if (visible && !document.hidden) view.draw();
      requestAnimationFrame(loop);
    })();
  }

  /* ---------------- tilting hero ---------------- */
  function tilt() {
    var fig = document.querySelector(".hero-shot");
    if (!fig || (window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches)) return;
    var hero = document.querySelector(".hero");
    hero.addEventListener("pointermove", function (e) {
      var r = fig.getBoundingClientRect();
      var x = (e.clientX - (r.left + r.width / 2)) / r.width, y = (e.clientY - (r.top + r.height / 2)) / r.height;
      fig.style.transform = "perspective(1600px) rotateX(" + (8 - y * 8).toFixed(2) + "deg) rotateY(" + (x * 10).toFixed(2) + "deg)";
    });
    hero.addEventListener("pointerleave", function () { fig.style.transform = ""; });
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", function () { init(); tilt(); });
  else { init(); tilt(); }
})();

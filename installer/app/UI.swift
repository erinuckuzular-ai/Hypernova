import SwiftUI

// MARK: - Backdrop: deep navy, two nebula blooms and a slowly twinkling star field.

struct SpaceBackdrop: View {
    var body: some View {
        TimelineView(.animation(minimumInterval: 1 / 30)) { tl in
            Canvas { ctx, size in
                SpaceBackdrop.draw(&ctx, size: size, t: tl.date.timeIntervalSinceReferenceDate)
            }
        }
        .ignoresSafeArea()
    }

    static func draw(_ ctx: inout GraphicsContext, size: CGSize, t: Double) {
        let all = Path(CGRect(origin: .zero, size: size))
        ctx.fill(all, with: .linearGradient(Gradient(colors: [Color.bg1, Color.bg0]), startPoint: .zero, endPoint: CGPoint(x: 0, y: size.height)))
        let violetBloom = Gradient(colors: [Color.violet.opacity(0.22), Color.clear])
        ctx.fill(all, with: .radialGradient(violetBloom, center: CGPoint(x: 140, y: 50), startRadius: 0, endRadius: 320))
        let cyanBloom = Gradient(colors: [Color.cyan.opacity(0.14), Color.clear])
        ctx.fill(all, with: .radialGradient(cyanBloom, center: CGPoint(x: size.width - 60, y: size.height - 60), startRadius: 0, endRadius: 340))
        var rng = SplitMix(seed: 42)
        for _ in 0..<130 {
            let x = rng.next() * Double(size.width), y = rng.next() * Double(size.height)
            let s = 0.6 + rng.next() * 1.5, phase = rng.next() * 6.28, speed = 0.6 + rng.next() * 1.6
            let a = 0.1 + 0.35 * (0.5 + 0.5 * sin(t * speed + phase))
            ctx.fill(Path(ellipseIn: CGRect(x: x, y: y, width: s, height: s)), with: .color(Color.white.opacity(a)))
        }
    }
}

struct SplitMix {
    var state: UInt64
    init(seed: UInt64) { state = seed }
    mutating func next() -> Double {
        state &+= 0x9E3779B97F4A7C15
        var z = state
        z = (z ^ (z >> 30)) &* 0xBF58476D1CE4E5B9
        z = (z ^ (z >> 27)) &* 0x94D049BB133111EB
        return Double((z ^ (z >> 31)) >> 11) / Double(1 << 53)
    }
}

// MARK: - The nova: a four-point star with an orbit ring spinning in 3D.

struct Nova: View {
    var energy: Double = 1   // spins faster while installing

    var body: some View {
        TimelineView(.animation) { tl in
            Canvas { ctx, size in
                Nova.draw(&ctx, size: size, t: tl.date.timeIntervalSinceReferenceDate, energy: energy)
            }
        }
    }

    static func draw(_ ctx: inout GraphicsContext, size: CGSize, t: Double, energy: Double) {
        let c = CGPoint(x: size.width / 2, y: size.height / 2)
        let r = Double(min(size.width, size.height)) * 0.42
        let glowRect = CGRect(x: c.x - r, y: c.y - r, width: r * 2, height: r * 2)
        let glow = Gradient(colors: [Color.cyan.opacity(0.28), Color.clear])
        ctx.fill(Path(ellipseIn: glowRect), with: .radialGradient(glow, center: c, startRadius: 0, endRadius: r))
        let colours: [Color] = [.violet, .cyan, .pink]
        for k in 0..<3 {
            let ring = orbit(centre: c, radius: r, yaw: t * 0.5 * energy + Double(k) * 2.1, tilt: 0.35 + 0.2 * Double(k))
            ctx.stroke(ring, with: .color(colours[k].opacity(0.12)), lineWidth: 7)
            ctx.stroke(ring, with: .color(colours[k].opacity(0.75)), lineWidth: 1.6)
        }
        let s = star(centre: c, radius: r, pulse: 1 + 0.06 * sin(t * 2.2 * energy))
        ctx.fill(s, with: .color(Color.cyan.opacity(0.22)))
        ctx.stroke(s, with: .color(Color.cyan.opacity(0.18)), lineWidth: 10)
        ctx.stroke(s, with: .color(Color.cyan), lineWidth: 2.2)
        let core = Gradient(colors: [Color.white, Color.cyan.opacity(0)])
        ctx.fill(Path(ellipseIn: CGRect(x: c.x - 9, y: c.y - 9, width: 18, height: 18)),
                 with: .radialGradient(core, center: c, startRadius: 0, endRadius: 12))
    }

    // An ellipse rotated in 3D (yaw), squashed by tilt, with light perspective.
    static func orbit(centre c: CGPoint, radius r: Double, yaw: Double, tilt: Double) -> Path {
        var path = Path()
        let cy = cos(yaw), sy = sin(yaw)
        for i in 0...96 {
            let a = Double(i) / 96.0 * 2.0 * Double.pi
            let x = cos(a) * cy - sin(a) * sy * 0.2
            let z = cos(a) * sy + sin(a) * cy * 0.2
            let y = sin(a) * tilt
            let p = 3.0 / (3.0 + z * 0.6)
            let px = Double(c.x) + x * r * 0.95 * p
            let py = Double(c.y) + (y - z * 0.15) * r * 0.95 * p
            if i == 0 { path.move(to: CGPoint(x: px, y: py)) } else { path.addLine(to: CGPoint(x: px, y: py)) }
        }
        return path
    }

    static func star(centre c: CGPoint, radius r: Double, pulse: Double) -> Path {
        var path = Path()
        for i in 0..<8 {
            let a = Double(i) / 8.0 * 2.0 * Double.pi - Double.pi / 2.0
            let rr = (i % 2 == 0 ? r * 0.72 : r * 0.1) * pulse
            let pt = CGPoint(x: Double(c.x) + cos(a) * rr, y: Double(c.y) + sin(a) * rr)
            if i == 0 { path.move(to: pt) } else { path.addLine(to: pt) }
        }
        path.closeSubpath()
        return path
    }
}

// MARK: - Pieces

struct NeonButton: View {
    let title: String
    var primary = true
    let action: () -> Void
    @State private var hover = false

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.heavy(14)).kerning(2)
                .foregroundStyle(primary ? Color.bg0 : Color.textMain)
                .frame(maxWidth: .infinity, minHeight: 44)
                .background {
                    if primary {
                        RoundedRectangle(cornerRadius: 12)
                            .fill(LinearGradient(colors: [.cyan, .violet], startPoint: .leading, endPoint: .trailing))
                            .shadow(color: Color.cyan.opacity(hover ? 0.55 : 0.3), radius: hover ? 16 : 10)
                    } else {
                        RoundedRectangle(cornerRadius: 12).fill(Color.panel)
                            .overlay(RoundedRectangle(cornerRadius: 12).stroke(hover ? Color.white.opacity(0.2) : Color.line))
                    }
                }
        }
        .buttonStyle(.plain)
        .onHover { hover = $0 }
    }
}

struct PartToggle: View {
    @Binding var part: Part
    var colour: Color

    var body: some View {
        Button { part.on.toggle() } label: {
            HStack(spacing: 12) {
                ZStack {
                    Circle().fill(part.on ? colour : Color.bg0).frame(width: 14, height: 14)
                        .shadow(color: part.on ? colour.opacity(0.7) : .clear, radius: 6)
                    Circle().stroke(part.on ? colour : Color.white.opacity(0.2), lineWidth: 1.2).frame(width: 14, height: 14)
                }
                VStack(alignment: .leading, spacing: 1) {
                    Text(part.title).font(.demi(13)).foregroundStyle(part.on ? Color.textMain : Color.textDim)
                    Text(part.detail).font(.body(11)).foregroundStyle(Color.textDim)
                }
                Spacer()
            }
            .padding(.horizontal, 12).padding(.vertical, 7)
            .background(RoundedRectangle(cornerRadius: 10).fill(part.on ? colour.opacity(0.08) : Color.panel.opacity(0.6)))
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(part.on ? colour.opacity(0.45) : Color.line))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

struct Wordmark: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("HYPERNOVA").font(.heavy(30)).kerning(4).foregroundStyle(Color.textMain)
            Text("WAVETABLE SPACE SYNTH").font(.demi(10)).kerning(3.2).foregroundStyle(Color.textDim)
        }
    }
}

struct SweepBar: View {
    var body: some View {
        TimelineView(.animation) { tl in
            GeometryReader { geo in
                let t = tl.date.timeIntervalSinceReferenceDate
                let w = geo.size.width, x = (t.truncatingRemainder(dividingBy: 1.6) / 1.6) * (w + 160) - 160
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.panel)
                    Capsule().fill(LinearGradient(colors: [.clear, .cyan, .violet, .clear], startPoint: .leading, endPoint: .trailing))
                        .frame(width: 160).offset(x: x)
                }
                .clipShape(Capsule())
            }
        }
        .frame(height: 6)
    }
}

// MARK: - Screens

struct InstallerView: View {
    @EnvironmentObject var installer: Installer

    var body: some View {
        ZStack {
            SpaceBackdrop()
            HStack(spacing: 0) {
                Nova(energy: installer.phase == .working ? 3 : 1)
                    .frame(width: 270)
                    .padding(.leading, 10)
                VStack(alignment: .leading, spacing: 0) {
                    Wordmark().padding(.top, 38)
                    Group {
                        switch installer.phase {
                        case .welcome: welcome
                        case .working: working
                        case .done: done
                        case .failed(let msg): failed(msg)
                        }
                    }
                    .transition(.opacity)
                }
                .padding(.trailing, 34)
                .animation(.easeInOut(duration: 0.25), value: installer.phase)
            }
        }
    }

    private var welcome: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(installer.headline.uppercased()).font(.demi(10)).kerning(2).foregroundStyle(Color.cyan)
                .padding(.horizontal, 10).padding(.vertical, 4)
                .background(Capsule().fill(Color.cyan.opacity(0.1))).overlay(Capsule().stroke(Color.cyan.opacity(0.4)))
                .padding(.top, 14).padding(.bottom, 8)
            let colours: [Color] = [.cyan, .violet, .mint, .pink]
            ForEach(installer.parts.indices, id: \.self) { i in
                PartToggle(part: $installer.parts[i], colour: colours[i])
            }
            Spacer(minLength: 10)
            NeonButton(title: installer.installedVersion == nil ? "INSTALL" : "UPDATE") { installer.install() }
                .disabled(!installer.parts.contains { $0.on })
                .opacity(installer.parts.contains { $0.on } ? 1 : 0.4)
            Text("Asks for your Mac password once, to put the plug-ins in /Library. Your saved sounds are kept.")
                .font(.body(10.5)).foregroundStyle(Color.textDim)
                .fixedSize(horizontal: false, vertical: true).padding(.bottom, 22)
        }
    }

    private var working: some View {
        VStack(alignment: .leading, spacing: 14) {
            Spacer()
            Text("INSTALLING").font(.heavy(14)).kerning(3).foregroundStyle(Color.cyan)
            SweepBar()
            Text(installer.status).font(.body(12.5)).foregroundStyle(Color.textDim)
            Spacer()
        }
    }

    private var done: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("READY FOR LIFTOFF").font(.heavy(15)).kerning(3).foregroundStyle(Color.mint).padding(.top, 22)
            VStack(alignment: .leading, spacing: 8) {
                step("1", "Open Ableton Live > Settings > Plug-Ins and click Rescan.")
                step("2", "Find it in Browser > Plug-Ins > Arrow > Hypernova.")
                step("3", "Start with Classic Log, Rager 808 or Hypernova. The FOUNDERS PACK is under Sound Packs.")
                step("4", "Share sounds: Export from the preset menu. Friends drag the file onto Hypernova.")
            }
            if installer.abletonRunning {
                Text("Ableton is open: rescan plug-ins (or restart it) to load the new version.")
                    .font(.body(11)).foregroundStyle(Color.pink)
            }
            Spacer(minLength: 8)
            HStack(spacing: 10) {
                if installer.abletonURL != nil {
                    NeonButton(title: "OPEN ABLETON") { installer.openAbleton(); NSApp.terminate(nil) }
                } else if installer.parts.first(where: { $0.id == "app" })?.on == true {
                    NeonButton(title: "OPEN HYPERNOVA") { installer.openHypernova(); NSApp.terminate(nil) }
                }
                NeonButton(title: "DONE", primary: false) { NSApp.terminate(nil) }
            }
            .padding(.bottom, 26)
        }
    }

    private func failed(_ msg: String) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Spacer()
            Text("THAT DIDN'T WORK").font(.heavy(15)).kerning(3).foregroundStyle(Color.pink)
            Text(msg).font(.body(12)).foregroundStyle(Color.textDim).fixedSize(horizontal: false, vertical: true).lineLimit(6)
            Text("You can also open \"Everything else\" in the disk image and run Install Hypernova.pkg.")
                .font(.body(11)).foregroundStyle(Color.textDim)
            NeonButton(title: "TRY AGAIN") { installer.phase = .welcome }.frame(width: 180)
            Spacer()
        }
    }

    private func step(_ n: String, _ text: String) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Text(n).font(.heavy(11)).foregroundStyle(Color.bg0).frame(width: 20, height: 20)
                .background(Circle().fill(Color.cyan))
            Text(text).font(.body(12.5)).foregroundStyle(Color.textMain).fixedSize(horizontal: false, vertical: true)
        }
    }
}

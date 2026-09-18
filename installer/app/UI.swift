import SwiftUI

// MARK: - Art (rendered by MakeInstallerArt from the plug-in's backdrop shader, bundled in Resources)

enum Art {
    static func image(_ name: String) -> NSImage? {
        Bundle.main.url(forResource: name, withExtension: "png").flatMap { NSImage(contentsOf: $0) }
    }
    static let nebula = image("nebula")
    static let disk = image("disk")
}

// MARK: - Backdrop: the nebula drifting slowly, stars twinkling. All GPU-composited transforms.

struct Cosmos: View {
    var body: some View {
        TimelineView(.animation(minimumInterval: 1 / 30)) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            ZStack {
                Color.bg0
                if let nebula = Art.nebula {
                    Image(nsImage: nebula)
                        .resizable()
                        .scaledToFill()
                        .scaleEffect(1.12 + 0.03 * sin(t * 0.05))
                        .offset(x: 14 * sin(t * 0.031), y: 9 * cos(t * 0.027))
                        .opacity(0.95)
                }
                Canvas { ctx, size in
                    var rng = SplitMix(seed: 7)
                    for _ in 0..<90 {
                        let x = rng.next() * Double(size.width), y = rng.next() * Double(size.height)
                        let s = 0.7 + rng.next() * 1.6, phase = rng.next() * 6.28, speed = 0.5 + rng.next() * 1.8
                        let a = 0.15 + 0.6 * (0.5 + 0.5 * sin(t * speed + phase))
                        ctx.fill(Path(ellipseIn: CGRect(x: x, y: y, width: s, height: s)), with: .color(Color.white.opacity(a)))
                    }
                }
                LinearGradient(colors: [.clear, Color.bg0.opacity(0.55)], startPoint: .center, endPoint: .trailing)
            }
        }
        .ignoresSafeArea()
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

// MARK: - The black hole: the accretion disk spins in perspective, the back half passes behind the horizon.

struct BlackHole: View {
    var energy: Double = 1   // spins faster while working
    var size: CGFloat = 300

    var body: some View {
        TimelineView(.animation(minimumInterval: 1 / 30)) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            let spin = Angle(degrees: (t * 16 * energy).truncatingRemainder(dividingBy: 360))
            let horizon = size * 0.16
            ZStack {
                Circle()
                    .fill(RadialGradient(colors: [Color.gold.opacity(0.32), Color.plasma.opacity(0.08), .clear],
                                         center: .center, startRadius: horizon, endRadius: size * 0.55))
                    .scaleEffect(1 + 0.04 * sin(t * 1.3 * energy))
                disk(spin, squash: 0.28).mask(fade(topVisible: true))          // far side of the disk, behind
                disk(spin, squash: 0.95).opacity(0.55).mask(fade(topVisible: true, band: 0.34)) // lensed over the top
                Circle().fill(Color.black).frame(width: horizon * 2, height: horizon * 2)
                Circle().stroke(Color(red: 1, green: 0.86, blue: 0.64), lineWidth: 2.2)
                    .frame(width: horizon * 2.14, height: horizon * 2.14).blur(radius: 1)
                Circle().stroke(Color.gold.opacity(0.55), lineWidth: 7)
                    .frame(width: horizon * 2.14, height: horizon * 2.14).blur(radius: 7)
                disk(spin, squash: 0.28).mask(fade(topVisible: false))         // near side, in front
            }
            .frame(width: size, height: size)
            .rotationEffect(.degrees(-10))
        }
    }

    private func disk(_ spin: Angle, squash: CGFloat) -> some View {
        Group {
            if let d = Art.disk {
                Image(nsImage: d).resizable().frame(width: size, height: size)
                    .rotationEffect(spin)
                    .scaleEffect(x: 1, y: squash)
            }
        }
    }

    // Soft split at the disk's centre line, so the near and far halves blend instead of cutting.
    private func fade(topVisible: Bool, band: CGFloat = 0.5) -> some View {
        LinearGradient(stops: topVisible
                       ? [.init(color: .white, location: 0), .init(color: .white, location: band - 0.02), .init(color: .clear, location: band + 0.03)]
                       : [.init(color: .clear, location: 0.47), .init(color: .white, location: 0.52), .init(color: .white, location: 1)],
                       startPoint: .top, endPoint: .bottom)
            .frame(width: size, height: size)
    }
}

// MARK: - Pieces

struct GlowButton: View {
    let title: String
    var style: Style = .primary
    let action: () -> Void
    @State private var hover = false
    enum Style { case primary, secondary, danger }

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.heavy(14)).kerning(2.2)
                .foregroundStyle(style == .secondary ? Color.textMain : Color.bg0)
                .frame(maxWidth: .infinity, minHeight: 44)
                .background {
                    switch style {
                    case .primary:
                        RoundedRectangle(cornerRadius: 12)
                            .fill(LinearGradient(colors: [.gold, .plasma, .violet], startPoint: .leading, endPoint: .trailing))
                            .shadow(color: Color.gold.opacity(hover ? 0.55 : 0.3), radius: hover ? 18 : 10)
                    case .danger:
                        RoundedRectangle(cornerRadius: 12).fill(Color.plasma)
                            .shadow(color: Color.plasma.opacity(hover ? 0.55 : 0.3), radius: hover ? 16 : 8)
                    case .secondary:
                        RoundedRectangle(cornerRadius: 12).fill(Color.panel.opacity(0.8))
                            .overlay(RoundedRectangle(cornerRadius: 12).stroke(hover ? Color.white.opacity(0.25) : Color.line))
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
                    Circle().fill(part.on ? colour : Color.bg0).frame(width: 13, height: 13)
                        .shadow(color: part.on ? colour.opacity(0.8) : .clear, radius: 6)
                    Circle().stroke(part.on ? colour : Color.white.opacity(0.25), lineWidth: 1.2).frame(width: 13, height: 13)
                }
                VStack(alignment: .leading, spacing: 1) {
                    Text(part.title).font(.demi(13)).foregroundStyle(part.on ? Color.textMain : Color.textDim)
                    Text(part.detail).font(.body(11)).foregroundStyle(Color.textDim)
                }
                Spacer()
            }
            .padding(.horizontal, 12).padding(.vertical, 6)
            .background(RoundedRectangle(cornerRadius: 10).fill(Color.panel.opacity(part.on ? 0.75 : 0.45)))
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(part.on ? colour.opacity(0.5) : Color.line))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

struct Wordmark: View {
    var subtitle = "WAVETABLE SPACE SYNTH"
    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text("HYPERNOVA").font(.heavy(31)).kerning(5)
                .foregroundStyle(LinearGradient(colors: [.textMain, Color(red: 0.85, green: 0.78, blue: 1.0)], startPoint: .leading, endPoint: .trailing))
            Text(subtitle).font(.demi(10)).kerning(3.4).foregroundStyle(Color.textDim)
        }
    }
}

struct SweepBar: View {
    var colours: [Color] = [.gold, .plasma, .violet]
    var body: some View {
        TimelineView(.animation(minimumInterval: 1 / 30)) { tl in
            GeometryReader { geo in
                let t = tl.date.timeIntervalSinceReferenceDate
                let w = geo.size.width, x = (t.truncatingRemainder(dividingBy: 1.6) / 1.6) * (w + 160) - 160
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.panel.opacity(0.8))
                    Capsule().fill(LinearGradient(colors: [.clear] + colours + [.clear], startPoint: .leading, endPoint: .trailing))
                        .frame(width: 160).offset(x: x)
                }
                .clipShape(Capsule())
            }
        }
        .frame(height: 6)
    }
}

struct Pill: View {
    let text: String
    var colour: Color = .gold
    var body: some View {
        Text(text.uppercased()).font(.demi(10)).kerning(2).foregroundStyle(colour)
            .padding(.horizontal, 10).padding(.vertical, 4)
            .background(Capsule().fill(colour.opacity(0.12))).overlay(Capsule().stroke(colour.opacity(0.45)))
    }
}

// MARK: - Screens

struct InstallerView: View {
    @EnvironmentObject var installer: Installer

    private var busy: Bool { installer.phase == .working || installer.phase == .uninstalling }

    var body: some View {
        ZStack {
            Cosmos()
            HStack(spacing: 0) {
                BlackHole(energy: busy ? 4 : 1, size: 290)
                    .frame(width: 300)
                    .padding(.leading, 6)
                VStack(alignment: .leading, spacing: 0) {
                    Wordmark(subtitle: installer.uninstallMode || installer.phase == .uninstallAsk || installer.phase == .uninstalled
                             ? "UNINSTALL" : "WAVETABLE SPACE SYNTH").padding(.top, 36)
                    Group {
                        switch installer.phase {
                        case .welcome: welcome
                        case .working: working("INSTALLING", colour: .gold)
                        case .done: done
                        case .failed(let msg): failed(msg)
                        case .uninstallAsk: uninstallAsk
                        case .uninstalling: working("REMOVING", colour: .plasma)
                        case .uninstalled: uninstalled
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
        VStack(alignment: .leading, spacing: 7) {
            Pill(text: installer.headline).padding(.top, 14).padding(.bottom, 8)
            let colours: [Color] = [.ion, .violet, .aurora, .gold]
            ForEach(installer.parts.indices, id: \.self) { i in
                PartToggle(part: $installer.parts[i], colour: colours[i])
            }
            Spacer(minLength: 10)
            GlowButton(title: installer.installedVersion == nil ? "INSTALL" : "UPDATE") { installer.install() }
                .disabled(!installer.parts.contains { $0.on })
                .opacity(installer.parts.contains { $0.on } ? 1 : 0.4)
            HStack {
                Text("Asks for your Mac password once. Your saved sounds are kept.")
                    .font(.body(10.5)).foregroundStyle(Color.textDim).fixedSize(horizontal: false, vertical: true)
                Spacer()
                if installer.somethingInstalled {
                    Button("Uninstall…") { installer.phase = .uninstallAsk }
                        .buttonStyle(.plain).font(.demi(10.5)).foregroundStyle(Color.plasma.opacity(0.9))
                }
            }
            .padding(.bottom, 22)
        }
    }

    private func working(_ title: String, colour: Color) -> some View {
        VStack(alignment: .leading, spacing: 14) {
            Spacer()
            Text(title).font(.heavy(14)).kerning(3).foregroundStyle(colour)
            SweepBar(colours: colour == .plasma ? [.plasma, .violet] : [.gold, .plasma, .violet])
            Text(installer.status).font(.body(12.5)).foregroundStyle(Color.textDim)
            Spacer()
        }
    }

    private var done: some View {
        VStack(alignment: .leading, spacing: 11) {
            Text("READY FOR LIFTOFF").font(.heavy(15)).kerning(3).foregroundStyle(Color.gold).padding(.top, 20)
            VStack(alignment: .leading, spacing: 8) {
                step("1", "Open Ableton Live > Settings > Plug-Ins and click Rescan.")
                step("2", "Find it in Browser > Plug-Ins > Arrow > Hypernova.")
                step("3", "Click the sound name to browse 300+ sounds. The FOUNDERS PACK is in the list.")
                step("4", "Share sounds: Export in the browser. Friends drag the file onto Hypernova.")
            }
            if installer.abletonRunning {
                Text("Ableton is open: rescan plug-ins (or restart it) to load the new version.")
                    .font(.body(11)).foregroundStyle(Color.plasma)
            }
            Spacer(minLength: 8)
            HStack(spacing: 10) {
                if installer.abletonURL != nil {
                    GlowButton(title: "OPEN ABLETON") { installer.openAbleton(); NSApp.terminate(nil) }
                } else if installer.parts.first(where: { $0.id == "app" })?.on == true {
                    GlowButton(title: "OPEN HYPERNOVA") { installer.openHypernova(); NSApp.terminate(nil) }
                }
                GlowButton(title: "DONE", style: .secondary) { NSApp.terminate(nil) }
            }
            .padding(.bottom, 26)
        }
    }

    private var uninstallAsk: some View {
        VStack(alignment: .leading, spacing: 12) {
            Pill(text: installer.somethingInstalled ? "Installed on this Mac" : "Nothing installed", colour: .plasma).padding(.top, 14)
            Text("This removes the Hypernova VST3 and AU plug-ins, the standalone app and the installed sound packs (and any old Arrow Bass).")
                .font(.body(12.5)).foregroundStyle(Color.textMain).fixedSize(horizontal: false, vertical: true)
            PartToggle(part: Binding (get: { Part (id: "sounds", title: "Also delete my saved sounds",
                                                   detail: "Off keeps them, so they come back if you reinstall.", on: installer.alsoDeleteSounds) },
                                      set: { installer.alsoDeleteSounds = $0.on }), colour: .plasma)
            Spacer(minLength: 8)
            HStack(spacing: 10) {
                GlowButton(title: "UNINSTALL", style: .danger) { installer.uninstall() }
                    .disabled(!installer.somethingInstalled).opacity(installer.somethingInstalled ? 1 : 0.4)
                GlowButton(title: installer.uninstallMode ? "CLOSE" : "BACK", style: .secondary) {
                    if installer.uninstallMode { NSApp.terminate(nil) } else { installer.phase = .welcome }
                }
            }
            .padding(.bottom, 26)
        }
    }

    private var uninstalled: some View {
        VStack(alignment: .leading, spacing: 12) {
            Spacer()
            Text("HYPERNOVA REMOVED").font(.heavy(15)).kerning(3).foregroundStyle(Color.aurora)
            Text(installer.alsoDeleteSounds ? "Everything is gone, including your saved sounds."
                                            : "Your saved sounds are still in ~/Library/Application Support/Arrow/Hypernova.")
                .font(.body(12.5)).foregroundStyle(Color.textDim).fixedSize(horizontal: false, vertical: true)
            GlowButton(title: "DONE", style: .secondary) { NSApp.terminate(nil) }.frame(width: 180)
            Spacer()
        }
    }

    private func failed(_ msg: String) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Spacer()
            Text("THAT DIDN'T WORK").font(.heavy(15)).kerning(3).foregroundStyle(Color.plasma)
            Text(msg).font(.body(12)).foregroundStyle(Color.textDim).fixedSize(horizontal: false, vertical: true).lineLimit(6)
            Text("You can also open \"Everything else\" in the disk image and run Install Hypernova.pkg.")
                .font(.body(11)).foregroundStyle(Color.textDim)
            GlowButton(title: "TRY AGAIN") { installer.phase = installer.uninstallMode ? .uninstallAsk : .welcome }.frame(width: 180)
            Spacer()
        }
    }

    private func step(_ n: String, _ text: String) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Text(n).font(.heavy(11)).foregroundStyle(Color.bg0).frame(width: 20, height: 20)
                .background(Circle().fill(Color.gold))
            Text(text).font(.body(12.5)).foregroundStyle(Color.textMain).fixedSize(horizontal: false, vertical: true)
        }
    }
}

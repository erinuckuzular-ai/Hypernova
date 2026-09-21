import SwiftUI
import CoreText

// MARK: - Type: Space Grotesk (bold, from the variable font) and Space Mono, bundled in Resources.
// Falls back to Helvetica Neue Bold / SF Mono if the files are missing.

enum Brand {
    private static func url(_ name: String) -> URL? { Bundle.main.url(forResource: name, withExtension: "ttf") }

    static func registerFonts() {
        for name in ["SpaceGrotesk-var", "SpaceMono-Regular", "SpaceMono-Bold"] {
            if let u = url(name) { CTFontManagerRegisterFontsForURL(u as CFURL, .process, nil) }
        }
    }

    // The variable font's default instance is Light; pin the weight axis instead of faking bold.
    private static let groteskDescriptor: CTFontDescriptor? = {
        guard let u = url("SpaceGrotesk-var"), let data = try? Data(contentsOf: u) else { return nil }
        return CTFontManagerCreateFontDescriptorFromData(data as CFData)
    }()
    private static let wght: UInt32 = 0x7767_6874 // 'wght'

    static func grotesk(_ size: CGFloat, weight: CGFloat = 700) -> Font {
        if let d = groteskDescriptor {
            let v = CTFontDescriptorCreateCopyWithVariation(d, NSNumber(value: wght) as CFNumber, weight)
            return Font(CTFontCreateWithFontDescriptor(v, size, nil))
        }
        return Font(NSFont(name: weight >= 600 ? "HelveticaNeue-Bold" : "HelveticaNeue", size: size) ?? .systemFont(ofSize: size, weight: weight >= 600 ? .bold : .regular))
    }

    static func mono(_ size: CGFloat, bold: Bool = false) -> Font {
        let name = bold ? "SpaceMono-Bold" : "SpaceMono-Regular"
        if NSFont(name: name, size: size) != nil { return .custom(name, size: size) }
        return .system(size: size, weight: bold ? .bold : .regular, design: .monospaced)
    }
}

// MARK: - Pieces

// The orange dot from the wordmark, breathing slowly (a tiny view redrawn ~20 times a second).
struct Dot: View {
    var size: CGFloat = 10
    var busy = false
    var body: some View {
        TimelineView(.animation(minimumInterval: 1 / 20)) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            let p = 0.5 + 0.5 * sin(t * (busy ? 5.0 : 1.6))
            ZStack {
                Circle().fill(Color.accent.opacity(0.18 * (1 - p))).frame(width: size, height: size).scaleEffect(1.4 + 1.1 * p)
                Circle().fill(Color.accent).frame(width: size, height: size)
            }
        }
        .frame(width: size, height: size)
    }
}

struct Wordmark: View {
    var subtitle = "wavetable synthesizer"
    var busy = false
    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack(spacing: 8) {
                Dot(size: 10, busy: busy)
                Text("hypernova").font(Brand.grotesk(21)).kerning(-0.5).foregroundColor(.ink)
            }
            Text(subtitle).font(Brand.mono(10.5)).foregroundColor(.ink3).padding(.leading, 18)
        }
    }
}

// A white card: 1px rule border, soft two-layer shadow; lifts a little on hover.
struct Card<Content: View>: View {
    var padding: CGFloat = 18
    @ViewBuilder var content: Content
    @State private var hover = false
    var body: some View {
        content
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: 14).fill(Color.card)
                    .shadow(color: .black.opacity(hover ? 0.10 : 0.07), radius: hover ? 22 : 16, x: 0, y: hover ? 14 : 10)
                    .shadow(color: .black.opacity(0.05), radius: 1, x: 0, y: 1)
            )
            .overlay(RoundedRectangle(cornerRadius: 14).stroke(Color.rule, lineWidth: 1))
            .offset(y: hover ? -2 : 0)
            .animation(.easeOut(duration: 0.18), value: hover)
            .onHover { hover = $0 }
    }
}

// Primary: black pill, cream text, orange on hover, slightly raised. Secondary: outlined pill.
struct PillButtonStyle: ButtonStyle {
    var primary = true
    func makeBody(configuration: Configuration) -> some View { PillBody(configuration: configuration, primary: primary) }

    struct PillBody: View {
        let configuration: ButtonStyleConfiguration
        let primary: Bool
        @State private var hover = false
        @Environment(\.isEnabled) private var enabled

        var body: some View {
            let pressed = configuration.isPressed
            let lit = hover && enabled
            configuration.label
                .font(Brand.mono(13, bold: true))
                .foregroundColor(primary ? (lit ? .ink : .paper) : (lit ? .accent : .ink))
                .frame(maxWidth: .infinity, minHeight: 42)
                .background {
                    if primary {
                        Capsule().fill(lit ? Color.accent : Color.ink)
                            .overlay(Capsule().stroke(Color.white.opacity(0.22), lineWidth: 1).padding(0.5).mask(
                                LinearGradient(colors: [.white, .clear], startPoint: .top, endPoint: .center)))
                            .shadow(color: .black.opacity(pressed ? 0.12 : 0.22), radius: pressed ? 1 : 5, x: 0, y: pressed ? 1 : 3)
                    } else {
                        Capsule().fill(Color.card.opacity(lit ? 1 : 0.6))
                            .overlay(Capsule().stroke(lit ? Color.accent : Color.ink.opacity(0.85), lineWidth: 1))
                    }
                }
                .offset(y: primary && pressed ? 1 : 0)
                .opacity(enabled ? 1 : 0.35)
                .contentShape(Capsule())
                .onHover { hover = $0 }
                .animation(.easeOut(duration: 0.15), value: hover)
        }
    }
}

struct PillButton: View {
    let title: String
    var primary = true
    let action: () -> Void
    var body: some View { Button(title, action: action).buttonStyle(PillButtonStyle(primary: primary)) }
}

struct PartToggle: View {
    @Binding var part: Part
    @State private var hover = false

    var body: some View {
        Button { part.on.toggle() } label: {
            HStack(spacing: 12) {
                ZStack {
                    Circle().fill(part.on ? Color.accent : Color.card).frame(width: 18, height: 18)
                    Circle().stroke(part.on ? Color.accent : (hover ? Color.ink3 : Color.rule), lineWidth: 1.2).frame(width: 18, height: 18)
                    if part.on { Image(systemName: "checkmark").font(.system(size: 9, weight: .heavy)).foregroundColor(.white) }
                }
                VStack(alignment: .leading, spacing: 1) {
                    Text(part.title).font(Brand.grotesk(14)).foregroundColor(part.on ? .ink : .ink3)
                    Text(part.detail).font(Brand.mono(10.5)).foregroundColor(.ink3)
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 8)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { hover = $0 }
    }
}

struct Rule: View {
    var body: some View { Rectangle().fill(Color.rule).frame(height: 1) }
}

// Orange segment sweeping along a hairline track.
struct SweepBar: View {
    var body: some View {
        TimelineView(.animation(minimumInterval: 1 / 30)) { tl in
            GeometryReader { geo in
                let t = tl.date.timeIntervalSinceReferenceDate
                let w = geo.size.width, seg = w * 0.3
                let x = (t.truncatingRemainder(dividingBy: 1.6) / 1.6) * (w + seg) - seg
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.paper).overlay(Capsule().stroke(Color.rule, lineWidth: 1))
                    Capsule().fill(Color.accent).frame(width: seg).offset(x: x)
                }
                .clipShape(Capsule())
            }
        }
        .frame(height: 8)
    }
}

struct Tag: View {
    let text: String
    var accent = false
    var body: some View {
        HStack(spacing: 6) {
            if accent { Circle().fill(Color.accent).frame(width: 6, height: 6) }
            Text(text).font(Brand.mono(10.5)).foregroundColor(.ink2).lineLimit(1)
        }
        .padding(.horizontal, 11).padding(.vertical, 5)
        .background(Capsule().fill(Color.card))
        .overlay(Capsule().stroke(Color.rule, lineWidth: 1))
    }
}

// MARK: - Screens

struct InstallerView: View {
    @EnvironmentObject var installer: Installer

    private var busy: Bool { installer.phase == .working || installer.phase == .uninstalling }
    private var uninstalling: Bool {
        installer.uninstallMode || installer.phase == .uninstallAsk || installer.phase == .uninstalling || installer.phase == .uninstalled
    }

    var body: some View {
        ZStack(alignment: .topLeading) {
            Color.paper
            VStack(alignment: .leading, spacing: 0) {
                // The header sits below the window buttons (hidden title bar).
                HStack(alignment: .top) {
                    Wordmark(subtitle: uninstalling ? "uninstall" : "wavetable synthesizer", busy: busy)
                    Spacer()
                    if let tag = tag { Tag(text: tag.0, accent: tag.1) }
                }
                .padding(.top, 44)
                Group {
                    switch installer.phase {
                    case .welcome: welcome
                    case .working: working("installing.")
                    case .done: done
                    case .failed(let msg): failed(msg)
                    case .uninstallAsk: uninstallAsk
                    case .uninstalling: working("removing.")
                    case .uninstalled: uninstalled
                    }
                }
                .padding(.top, 28)
                .transition(.opacity)
            }
            .padding(.horizontal, 40)
            .padding(.bottom, 34)
            .animation(.easeInOut(duration: 0.25), value: installer.phase)
        }
    }

    private var tag: (String, Bool)? {
        switch installer.phase {
        case .welcome: return (installer.headline, true)
        case .uninstallAsk: return (installer.somethingInstalled ? "installed on this mac" : "nothing installed", installer.somethingInstalled)
        default: return installer.version.isEmpty ? nil : ("version \(installer.version)", false)
        }
    }

    // Left: the big dry headline and a line of copy. Right: the card, and the buttons under it.
    private func layout<C: View, B: View>(_ title: String, _ copy: String, note: String? = nil, noteAccent: Bool = false,
                                          @ViewBuilder card: () -> C, @ViewBuilder buttons: () -> B) -> some View {
        HStack(alignment: .top, spacing: 34) {
            VStack(alignment: .leading, spacing: 12) {
                Text(title).font(Brand.grotesk(38)).kerning(-1.6).foregroundColor(.ink)
                    .lineLimit(2).fixedSize(horizontal: false, vertical: true)
                Text(copy).font(Brand.grotesk(14, weight: 400)).foregroundColor(.ink2).lineSpacing(3)
                    .fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 0)
                if let note {
                    Text(note).font(Brand.mono(10.5)).foregroundColor(noteAccent ? .accent : .ink3).lineSpacing(2)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .frame(width: 250, alignment: .leading)
            VStack(alignment: .leading, spacing: 16) {
                card()
                Spacer(minLength: 0)
                buttons()
            }
        }
    }

    private var welcome: some View {
        layout("install hypernova.", "the plug-ins, the app and the founders pack. keep what you want.",
               note: "asks for your mac password once.\nyour saved sounds are kept.") {
            Card(padding: 14) {
                VStack(spacing: 0) {
                    ForEach(installer.parts.indices, id: \.self) { i in
                        if i > 0 { Rule() }
                        PartToggle(part: $installer.parts[i])
                    }
                }
                .padding(.horizontal, 4)
            }
        } buttons: {
            HStack(spacing: 10) {
                PillButton(title: installer.installedVersion == nil ? "install" : "update") { installer.install() }
                    .disabled(!installer.parts.contains { $0.on })
                if installer.somethingInstalled {
                    PillButton(title: "uninstall…", primary: false) { installer.phase = .uninstallAsk }.frame(width: 130)
                }
            }
        }
    }

    private func working(_ title: String) -> some View {
        layout(title, "hang on.", note: "leave this window open.") {
            Card(padding: 22) {
                VStack(alignment: .leading, spacing: 14) {
                    SweepBar()
                    Text(installer.status + "…").font(Brand.mono(11.5)).foregroundColor(.ink2)
                }
            }
        } buttons: { EmptyView() }
    }

    private var done: some View {
        layout("done.", "it's in /Library/Audio/Plug-Ins. rescan in ableton.",
               note: installer.abletonRunning ? "ableton is open. rescan plug-ins (or restart it) to load the new version." : nil,
               noteAccent: true) {
            Card(padding: 18) {
                VStack(alignment: .leading, spacing: 10) {
                    step("01", "open ableton live › settings › plug-ins and click rescan.")
                    step("02", "find it in browser › plug-ins › arrow › hypernova.")
                    step("03", "click the sound name to browse 300+ sounds. the founders pack is in the list.")
                    step("04", "share sounds: export in the browser. friends drag the file onto hypernova.")
                }
            }
        } buttons: {
            HStack(spacing: 10) {
                if installer.abletonURL != nil {
                    PillButton(title: "open ableton") { installer.openAbleton(); NSApp.terminate(nil) }
                } else if installer.parts.first(where: { $0.id == "app" })?.on == true {
                    PillButton(title: "open hypernova") { installer.openHypernova(); NSApp.terminate(nil) }
                }
                PillButton(title: "done", primary: false) { NSApp.terminate(nil) }
            }
        }
    }

    private var uninstallAsk: some View {
        layout("uninstall hypernova.",
               "removes the vst3 and au plug-ins, the standalone app and the installed sound packs. and any old arrow bass.") {
            Card(padding: 14) {
                PartToggle(part: Binding(get: { Part(id: "sounds", title: "also delete my saved sounds",
                                                     detail: "off keeps them for a reinstall.", on: installer.alsoDeleteSounds) },
                                         set: { installer.alsoDeleteSounds = $0.on }))
                    .padding(.horizontal, 4)
            }
        } buttons: {
            HStack(spacing: 10) {
                PillButton(title: "uninstall") { installer.uninstall() }
                    .disabled(!installer.somethingInstalled)
                PillButton(title: installer.uninstallMode ? "close" : "back", primary: false) {
                    if installer.uninstallMode { NSApp.terminate(nil) } else { installer.phase = .welcome }
                }
            }
        }
    }

    private var uninstalled: some View {
        layout("gone.", installer.alsoDeleteSounds ? "everything, including your saved sounds." : "hypernova is off this mac. your saved sounds stayed.") {
            if !installer.alsoDeleteSounds {
                Card(padding: 18) {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("your sounds are still in").font(Brand.mono(10.5)).foregroundColor(.ink3)
                        Text("~/Library/Application Support/Arrow/Hypernova").font(Brand.mono(11.5)).foregroundColor(.ink)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
        } buttons: {
            PillButton(title: "done") { NSApp.terminate(nil) }
        }
    }

    private func failed(_ msg: String) -> some View {
        layout("that didn't work.", "something got in the way. try again, or use the package.",
               note: "open everything else in the disk image and run install hypernova.pkg.") {
            Card(padding: 18) {
                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 6) {
                        Circle().fill(Color.accent).frame(width: 6, height: 6)
                        Text("macOS said").font(Brand.mono(10.5)).foregroundColor(.ink3)
                    }
                    Text(msg).font(Brand.mono(11)).foregroundColor(.ink).lineSpacing(2).lineLimit(7)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        } buttons: {
            PillButton(title: "try again") { installer.phase = installer.uninstallMode ? .uninstallAsk : .welcome }
        }
    }

    private func step(_ n: String, _ text: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(n).font(Brand.mono(10.5, bold: true)).foregroundColor(.accent)
            Text(text).font(Brand.grotesk(13, weight: 400)).foregroundColor(.ink).lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}

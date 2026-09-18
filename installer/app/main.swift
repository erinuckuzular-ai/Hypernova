// Install Hypernova: the branded installer app.
//
// It carries the notarized Hypernova.pkg and runs it with the macOS `installer` tool (one password prompt,
// because plug-ins live in /Library). Component choices map onto the package's choices, so an update
// replaces the old version exactly like the package would. Built by scripts/build-installer-app.sh.

import AppKit
import SwiftUI

// MARK: - Palette (Source/UI/Style.h)

extension Color {
    static let bg0 = Color(red: 0.024, green: 0.031, blue: 0.051)
    static let bg1 = Color(red: 0.043, green: 0.059, blue: 0.090)
    static let panel = Color(red: 0.067, green: 0.090, blue: 0.141)
    static let line = Color.white.opacity(0.08)
    static let textMain = Color(red: 0.914, green: 0.933, blue: 0.973)
    static let textDim = Color(red: 0.506, green: 0.565, blue: 0.671)
    static let cyan = Color(red: 0.184, green: 0.953, blue: 0.878)
    static let violet = Color(red: 0.545, green: 0.424, blue: 1.0)
    static let pink = Color(red: 1.0, green: 0.361, blue: 0.541)
    static let mint = Color(red: 0.302, green: 1.0, blue: 0.690)
}

extension Font {
    static func heavy(_ s: CGFloat) -> Font { .custom("AvenirNext-Heavy", size: s) }
    static func demi(_ s: CGFloat) -> Font { .custom("AvenirNext-DemiBold", size: s) }
    static func body(_ s: CGFloat) -> Font { .custom("AvenirNext-Medium", size: s) }
}

// MARK: - Install logic

enum Phase: Equatable { case welcome, working, done, failed(String) }

struct Part: Identifiable {
    let id: String          // choice id in packaging/distribution.xml
    let title: String
    let detail: String
    var on = true
}

@MainActor final class Installer: ObservableObject {
    @Published var phase: Phase = .welcome
    @Published var status = ""
    @Published var parts: [Part] = [
        Part(id: "vst3", title: "VST3 plug-in", detail: "Ableton Live, FL Studio and most DAWs"),
        Part(id: "au", title: "Audio Unit", detail: "Ableton Live, Logic Pro, GarageBand"),
        Part(id: "app", title: "Standalone app", detail: "play Hypernova without a DAW"),
        Part(id: "pack", title: "FOUNDERS PACK", detail: "22 exclusive sounds, free"),
    ]

    let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""

    var installedVersion: String? {
        for path in ["/Library/Audio/Plug-Ins/VST3/Hypernova.vst3", "/Library/Audio/Plug-Ins/Components/Hypernova.component",
                     "/Applications/Hypernova.app"] {
            if let b = Bundle(path: path), let v = b.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String { return v }
        }
        return nil
    }

    var hasArrowBass: Bool {
        let fm = FileManager.default, home = fm.homeDirectoryForCurrentUser.path
        return ["/Library/Audio/Plug-Ins/VST3/Arrow Bass.vst3", "/Library/Audio/Plug-Ins/Components/Arrow Bass.component",
                "/Applications/Arrow Bass.app", home + "/Library/Audio/Plug-Ins/VST3/Arrow Bass.vst3",
                home + "/Library/Audio/Plug-Ins/Components/Arrow Bass.component"].contains { fm.fileExists(atPath: $0) }
    }

    var abletonURL: URL? {
        if let u = NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.ableton.live") { return u }
        let apps = (try? FileManager.default.contentsOfDirectory(atPath: "/Applications")) ?? []
        return apps.filter { $0.hasPrefix("Ableton Live") && $0.hasSuffix(".app") }.sorted().last
            .map { URL(fileURLWithPath: "/Applications/" + $0) }
    }

    var abletonRunning: Bool {
        NSWorkspace.shared.runningApplications.contains { ($0.bundleIdentifier ?? "").hasPrefix("com.ableton.live") }
    }

    var headline: String {
        if let v = installedVersion { return v == version ? "Reinstall \(version)" : "Update \(v) → \(version)" }
        return hasArrowBass ? "Replaces Arrow Bass" : "Version \(version)"
    }

    func install() {
        guard parts.contains(where: { $0.on }) else { return }
        phase = .working
        status = "Waiting for your password"
        let choices = parts.map { ($0.id, $0.on) }
        Task.detached(priority: .userInitiated) {
            let result = Self.runPackage(choices: choices)
            await MainActor.run {
                switch result {
                case .success:
                    self.cleanUpUserCopies()
                    self.phase = .done
                case .failure(let e):
                    self.phase = e.cancelled ? .welcome : .failed(e.message)
                }
            }
        }
        // Status lines while the package runs.
        Task { @MainActor in
            let lines = ["Installing the plug-ins", "Placing the standalone app", "Unpacking the FOUNDERS PACK", "Telling your DAWs"]
            var i = 0
            try? await Task.sleep(for: .seconds(2.5))
            while self.phase == .working {
                self.status = lines[i % lines.count]
                i += 1
                try? await Task.sleep(for: .seconds(1.4))
            }
        }
    }

    struct InstallError: Error { let message: String; let cancelled: Bool }

    nonisolated static func runPackage(choices: [(String, Bool)]) -> Result<Void, InstallError> {
        guard let pkg = Bundle.main.url(forResource: "Hypernova", withExtension: "pkg") else {
            return .failure(.init(message: "The installer is missing its package. Download the DMG again.", cancelled: false))
        }
        // Choice changes for `installer -applyChoiceChangesXML`.
        let xml = FileManager.default.temporaryDirectory.appending(path: "hypernova-choices.plist")
        let entries: [[String: Any]] = choices.map { ["choiceIdentifier": $0.0, "choiceAttribute": "selected", "attributeSetting": $0.1 ? 1 : 0] }
        do {
            let data = try PropertyListSerialization.data(fromPropertyList: entries, format: .xml, options: 0)
            try data.write(to: xml)
        } catch {
            return .failure(.init(message: "Couldn't prepare the install: \(error.localizedDescription)", cancelled: false))
        }

        func applescriptString(_ s: String) -> String {
            "\"" + s.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"") + "\""
        }
        let script = """
        do shell script "/usr/sbin/installer -pkg " & quoted form of \(applescriptString(pkg.path)) & " -target / -applyChoiceChangesXML " & quoted form of \(applescriptString(xml.path)) with administrator privileges with prompt "Hypernova needs your password to install its plug-ins."
        """
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/usr/bin/osascript")
        p.arguments = ["-e", script]
        let err = Pipe()
        p.standardError = err
        p.standardOutput = Pipe()
        do { try p.run() } catch {
            return .failure(.init(message: error.localizedDescription, cancelled: false))
        }
        p.waitUntilExit()
        try? FileManager.default.removeItem(at: xml)
        if p.terminationStatus == 0 { return .success(()) }
        let text = String(data: err.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        if text.contains("-128") { return .failure(.init(message: "", cancelled: true)) }
        return .failure(.init(message: text.isEmpty ? "The macOS installer stopped (\(p.terminationStatus))." : text, cancelled: false))
    }

    // Per-user copies (old dev builds, Arrow Bass) would show up twice next to the system-wide install.
    private func cleanUpUserCopies() {
        let fm = FileManager.default, home = fm.homeDirectoryForCurrentUser
        for rel in ["Library/Audio/Plug-Ins/VST3/Hypernova.vst3", "Library/Audio/Plug-Ins/Components/Hypernova.component",
                    "Library/Audio/Plug-Ins/VST3/Arrow Bass.vst3", "Library/Audio/Plug-Ins/Components/Arrow Bass.component"] {
            try? fm.removeItem(at: home.appending(path: rel))
        }
    }

    func openAbleton() {
        if let u = abletonURL { NSWorkspace.shared.openApplication(at: u, configuration: .init()) }
    }

    func openHypernova() {
        NSWorkspace.shared.openApplication(at: URL(fileURLWithPath: "/Applications/Hypernova.app"), configuration: .init())
    }
}

// MARK: - App

@main struct InstallHypernova: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    @StateObject private var installer = Installer()

    // `--snapshot <dir>` renders every screen to PNG and quits (used to check the design).
    init() {
        let args = CommandLine.arguments
        guard let i = args.firstIndex(of: "--snapshot"), i + 1 < args.count else { return }
        let dir = URL(fileURLWithPath: args[i + 1])
        let screens: [(String, Phase)] = [("welcome", .welcome), ("working", .working), ("done", .done), ("failed", .failed("Sample error"))]
        for (name, phase) in screens {
            let model = Installer()
            model.phase = phase
            model.status = "Installing the plug-ins"
            let view = InstallerView().environmentObject(model).frame(width: 680, height: 460).preferredColorScheme(.dark)
            let renderer = ImageRenderer(content: view)
            renderer.scale = 2
            if let img = renderer.nsImage, let tiff = img.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
               let png = rep.representation(using: .png, properties: [:]) {
                try? png.write(to: dir.appending(path: "installer-\(name).png"))
            }
        }
        exit(0)
    }

    var body: some Scene {
        Window("Install Hypernova", id: "main") {
            InstallerView()
                .environmentObject(installer)
                .frame(width: 680, height: 460)
                .background(Color.bg0)
                .preferredColorScheme(.dark)
        }
        .windowStyle(.hiddenTitleBar)
        .windowResizability(.contentSize)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.activate(ignoringOtherApps: true)
        NSApp.windows.first?.isMovableByWindowBackground = true
    }
}

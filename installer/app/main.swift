// Install Hypernova: the branded installer (and uninstaller).
//
// Installing runs the bundled, notarized Hypernova.pkg with the macOS `installer` tool (one password prompt,
// because plug-ins live in /Library); component choices map onto the package's choices, so an update replaces
// the old version exactly like the package would. Uninstalling removes the same files (and, if asked, the
// user's own sounds). The same app ships twice in the disk image: "Install Hypernova" and "Uninstall
// Hypernova" (Info.plist key HNMode = uninstall). Built by scripts/build-installer-app.sh.

import AppKit
import SwiftUI

// MARK: - Palette (the Paper house style: docs/style.css, the plug-in's Paper theme)

extension Color {
    init(hex: UInt32) { self.init(red: Double((hex >> 16) & 0xff) / 255, green: Double((hex >> 8) & 0xff) / 255, blue: Double(hex & 0xff) / 255) }
    static let paper = Color(hex: 0xf2f1ee)   // background
    static let card = Color(hex: 0xffffff)
    static let rule = Color(hex: 0xd9d8d3)    // 1px borders
    static let ink = Color(hex: 0x111111)
    static let ink2 = Color(hex: 0x4a4a4a)    // secondary text
    static let ink3 = Color(hex: 0x8b8b88)    // faint
    static let accent = Color(hex: 0xff5a1f)  // the one orange
}

// MARK: - Model

enum Phase: Equatable { case welcome, working, done, failed(String), uninstallAsk, uninstalling, uninstalled }

struct Part: Identifiable {
    let id: String          // choice id in packaging/distribution.xml
    let title: String
    let detail: String
    var on = true
}

@MainActor final class Installer: ObservableObject {
    @Published var phase: Phase = .welcome
    @Published var status = ""
    @Published var alsoDeleteSounds = false
    @Published var parts: [Part] = [
        Part(id: "vst3", title: "vst3 plug-in", detail: "ableton live, fl studio and most daws"),
        Part(id: "au", title: "audio unit", detail: "ableton live, logic pro, garageband"),
        Part(id: "app", title: "standalone app", detail: "play hypernova without a daw"),
        Part(id: "pack", title: "founders pack", detail: "22 exclusive sounds. free."),
    ]

    let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
    let uninstallMode = (Bundle.main.object(forInfoDictionaryKey: "HNMode") as? String) == "uninstall"

    init() { if uninstallMode { phase = .uninstallAsk } }

    nonisolated static let systemItems = [
        "/Library/Audio/Plug-Ins/VST3/Hypernova.vst3", "/Library/Audio/Plug-Ins/Components/Hypernova.component",
        "/Applications/Hypernova.app", "/Library/Application Support/Arrow/Hypernova",
        "/Library/Audio/Plug-Ins/VST3/Arrow Bass.vst3", "/Library/Audio/Plug-Ins/Components/Arrow Bass.component", "/Applications/Arrow Bass.app",
    ]
    nonisolated static let userItems = [
        "Library/Audio/Plug-Ins/VST3/Hypernova.vst3", "Library/Audio/Plug-Ins/Components/Hypernova.component",
        "Library/Audio/Plug-Ins/VST3/Arrow Bass.vst3", "Library/Audio/Plug-Ins/Components/Arrow Bass.component",
    ]
    nonisolated static var soundsFolder: URL { FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/Arrow/Hypernova") }

    var installedVersion: String? {
        for path in Self.systemItems.prefix(3) {
            if let b = Bundle(path: path), let v = b.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String { return v }
        }
        return nil
    }

    var somethingInstalled: Bool {
        let fm = FileManager.default, home = fm.homeDirectoryForCurrentUser
        return Self.systemItems.contains { fm.fileExists(atPath: $0) } || Self.userItems.contains { fm.fileExists(atPath: home.appendingPathComponent($0).path) }
    }

    var hasArrowBass: Bool {
        let fm = FileManager.default
        return Self.systemItems.filter { $0.contains("Arrow Bass") }.contains { fm.fileExists(atPath: $0) }
    }

    var abletonURL: URL? {
        if let u = NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.ableton.live") { return u }
        let apps = (try? FileManager.default.contentsOfDirectory(atPath: "/Applications")) ?? []
        return apps.filter { $0.hasPrefix("Ableton Live") && $0.hasSuffix(".app") }.sorted().last.map { URL(fileURLWithPath: "/Applications/" + $0) }
    }

    var abletonRunning: Bool {
        NSWorkspace.shared.runningApplications.contains { ($0.bundleIdentifier ?? "").hasPrefix("com.ableton.live") }
    }

    var headline: String {
        if let v = installedVersion { return v == version ? "reinstall \(version)" : "update \(v) → \(version)" }
        return hasArrowBass ? "replaces arrow bass" : "version \(version)"
    }

    // MARK: Install

    func install() {
        guard parts.contains(where: { $0.on }) else { return }
        phase = .working
        status = "waiting for your password"
        let choices = parts.map { ($0.id, $0.on) }
        Task.detached(priority: .userInitiated) {
            let result = Self.runPackage(choices: choices)
            await MainActor.run {
                switch result {
                case .success: self.removeUserCopies(); self.phase = .done
                case .failure(let e): self.phase = e.cancelled ? .welcome : .failed(e.message)
                }
            }
        }
        cycleStatus(["installing the plug-ins", "placing the standalone app", "unpacking the founders pack", "telling your daws"], while: .working)
    }

    struct InstallError: Error { let message: String; let cancelled: Bool }

    nonisolated static func runAsAdmin(_ shellCommand: String, prompt: String) -> Result<Void, InstallError> {
        func q(_ s: String) -> String { "\"" + s.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"") + "\"" }
        let script = "do shell script \(q(shellCommand)) with administrator privileges with prompt \(q(prompt))"
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/usr/bin/osascript")
        p.arguments = ["-e", script]
        let err = Pipe()
        p.standardError = err
        p.standardOutput = Pipe()
        do { try p.run() } catch { return .failure(.init(message: error.localizedDescription, cancelled: false)) }
        p.waitUntilExit()
        if p.terminationStatus == 0 { return .success(()) }
        let text = String(data: err.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        if text.contains("-128") { return .failure(.init(message: "", cancelled: true)) }
        return .failure(.init(message: text.isEmpty ? "macOS stopped the operation (\(p.terminationStatus))." : text, cancelled: false))
    }

    nonisolated static func shellQuote(_ s: String) -> String { "'" + s.replacingOccurrences(of: "'", with: "'\\''") + "'" }

    nonisolated static func runPackage(choices: [(String, Bool)]) -> Result<Void, InstallError> {
        guard let pkg = Bundle.main.url(forResource: "Hypernova", withExtension: "pkg") else {
            return .failure(.init(message: "the installer is missing its package. download the dmg again.", cancelled: false))
        }
        let xml = FileManager.default.temporaryDirectory.appendingPathComponent("hypernova-choices.plist")
        let entries: [[String: Any]] = choices.map { ["choiceIdentifier": $0.0, "choiceAttribute": "selected", "attributeSetting": $0.1 ? 1 : 0] }
        do {
            let data = try PropertyListSerialization.data(fromPropertyList: entries, format: .xml, options: 0)
            try data.write(to: xml)
        } catch {
            return .failure(.init(message: "couldn't prepare the install: \(error.localizedDescription)", cancelled: false))
        }
        defer { try? FileManager.default.removeItem(at: xml) }
        return runAsAdmin("/usr/sbin/installer -pkg \(shellQuote(pkg.path)) -target / -applyChoiceChangesXML \(shellQuote(xml.path))",
                          prompt: "hypernova needs your password to install its plug-ins.")
    }

    // Per-user copies (old dev builds, Arrow Bass) would show up twice next to the system-wide install.
    private func removeUserCopies() {
        let fm = FileManager.default, home = fm.homeDirectoryForCurrentUser
        for rel in Self.userItems { try? fm.removeItem(at: home.appendingPathComponent(rel)) }
    }

    // MARK: Uninstall

    func uninstall() {
        phase = .uninstalling
        status = "waiting for your password"
        let deleteSounds = alsoDeleteSounds
        Task.detached(priority: .userInitiated) {
            let rm = Self.systemItems.map { "rm -rf " + Self.shellQuote($0) }.joined(separator: "; ")
            let forget = ["vst3", "au", "app", "pack"].flatMap { ["com.arrow.hypernova.\($0)", "com.arrow.arrowbass.\($0)"] }
                .map { "pkgutil --forget \($0) >/dev/null 2>&1" }.joined(separator: "; ")
            let result = Self.runAsAdmin("\(rm); \(forget); killall -9 AudioComponentRegistrar >/dev/null 2>&1; exit 0",
                                         prompt: "hypernova needs your password to remove its plug-ins.")
            await MainActor.run {
                switch result {
                case .success:
                    self.removeUserCopies()
                    if deleteSounds { try? FileManager.default.removeItem(at: Self.soundsFolder) }
                    self.phase = .uninstalled
                case .failure(let e):
                    self.phase = e.cancelled ? .uninstallAsk : .failed(e.message)
                }
            }
        }
        cycleStatus(["removing the plug-ins", "removing the app and packs", "tidying up"], while: .uninstalling)
    }

    private func cycleStatus(_ lines: [String], while phase: Phase) {
        Task { @MainActor in
            var i = 0
            try? await Task.sleep(nanoseconds: 2_500_000_000)
            while self.phase == phase {
                self.status = lines[i % lines.count]
                i += 1
                try? await Task.sleep(nanoseconds: 1_400_000_000)
            }
        }
    }

    func openAbleton() { if let u = abletonURL { NSWorkspace.shared.openApplication(at: u, configuration: .init()) } }
    func openHypernova() { NSWorkspace.shared.openApplication(at: URL(fileURLWithPath: "/Applications/Hypernova.app"), configuration: .init()) }
}

// MARK: - App

@main struct InstallHypernova: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    @StateObject private var installer = Installer()

    // `--snapshot <dir>` renders every screen to PNG and quits (used to check the design).
    init() {
        Brand.registerFonts()
        let args = CommandLine.arguments
        guard let i = args.firstIndex(of: "--snapshot"), i + 1 < args.count else { return }
        guard #available(macOS 13.0, *) else { exit(1) } // snapshots use ImageRenderer; the installer itself runs on macOS 12
        let dir = URL(fileURLWithPath: args[i + 1])
        let screens: [(String, Phase)] = [("welcome", .welcome), ("working", .working), ("done", .done), ("failed", .failed("installer: The install failed. (The Installer encountered an error that caused the installation to fail. Contact the software manufacturer for assistance.)")),
                                          ("uninstall", .uninstallAsk), ("uninstalled", .uninstalled)]
        for (name, phase) in screens {
            let model = Installer()
            model.phase = phase
            model.status = "installing the plug-ins"
            let view = InstallerView().environmentObject(model).frame(width: 720, height: 480).background(Color.paper).preferredColorScheme(.light)
            let renderer = ImageRenderer(content: view)
            renderer.scale = 2
            if let img = renderer.nsImage, let tiff = img.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
               let png = rep.representation(using: .png, properties: [:]) {
                try? png.write(to: dir.appendingPathComponent("installer-\(name).png"))
            }
        }
        exit(0)
    }

    var body: some Scene {
        // WindowGroup rather than Window (macOS 13+) so the installer opens on macOS 12 too; File > New is removed
        // and the delegate makes the window fixed-size.
        WindowGroup(installer.uninstallMode ? "Uninstall Hypernova" : "Install Hypernova") {
            InstallerView()
                .environmentObject(installer)
                .frame(width: 720, height: 480)
                .background(Color.paper)
                .preferredColorScheme(.light)
        }
        .windowStyle(.hiddenTitleBar)
        .commands { CommandGroup(replacing: .newItem) {} }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.activate(ignoringOtherApps: true)
        DispatchQueue.main.async {
            for w in NSApp.windows {
                w.isMovableByWindowBackground = true
                w.styleMask.remove(.resizable)
                w.standardWindowButton(.zoomButton)?.isEnabled = false
            }
        }
    }
}

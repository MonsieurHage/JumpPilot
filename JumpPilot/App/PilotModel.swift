import Foundation
import SwiftUI
import UIKit

/// All presentation and input state belongs to the main thread.
/// The C transport copies callback payloads before they leave its network thread.
final class PilotModel: ObservableObject {
    enum Link: Int { case offline = 0, connecting = 1, connected = 2, failed = 3 }

    @Published private(set) var link: Link = .offline
    @Published private(set) var status = "Prêt à se connecter"
    @Published private(set) var battery: Int?
    @Published private(set) var image: UIImage?
    @Published private(set) var videoDate: Date?
    @Published private(set) var armed = false
    @Published private(set) var isTouching = false
    @Published private(set) var stick = CGPoint.zero
    @Published private(set) var alert = 0
    @Published private(set) var jumpAvailableAt = Date.distantPast
    @Published var speedLimit = 35.0
    @Published var address = UserDefaults.standard.string(forKey: "drone.address") ?? "192.168.2.1"

    private var client: OpaquePointer?
    private var timer: Timer?
    private var generation: UInt = 0
    private var clientContext: UnsafeMutableRawPointer?

    private final class SessionContext {
        weak var model: PilotModel?
        let generation: UInt
        let videoLock = NSLock()
        var videoPending = false
        init(model: PilotModel, generation: UInt) {
            self.model = model; self.generation = generation
        }
    }

    init() { }

    private func createClient() -> OpaquePointer? {
        if let client { return client }
        let context = Unmanaged.passRetained(SessionContext(model: self, generation: generation)).toOpaque()
        clientContext = context
        client = jp_client_create({ context, event, value, bytes, length in
            guard let context else { return }
            let session = Unmanaged<SessionContext>.fromOpaque(context).takeUnretainedValue()
            guard let model = session.model else { return }
            let sessionID = session.generation
            if event == 3 {
                // Keep at most one image awaiting display: MJPEG should not form a stale queue.
                session.videoLock.lock()
                if session.videoPending { session.videoLock.unlock(); return }
                session.videoPending = true
                session.videoLock.unlock()
                let data = bytes.map { Data(bytes: $0, count: length) } ?? Data()
                let decoded = UIImage(data: data)
                DispatchQueue.main.async { [weak model] in
                    guard let model else { return }
                    if model.generation == sessionID, model.link == .connected, let decoded {
                        model.image = decoded
                        model.videoDate = Date()
                    }
                    session.videoLock.lock()
                    session.videoPending = false
                    session.videoLock.unlock()
                }
            } else {
                let text: String
                if let bytes, length > 0 {
                    text = String(decoding: UnsafeBufferPointer(start: bytes, count: length), as: UTF8.self)
                } else { text = "" }
                DispatchQueue.main.async { [weak model] in
                    guard let model, model.generation == sessionID else { return }
                    model.receive(event: event, value: value, text: text)
                }
            }
        }, context)
        if client == nil {
            Unmanaged<SessionContext>.fromOpaque(context).release()
            clientContext = nil
        }
        return client
    }

    func connect() {
        dispatchPrecondition(condition: .onQueue(.main))
        disconnect()
        address = address.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let client = createClient() else {
            link = .failed; status = "Impossible d’initialiser la connexion."; return
        }
        battery = nil; image = nil; videoDate = nil; alert = 0
        jumpAvailableAt = .distantPast
        link = .connecting
        status = "Connexion au drone…"
        let result = address.withCString { jp_client_connect(client, $0, 44444) }
        if result != 0 {
            link = .failed; status = "Adresse IPv4 invalide ou connexion indisponible."; return
        }
        UserDefaults.standard.set(address, forKey: "drone.address")
        timer = Timer.scheduledTimer(withTimeInterval: 0.05, repeats: true) { [weak self] _ in
            self?.refreshInput()
        }
        if let timer { RunLoop.main.add(timer, forMode: .common) }
    }

    func disconnect() {
        dispatchPrecondition(condition: .onQueue(.main))
        stop()
        timer?.invalidate(); timer = nil
        if let client { jp_client_destroy(client); self.client = nil }
        if let clientContext {
            Unmanaged<SessionContext>.fromOpaque(clientContext).release()
            self.clientContext = nil
        }
        generation &+= 1
        link = .offline; status = "Déconnecté"
        image = nil; videoDate = nil; battery = nil; alert = 0
        UIApplication.shared.isIdleTimerDisabled = false
    }

    func activate() {
        guard link == .connected, alert != 1, let client else { return }
        releaseStick()
        jp_client_arm(client, 1)
        armed = true
        status = "Commandes actives"
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
    }

    func stop() {
        armed = false; isTouching = false; stick = .zero
        if let client { jp_client_halt(client) }
        if link == .connected { status = "Arrêt · réactivez les commandes pour repartir" }
    }

    func moveStick(x: CGFloat, y: CGFloat) {
        guard armed, link == .connected else { return }
        let distance = max(1, hypot(x, y))
        stick = CGPoint(x: x / distance, y: y / distance)
        isTouching = true
        refreshInput()
    }

    func releaseStick() {
        isTouching = false; stick = .zero
        if let client { jp_client_drive(client, 0, 0) }
    }

    private func refreshInput() {
        guard armed, link == .connected, let client else { return }
        let x = abs(stick.x) < 0.09 ? 0 : Double(stick.x)
        let y = abs(stick.y) < 0.09 ? 0 : Double(stick.y)
        let speed = isTouching ? Int32((-y * speedLimit).rounded()) : 0
        let turn = isTouching ? Int32((x * min(speedLimit, 65)).rounded()) : 0
        jp_client_drive(client, speed, turn)
    }

    func jump(high: Bool) {
        guard armed, link == .connected, Date() >= jumpAvailableAt, let client else { return }
        releaseStick()
        if jp_client_jump(client, high ? 1 : 0) == 0 {
            jumpAvailableAt = Date().addingTimeInterval(3)
            status = high ? "Saut en hauteur demandé" : "Saut en longueur demandé"
            UIImpactFeedbackGenerator(style: .medium).impactOccurred()
        } else { status = "Saut indisponible. Attendez quelques secondes." }
    }

    private func receive(event: Int32, value: Int32, text: String) {
        switch event {
        case 1:
            guard let newLink = Link(rawValue: Int(value)) else { return }
            link = newLink; status = text
            if newLink != .connected {
                armed = false; releaseStick(); image = nil; videoDate = nil
            }
            if newLink == .failed || newLink == .offline {
                timer?.invalidate(); timer = nil
            }
            UIApplication.shared.isIdleTimerDisabled = newLink == .connected
        case 2: battery = Int(value)
        case 4: status = text
        case 5:
            alert = Int(value)
            if value == 1 { stop(); status = "Batterie critique · commandes désactivées" }
            else if value == 2 { status = "Batterie faible" }
        default: break
        }
    }

    deinit {
        timer?.invalidate()
        if let client { jp_client_destroy(client) }
        if let clientContext { Unmanaged<SessionContext>.fromOpaque(clientContext).release() }
    }
}

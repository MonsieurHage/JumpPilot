import SwiftUI

@main
struct JumpPilotApp: App {
    @StateObject private var pilot = PilotModel()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            CockpitView(pilot: pilot)
                .preferredColorScheme(.dark)
                .onChange(of: scenePhase) { phase in
                    if phase == .inactive { pilot.stop() }
                    if phase == .background { pilot.disconnect() }
                }
        }
    }
}

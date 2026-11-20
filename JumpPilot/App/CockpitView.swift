import SwiftUI

private let mint = Color(red: 0.48, green: 0.96, blue: 0.72)
private let panel = Color(red: 0.075, green: 0.09, blue: 0.12)

struct CockpitView: View {
    @ObservedObject var pilot: PilotModel
    @State private var showConnection = false
    @State private var showHelp = false
    @GestureState private var dragging = false

    var body: some View {
        GeometryReader { geometry in
            let landscape = geometry.size.width > geometry.size.height
            ZStack {
                Color(red: 0.025, green: 0.035, blue: 0.05).ignoresSafeArea()
                VStack(spacing: landscape ? 8 : 16) {
                    header
                    if landscape {
                        HStack(spacing: 18) {
                            joystick(size: min(190, geometry.size.height - 140))
                            camera.frame(maxWidth: .infinity, maxHeight: .infinity)
                            actions.frame(width: 150)
                        }
                        footer
                    } else {
                        camera.frame(height: geometry.size.height * 0.31)
                        HStack(alignment: .center, spacing: 16) {
                            joystick(size: min(200, geometry.size.width * 0.50))
                            actions.frame(maxWidth: .infinity)
                        }.frame(maxHeight: .infinity)
                        footer
                    }
                }
                .padding(.horizontal, landscape ? 16 : 20)
                .padding(.vertical, 10)
            }
            .onChange(of: geometry.size) { _ in pilot.releaseStick() }
        }
        .sheet(isPresented: $showConnection) { connectionSheet }
        .sheet(isPresented: $showHelp) { helpSheet }
        .onChange(of: pilot.link) { link in
            if link == .connected { showConnection = false }
        }
        .onDisappear { pilot.stop() }
        .onChange(of: dragging) { active in
            if !active { pilot.releaseStick() }
        }
    }

    private var header: some View {
        HStack {
            HStack(spacing: 9) {
                Image(systemName: "steeringwheel").font(.title2).foregroundStyle(mint)
                VStack(alignment: .leading, spacing: 1) {
                    Text("JUMP PILOT").font(.system(.headline, design: .rounded, weight: .heavy)).tracking(1.5)
                    Text("PARROT JUMPING").font(.system(size: 9, weight: .semibold)).tracking(2).foregroundStyle(.secondary)
                }
            }
            Spacer()
            if let battery = pilot.battery {
                Label("\(battery) %", systemImage: battery < 20 ? "battery.25" : "battery.75")
                    .font(.system(.subheadline, design: .monospaced)).foregroundStyle(battery < 20 ? Color.orange : mint)
            }
            Button {
                pilot.stop(); showHelp = true
            } label: { Image(systemName: "questionmark.circle").font(.title3).frame(width: 36, height: 40) }
                .tint(.white).accessibilityLabel("Aide")
            Button {
                pilot.stop(); showConnection = true
            } label: {
                HStack(spacing: 6) {
                    Circle().fill(pilot.link == .connected ? mint : Color.orange).frame(width: 6, height: 6)
                    Text(pilot.link == .connected ? "Connecté" : "Connexion").font(.subheadline.weight(.semibold))
                }
                .padding(.horizontal, 12).frame(height: 38)
                .background(panel, in: Capsule())
            }.tint(.white)
        }
    }

    private var camera: some View {
        TimelineView(.periodic(from: .now, by: 0.5)) { timeline in
            let fresh = pilot.videoDate.map { timeline.date.timeIntervalSince($0) < 2 } ?? false
            ZStack {
                RoundedRectangle(cornerRadius: 22).fill(panel)
                if let image = pilot.image {
                    GeometryReader { geometry in
                        Image(uiImage: image).resizable().scaledToFit()
                            .frame(width: geometry.size.width, height: geometry.size.height)
                            .opacity(fresh ? 1 : 0.30)
                    }
                }
                if !fresh {
                    VStack(spacing: 10) {
                        Image(systemName: pilot.link == .connected ? "video.slash" : "wifi")
                            .font(.system(size: 28, weight: .light)).foregroundStyle(mint)
                        Text(pilot.link == .connected ? "En attente de la caméra" : "Votre point de vue, à ras du sol.")
                            .font(.subheadline.weight(.medium)).multilineTextAlignment(.center)
                        Text(pilot.link == .connected ? "Le pilotage reste disponible." : "Rejoignez le Wi-Fi du drone, puis connectez-vous.")
                            .font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center)
                    }.padding(20)
                }
                VStack {
                    HStack {
                        Label(fresh ? "CAMÉRA EN DIRECT" : "CAMÉRA", systemImage: "viewfinder")
                            .font(.system(size: 9, weight: .bold, design: .monospaced)).tracking(1)
                            .padding(9).background(.black.opacity(0.5), in: Capsule())
                        Spacer()
                        if fresh { Circle().fill(mint).frame(width: 6, height: 6) }
                    }
                    Spacer()
                }.padding(12)
            }
            .clipShape(RoundedRectangle(cornerRadius: 22))
            .overlay(RoundedRectangle(cornerRadius: 22).stroke(.white.opacity(0.08)))
        }
        .accessibilityLabel("Retour vidéo de la caméra du drone")
    }

    private func joystick(size: CGFloat) -> some View {
        VStack(spacing: 8) {
            ZStack {
                Circle().fill(panel)
                Circle().stroke(.white.opacity(0.10), lineWidth: 1)
                Circle().stroke(.white.opacity(0.05), style: StrokeStyle(lineWidth: 1, dash: [3, 6])).padding(24)
                Rectangle().fill(.white.opacity(0.07)).frame(width: 1).padding(.vertical, 15)
                Rectangle().fill(.white.opacity(0.07)).frame(height: 1).padding(.horizontal, 15)
                VStack { Image(systemName: "chevron.up"); Spacer(); Image(systemName: "chevron.down") }
                    .padding(12).font(.caption2.weight(.bold)).foregroundStyle(.gray)
                HStack { Image(systemName: "chevron.left"); Spacer(); Image(systemName: "chevron.right") }
                    .padding(12).font(.caption2.weight(.bold)).foregroundStyle(.gray)
                Circle().fill(pilot.armed ? mint : Color.gray.opacity(0.4))
                    .frame(width: size * 0.34, height: size * 0.34)
                    .shadow(color: pilot.armed ? mint.opacity(0.2) : .clear, radius: 18)
                    .offset(x: pilot.stick.x * size * 0.30, y: pilot.stick.y * size * 0.30)
            }
            .frame(width: size, height: size)
            .contentShape(Circle())
            .gesture(DragGesture(minimumDistance: 0)
                .updating($dragging) { _, active, _ in active = true }
                .onChanged { value in
                    pilot.moveStick(x: (value.location.x - size / 2) / (size * 0.30),
                                    y: (value.location.y - size / 2) / (size * 0.30))
                }
                .onEnded { _ in pilot.releaseStick() })
            .accessibilityLabel("Joystick. Glissez vers le haut pour avancer, vers le bas pour reculer, à gauche ou à droite pour tourner.")
            .accessibilityAddTraits(.allowsDirectInteraction)
            Text(pilot.armed ? "RELÂCHER = ARRÊTER LES ROUES" : "ACTIVEZ LES COMMANDES")
                .font(.system(size: 8, weight: .semibold)).tracking(0.8).foregroundStyle(.secondary)
        }
    }

    private var actions: some View {
        VStack(spacing: 10) {
            if !pilot.armed {
                Button { pilot.activate() } label: {
                    Label("Activer", systemImage: "power").frame(maxWidth: .infinity).frame(height: 40)
                }
                .buttonStyle(.borderedProminent).tint(mint).foregroundStyle(.black)
                .disabled(pilot.link != .connected || pilot.alert == 1)
            }
            TimelineView(.periodic(from: .now, by: 0.2)) { timeline in
                VStack(spacing: 8) {
                    jumpButton("Saut haut", icon: "arrow.up", high: true)
                    jumpButton("Saut long", icon: "arrow.up.right", high: false)
                }
                .disabled(!pilot.armed || timeline.date < pilot.jumpAvailableAt)
            }
            Button {
                pilot.stop()
            } label: {
                Label("STOP", systemImage: "stop.fill")
                    .font(.system(.headline, design: .rounded, weight: .heavy))
                    .frame(maxWidth: .infinity).frame(height: 48)
            }
            .buttonStyle(.borderedProminent).tint(Color(red: 0.93, green: 0.25, blue: 0.25)).foregroundStyle(.white)
            .accessibilityLabel("Arrêter les roues et désactiver les commandes")
        }
    }

    private func jumpButton(_ title: String, icon: String, high: Bool) -> some View {
        Button { pilot.jump(high: high) } label: {
            Label(title, systemImage: icon).font(.subheadline.weight(.semibold))
                .frame(maxWidth: .infinity).frame(height: 37)
        }.buttonStyle(.bordered).tint(mint)
    }

    private var footer: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 10) {
                Text("VITESSE").font(.system(size: 9, weight: .bold)).tracking(1).foregroundStyle(.secondary)
                Slider(value: $pilot.speedLimit, in: 15...100, step: 5).tint(mint)
                    .accessibilityLabel("Limite de vitesse en pourcentage")
                Text("\(Int(pilot.speedLimit)) %").font(.system(.caption, design: .monospaced)).frame(width: 46)
            }
            Text(pilot.status)
                .font(.caption).foregroundStyle(pilot.link == .failed ? Color.orange : Color.secondary)
                .lineLimit(2).frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var connectionSheet: some View {
        NavigationStack {
            Form {
                Section("Votre drone") {
                    Text("Jumping Sumo · Jumping Night · Jumping Race")
                    Text("FreeFlight Jumping est le nom de l’application Parrot. Ces trois robots utilisent le même protocole de commande.")
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section("Connexion en trois étapes") {
                    Label("Allumez le drone et fermez FreeFlight.", systemImage: "1.circle")
                    Label("Dans Réglages > Wi-Fi, rejoignez son réseau Jumping…", systemImage: "2.circle")
                    Label("Revenez ici et touchez « Se connecter ».", systemImage: "3.circle")
                    Text("Le réseau du drone n’a pas besoin d’accès Internet. Autorisez l’accès au réseau local lorsque l’iPhone le demande.")
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section("Adresse du drone") {
                    TextField("192.168.2.1", text: $pilot.address)
                        .keyboardType(.numbersAndPunctuation).textInputAutocapitalization(.never).autocorrectionDisabled()
                        .disabled(pilot.link == .connecting || pilot.link == .connected)
                    Text("Conservez 192.168.2.1 sauf si vous avez modifié le réseau du drone.")
                        .font(.caption).foregroundStyle(.secondary)
                }
                Section {
                    if pilot.link == .connecting {
                        HStack { ProgressView(); Text("Connexion en cours…") }
                        Button("Annuler", role: .cancel) { pilot.disconnect() }
                    } else if pilot.link == .connected {
                        Button("Se déconnecter", role: .destructive) { pilot.disconnect() }
                    } else {
                        Button("Se connecter") { pilot.connect() }.tint(mint)
                    }
                    Text(pilot.status).font(.caption)
                }
            }
            .navigationTitle("Connexion")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Fermer") { showConnection = false } } }
        }.preferredColorScheme(.dark)
    }

    private var helpSheet: some View {
        NavigationStack {
            List {
                Section("Pilotage") {
                    Text("1. Connectez le drone puis touchez Activer.")
                    Text("2. Maintenez le joystick : haut pour avancer, bas pour reculer, gauche et droite pour tourner.")
                    Text("3. Relâchez le joystick pour arrêter les roues. STOP désactive aussi les commandes.")
                    Text("4. Commencez avec une vitesse de 35 %. Tournez l’iPhone pour une vue plus large.")
                }
                Section("Sauts et caméra") {
                    Text("Un intervalle de trois secondes sépare les demandes de saut. Utilisez un espace dégagé. Un saut déjà lancé ne peut pas être annulé en l’air.")
                    Text("Une image sans mise à jour pendant deux secondes est assombrie. Gardez le robot en vue directe.")
                }
                Section("Si la connexion échoue") {
                    Text("Vérifiez le réseau Wi-Fi du drone et fermez les autres applications de pilotage.")
                    Text("Dans Réglages > Confidentialité et sécurité > Réseau local, autorisez Jump Pilot, puis réessayez.")
                    Text("Une interruption désactive les commandes. Le verrouillage ou le passage en arrière-plan coupe la session : reconnectez-vous puis réactivez les commandes.")
                }
                Section("À propos") {
                    Text("Jump Pilot 1.0 · application indépendante pour Parrot Jumping. Connexion locale, sans compte ni collecte de données.")
                    Text("Version initiale : protocole testé avec un simulateur réseau. La compilation Xcode et le fonctionnement sur un drone réel restent à valider.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Mode d’emploi")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Fermer") { showHelp = false } } }
        }.preferredColorScheme(.dark)
    }
}

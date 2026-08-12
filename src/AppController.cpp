#include "AppController.h"

#include <WiFi.h>
#include <mbedtls/base64.h>
#include <string.h>

#include "ListViewport.h"

namespace app {

namespace {

constexpr uint16_t kAccent = 0x07FF;
constexpr uint16_t kSuccess = 0x07E0;
constexpr uint16_t kWarning = 0xFD20;
constexpr uint16_t kError = 0xF800;
constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kFrameIntervalMs = 50;
constexpr uint32_t kCursorBlinkIntervalMs = 500;

bool recoveryCombo() {
    bool fn = false;
    bool backspace = false;
    for (const Point2D_t& point : M5Cardputer.Keyboard.keyList()) {
        if (point.x < 0 || point.y < 0 || point.x >= static_cast<int>(kKeyboardColumns) ||
            point.y >= static_cast<int>(kKeyboardRows)) continue;
        const PhysicalKeyId id = keyId(static_cast<uint8_t>(point.y), static_cast<uint8_t>(point.x));
        fn = fn || id == kKeyFn;
        backspace = backspace || id == kKeyDelete;
    }
    return fn && backspace;
}

void clearSensitiveString(String& value) {
    if (!value.isEmpty()) secureClear(value.begin(), value.length());
    value = "";
}

String priorityLabel(uint8_t priority) {
    if (priority == static_cast<uint8_t>(WifiPriority::High)) return "High";
    if (priority == static_cast<uint8_t>(WifiPriority::Low)) return "Low";
    return "Normal";
}

}  // namespace

void AppController::begin() {
    Serial.begin(115200);
    auto hardwareConfig = M5.config();
    M5Cardputer.begin(hardwareConfig, true);
    M5Cardputer.Display.setRotation(1);
    if (!ui_.begin()) {
        M5Cardputer.Display.fillScreen(TFT_RED);
        M5Cardputer.Display.setCursor(4, 4);
        M5Cardputer.Display.print("Display buffer failed");
        while (true) delay(1000);
    }

    ui_.progress("Cardputer SSH", "Starting", 0, 0, true);
    if (bootRecoveryRequested()) recoveryMenu();
    while (!loadConfiguration()) delay(10);
    flow_.apply(SessionEvent::ConfigReady);
    const int directWifiIndex = ensureWifiConfigured();
    wifiConnectionFlow(directWifiIndex);
    hostLauncher();
    renderTerminal(true);
}

void AppController::loop() {
    if (!flow_.sessionLive()) {
        if (WiFi.status() != WL_CONNECTED) wifiConnectionFlow();
        hostLauncher();
        renderTerminal(true);
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        closeTerminalSession(SessionEvent::WifiLost);
        return;
    }

    KeyEvent eventValue;
    const InputContext context = flow_.mode() == TerminalMode::Remote ? InputContext::RemoteTerminal
                                                                      : InputContext::LocalScroll;
    if (pollEvent(context, eventValue)) handleTerminalEvent(eventValue);
    readSshOutput();
    renderTerminal();

    if (flow_.sessionLive() && !ssh_.connected()) {
        waitMessage("SSH session closed", "Returned to host list", "Wi-Fi stays connected", kWarning);
        closeTerminalSession(SessionEvent::RemoteClosed);
    }
    delay(2);
}

bool AppController::operationCanceledThunk(void* context) {
    return static_cast<AppController*>(context)->operationCanceled();
}

void AppController::operationProgressThunk(const char* stage, uint16_t current, uint16_t total, void* context) {
    static_cast<AppController*>(context)->operationProgress(stage, current, total);
}

OperationCallbacks AppController::operationCallbacks() {
    OperationCallbacks callbacks;
    callbacks.canceled = operationCanceledThunk;
    callbacks.progress = operationProgressThunk;
    callbacks.context = this;
    return callbacks;
}

bool AppController::operationCanceled() {
    KeyEvent eventValue;
    return pollEvent(InputContext::Dialog, eventValue) && eventValue.type == KeyEventType::Back;
}

void AppController::operationProgress(const char* stage, uint16_t current, uint16_t total) {
    ui_.progress(stage == nullptr ? "Working" : stage, operationDetail_, current, total);
}

bool AppController::pollEvent(InputContext context, KeyEvent& eventValue) {
    keyboard_.pollHardware(context, millis());
    return keyboard_.pop(eventValue);
}

KeyEvent AppController::waitEvent(InputContext context) {
    KeyEvent eventValue;
    while (!pollEvent(context, eventValue)) {
        serviceBackgroundSession();
        delay(5);
    }
    return eventValue;
}

void AppController::transitionInputContext(InputContext context) {
    keyboard_.suppressUntilReleased();
    keyboard_.pollHardware(context, millis());
}

int AppController::selectMenu(const String& title, const String* labels, const String* details,
                              size_t count, size_t initial, const String& step) {
    if (labels == nullptr || count == 0) return -1;
    size_t selected = min(initial, count - 1);
    ListViewportState viewport = listViewport(count, selected, 0);
    transitionInputContext(InputContext::Menu);
    ui_.list(title, labels, details, count, selected, viewport.first,
             "Fn+Arrows  Enter  Del: Back", step);
    while (true) {
        const KeyEvent eventValue = waitEvent(InputContext::Menu);
        if (eventValue.type == KeyEventType::Back || eventValue.type == KeyEventType::MoveLeft) return -1;
        if (eventValue.type == KeyEventType::MoveUp) selected = selected == 0 ? count - 1 : selected - 1;
        else if (eventValue.type == KeyEventType::MoveDown) selected = (selected + 1) % count;
        else if (eventValue.type == KeyEventType::Confirm || eventValue.type == KeyEventType::MoveRight)
            return static_cast<int>(selected);
        else continue;
        viewport = listViewport(count, selected, viewport.first);
        ui_.list(title, labels, details, count, selected, viewport.first,
                 "Fn+Arrows  Enter  Del: Back", step);
    }
}

bool AppController::inputText(const String& title, const String& label, String& value, size_t maximum,
                              bool masked, bool allowEmpty, const String& step) {
    maximum = min(maximum, kSshPasswordCapacity - 1);
    char storage[kSshPasswordCapacity]{};
    value.toCharArray(storage, maximum + 1);
    TextEditModel edit(storage, maximum + 1);
    transitionInputContext(InputContext::TextInput);
    while (true) {
        char visible[kSshPasswordCapacity]{};
        size_t viewportStart = 0;
        size_t cursorColumn = 0;
        edit.visibleSlice(visible, sizeof(visible), 36, masked, viewportStart, cursorColumn);
        ui_.input(title, label, String(visible), cursorColumn, viewportStart > 0,
                  "Enter OK  Fn+` Back  Del/Fn+Del", step);
        const KeyEvent eventValue = waitEvent(InputContext::TextInput);
        if (eventValue.type == KeyEventType::Back) { edit.clearSensitive(); return false; }
        if (eventValue.type == KeyEventType::MoveLeft) edit.moveLeft();
        else if (eventValue.type == KeyEventType::MoveRight) edit.moveRight();
        else if (eventValue.type == KeyEventType::DeleteBackward) edit.eraseBeforeCursor();
        else if (eventValue.type == KeyEventType::DeleteForward) edit.eraseAtCursor();
        else if (eventValue.type == KeyEventType::Text) edit.insert(static_cast<char>(eventValue.value));
        else if (eventValue.type == KeyEventType::Confirm && (allowEmpty || edit.length() > 0)) {
            value = edit.data(); edit.clearSensitive(); return true;
        }
    }
}

bool AppController::confirm(const String& title, const String& detail, const String& acceptLabel) {
    const String labels[] = {"Cancel", acceptLabel};
    const String details[] = {"", detail};
    return selectMenu(title, labels, details, 2, 0) == 1;
}

void AppController::waitMessage(const String& title, const String& line1, const String& line2, uint16_t color) {
    ui_.dialog(title, line1, line2, "Enter: continue", color);
    transitionInputContext(InputContext::Dialog);
    while (true) {
        const KeyEvent eventValue = waitEvent(InputContext::Dialog);
        if (eventValue.type == KeyEventType::Confirm || eventValue.type == KeyEventType::Back) return;
    }
}

bool AppController::bootRecoveryRequested() {
    const uint32_t started = millis();
    bool detected = false;
    while (millis() - started < 600U) {
        M5Cardputer.update();
        if (recoveryCombo()) { detected = true; break; }
        delay(10);
    }
    if (!detected) return false;
    const uint32_t heldAt = millis();
    while (millis() - heldAt < 3000U) {
        M5Cardputer.update();
        if (!recoveryCombo()) return false;
        ui_.progress("Recovery", "Keep holding Fn+Del", millis() - heldAt, 3000U);
        delay(20);
    }
    return true;
}

void AppController::recoveryMenu() {
    const String labels[] = {"Continue boot", "Clear all configuration", "Restart"};
    const int action = selectMenu("Recovery", labels, nullptr, 3, 0);
    if (action == 1 && confirm("Clear everything?", "Removes Wi-Fi and hosts", "Clear")) {
        if (profileStore_.clearAll()) ESP.restart();
        waitMessage("Clear failed", "NVS was not changed", "Try restarting", kError);
    } else if (action == 2) ESP.restart();
}

bool AppController::loadConfiguration() {
    while (true) {
        const ProfileLoadStatus status = profileStore_.load(config_);
        if (status == ProfileLoadStatus::LoadedV2 || status == ProfileLoadStatus::MigratedV1) return true;
        if (status == ProfileLoadStatus::Missing) { resetProfileConfig(config_); return true; }
        const String labels[] = {"Retry", "Clear all", "Restart"};
        const int action = selectMenu(status == ProfileLoadStatus::Invalid ? "Invalid configuration" : "Storage error",
                                      labels, nullptr, 3, 0);
        if (action == 1 && confirm("Clear all config?", "Cannot be undone", "Clear") && profileStore_.clearAll()) {
            resetProfileConfig(config_); return true;
        }
        if (action == 2) ESP.restart();
    }
}

bool AppController::persistConfiguration(const char* failureTitle) {
    if (validateProfileConfig(config_) && profileStore_.save(config_)) {
        ++configGeneration_;
        return true;
    }
    waitMessage(failureTitle, "Configuration kept", "Check Diagnostics", kError);
    return false;
}

int AppController::ensureWifiConfigured() {
    if (config_.wifiCount > 0) return -1;
    while (config_.wifiCount == 0) {
        waitMessage("First setup", "Add a 2.4 GHz Wi-Fi", "No secrets in firmware", kAccent);
        const int saved = editWifiProfile(-1, true);
        if (saved >= 0) {
            ui_.dialog("Wi-Fi saved", config_.wifi[saved].ssid, "Connecting now", "", kSuccess);
            delay(350);
            return saved;
        }
        const String labels[] = {"Add Wi-Fi", "Restart"};
        if (selectMenu("Wi-Fi required", labels, nullptr, 2, 0) == 1) ESP.restart();
    }
    return -1;
}

NetworkResult AppController::connectWifiProfile(uint8_t index) {
    if (index >= config_.wifiCount || config_.wifi[index].enabled == 0) {
        return NetworkResult::Failed;
    }
    operationDetail_ = config_.wifi[index].ssid;
    const NetworkResult result = wifi_.connect(config_.wifi[index], kWifiTimeoutMs, operationCallbacks());
    if (result == NetworkResult::Success) {
        activeWifiIndex_ = index;
        flow_.apply(SessionEvent::WifiConnected);
        return result;
    }
    activeWifiIndex_ = -1;
    flow_.apply(SessionEvent::WifiFailed);
    return result;
}

bool AppController::connectConfiguredWifi() {
    WifiScanModel scanModel;
    operationDetail_ = "Channels 1-13";
    const WifiScanResult scanResult = wifi_.scan(scanModel, operationCallbacks());
    lastScanSummary_ = scanModel.summary();
    if (scanResult == WifiScanResult::Canceled) return false;

    WifiCandidate candidates[kMaxWifiProfiles]{};
    const size_t count = WifiProfileSelector::build(config_.wifi, config_.wifiCount,
                                                     scanModel.results(), scanModel.resultCount(),
                                                     candidates, kMaxWifiProfiles);
    for (size_t index = 0; index < count; ++index) {
        const uint8_t profileIndex = candidates[index].profileIndex;
        operationDetail_ = String("Profile ") + (profileIndex + 1);
        const NetworkResult result = wifi_.connect(config_.wifi[profileIndex], kWifiTimeoutMs, operationCallbacks());
        if (result == NetworkResult::Success) {
            activeWifiIndex_ = profileIndex;
            flow_.apply(SessionEvent::WifiConnected);
            return true;
        }
        if (result == NetworkResult::Canceled) return false;
    }
    activeWifiIndex_ = -1;
    flow_.apply(SessionEvent::WifiFailed);
    return false;
}

void AppController::wifiConnectionFlow(int directIndex) {
    while (WiFi.status() != WL_CONNECTED) {
        if (directIndex >= 0) {
            const NetworkResult result = connectWifiProfile(static_cast<uint8_t>(directIndex));
            if (result == NetworkResult::Success) return;

            const String labels[] = {"Retry", "Edit password", "Choose another network"};
            const String title = result == NetworkResult::Canceled ? "Wi-Fi connection canceled"
                                                                    : "Wi-Fi connection failed";
            bool retryDirect = false;
            while (directIndex >= 0 && !retryDirect) {
                const int action = selectMenu(title, labels, nullptr, 3, 0);
                if (action == 0) {
                    retryDirect = true;
                } else if (action == 1) {
                    const int edited = editWifiProfile(directIndex, false, true);
                    if (edited >= 0) {
                        directIndex = edited;
                        ui_.dialog("Wi-Fi saved", config_.wifi[edited].ssid, "Connecting now", "", kSuccess);
                        delay(350);
                        retryDirect = true;
                    }
                } else if (action == 2) {
                    const int created = editWifiProfile(-1, true);
                    if (created >= 0) {
                        directIndex = created;
                        ui_.dialog("Wi-Fi saved", config_.wifi[created].ssid, "Connecting now", "", kSuccess);
                        delay(350);
                        retryDirect = true;
                    }
                } else {
                    directIndex = -1;
                }
            }
            if (retryDirect) continue;
        } else if (connectConfiguredWifi()) {
            return;
        }

        const String labels[] = {"Retry", "Add Wi-Fi", "Manage Wi-Fi", "Restart"};
        const int action = selectMenu("Wi-Fi recovery", labels, nullptr, 4, 0);
        if (action == 1) {
            const int created = editWifiProfile(-1, true);
            if (created >= 0) {
                directIndex = created;
                ui_.dialog("Wi-Fi saved", config_.wifi[created].ssid, "Connecting now", "", kSuccess);
                delay(350);
            }
        } else if (action == 2) {
            wifiManagerMenu();
        } else if (action == 3) {
            ESP.restart();
        }
    }
}

int AppController::editWifiProfile(int index, bool creating, bool passwordOnly) {
    if (creating && config_.wifiCount >= kMaxWifiProfiles) {
        waitMessage("Wi-Fi list full", "Maximum is 4", "Delete one first", kWarning);
        return -1;
    }
    WifiProfile profile{};
    if (!creating) {
        if (index < 0 || static_cast<size_t>(index) >= config_.wifiCount) return -1;
        profile = config_.wifi[index];
    } else {
        profile.priority = static_cast<uint8_t>(WifiPriority::Normal);
        profile.enabled = 1;
        WifiScanModel model;
        operationDetail_ = "Choose network";
        if (wifi_.scan(model, operationCallbacks()) == WifiScanResult::Canceled) return -1;
        lastScanSummary_ = model.summary();
        String labels[kMaxWifiResults + 2];
        String details[kMaxWifiResults + 2];
        for (size_t result = 0; result < model.resultCount(); ++result) {
            labels[result] = model.results()[result].ssid;
            details[result] = String(model.results()[result].rssi) + " dBm";
        }
        const size_t hiddenIndex = model.resultCount();
        labels[hiddenIndex] = "Hidden SSID...";
        labels[hiddenIndex + 1] = "Cancel";
        const int selected = selectMenu("Add Wi-Fi", labels, details, hiddenIndex + 2, 0);
        if (selected < 0 || selected == static_cast<int>(hiddenIndex + 1)) return -1;
        profile.hidden = selected == static_cast<int>(hiddenIndex) ? 1 : 0;
        if (profile.hidden == 0) {
            copyConfigString(profile.ssid, sizeof(profile.ssid), model.results()[selected].ssid);
        }
    }

    String ssid = profile.ssid;
    if ((creating && profile.hidden != 0) || (!creating && !passwordOnly)) {
        if (!inputText("Wi-Fi profile", "SSID", ssid, kWifiSsidCapacity - 1, false, false)) return -1;
    }
    String password = profile.password;
    if (!inputText("Wi-Fi profile", "Password (empty=open)", password,
                   kWifiPasswordCapacity - 1, true, true)) {
        clearSensitiveString(password);
        return -1;
    }
    if (!copyConfigString(profile.ssid, sizeof(profile.ssid), ssid.c_str()) ||
        !copyConfigString(profile.password, sizeof(profile.password), password.c_str())) {
        clearSensitiveString(password);
        waitMessage("Invalid Wi-Fi", "Value too long", "Nothing saved", kError);
        return -1;
    }
    clearSensitiveString(password);
    profile.enabled = 1;
    profile.reserved = 0;

    ProfileConfigData updated = config_;
    const bool changed = creating ? addWifiProfile(updated, profile)
                                  : replaceWifiProfile(updated, static_cast<size_t>(index), profile);
    const int savedIndex = creating ? static_cast<int>(config_.wifiCount) : index;
    secureClear(&profile, sizeof(profile));
    if (!changed || !profileStore_.save(updated)) {
        secureClear(&updated, sizeof(updated));
        waitMessage("Wi-Fi not saved", "Duplicate or invalid", "Old settings kept", kError);
        return -1;
    }
    config_ = updated;
    secureClear(&updated, sizeof(updated));
    ++configGeneration_;
    return savedIndex;
}

void AppController::wifiManagerMenu() {
    while (true) {
        String labels[kMaxWifiProfiles + 2];
        String details[kMaxWifiProfiles + 2];
        for (uint8_t index = 0; index < config_.wifiCount; ++index) {
            labels[index] = String(config_.wifi[index].enabled ? "[ON] " : "[OFF] ") + config_.wifi[index].ssid;
            details[index] = priorityLabel(config_.wifi[index].priority);
        }
        labels[config_.wifiCount] = "Add Wi-Fi...";
        labels[config_.wifiCount + 1] = "Back";
        const int selected = selectMenu("Wi-Fi profiles", labels, details, config_.wifiCount + 2, 0);
        if (selected < 0 || selected == config_.wifiCount + 1) return;
        if (selected == config_.wifiCount) { editWifiProfile(-1, true); continue; }

        const size_t index = static_cast<size_t>(selected);
        const String actions[] = {"Edit credentials", config_.wifi[index].enabled ? "Disable" : "Enable",
                                  "Change priority", "Delete", "Back"};
        const int action = selectMenu(config_.wifi[index].ssid, actions, nullptr, 5, 0);
        if (action == 0) editWifiProfile(selected, false);
        else if (action == 1 || action == 2 || action == 3) {
            ProfileConfigData updated = config_;
            bool changed = false;
            if (action == 1) {
                WifiProfile profile = updated.wifi[index];
                profile.enabled = profile.enabled ? 0 : 1;
                changed = replaceWifiProfile(updated, index, profile);
                secureClear(&profile, sizeof(profile));
            } else if (action == 2) {
                WifiProfile profile = updated.wifi[index];
                profile.priority = static_cast<uint8_t>((profile.priority + 1) % 3);
                changed = replaceWifiProfile(updated, index, profile);
                secureClear(&profile, sizeof(profile));
            } else if (confirm("Delete Wi-Fi?", config_.wifi[index].ssid, "Delete")) {
                changed = deleteWifiProfile(updated, index);
            }
            if (changed && profileStore_.save(updated)) {
                config_ = updated;
                ++configGeneration_;
            } else if (changed) waitMessage("Save failed", "Old settings kept", "", kError);
            secureClear(&updated, sizeof(updated));
        }
    }
}

void AppController::hostLauncher() {
    while (!flow_.sessionLive()) {
        if (WiFi.status() != WL_CONNECTED) return;
        uint8_t indices[kMaxSshHostProfiles]{};
        const size_t enabled = HostCatalog::launcherIndices(config_, indices, kMaxSshHostProfiles);
        String labels[kMaxSshHostProfiles + 3];
        String details[kMaxSshHostProfiles + 3];
        for (size_t row = 0; row < enabled; ++row) {
            const SshHostProfile& host = config_.hosts[indices[row]];
            labels[row] = host.displayName;
            details[row] = clipped(String(host.user) + "@" + host.host + ":" + host.port, 38);
        }
        labels[enabled] = "Discover SSH...";
        labels[enabled + 1] = "Add host...";
        labels[enabled + 2] = "Settings";
        const int selected = selectMenu("SSH HOSTS", labels, details, enabled + 3, 0,
                                        String(enabled) + "/" + config_.hostCount + "  WIFI READY");
        if (selected < 0 || selected == static_cast<int>(enabled + 2)) {
            settingsMenu();
            if (flow_.sessionLive()) return;
            continue;
        }
        if (selected == static_cast<int>(enabled)) {
            if (discoverHost() == HostEditOutcome::Connected) return;
            continue;
        }
        if (selected == static_cast<int>(enabled + 1)) {
            if (editHostProfile(-1, true) == HostEditOutcome::Connected) return;
            continue;
        }

        String error;
        const uint8_t hostIndex = indices[selected];
        const SshAttemptResult result = connectHostProfile(hostIndex, error);
        if (result == SshAttemptResult::Success) return;
        if (result == SshAttemptResult::HostKeyMismatch) {
            waitMessage("HOST KEY MISMATCH", "Password was not sent", "Reset trust only if expected", kError);
            flow_.apply(SessionEvent::RemoteClosed);
        } else if (result != SshAttemptResult::Canceled) {
            waitMessage("SSH connection failed", clipped(error, 38), "Choose a host", kError);
        }
    }
}

AppController::HostEditOutcome AppController::editHostProfile(int index,
                                                               bool creating,
                                                               const SshServiceInfo* seed) {
    if (creating && config_.hostCount >= kMaxSshHostProfiles) {
        waitMessage("Host list full", "Maximum is 8", "Delete one first", kWarning);
        return HostEditOutcome::Stay;
    }

    const bool startedInSettings = flow_.page() == AppPage::Settings;
    if (!prepareHostDraftAttempt()) return HostEditOutcome::Stay;
    const HostEditOutcome canceledOutcome = startedInSettings ? HostEditOutcome::ReturnToLauncher
                                                               : HostEditOutcome::Stay;

    SshHostProfile initial{};
    if (!creating) {
        if (index < 0 || static_cast<size_t>(index) >= config_.hostCount) return canceledOutcome;
        initial = config_.hosts[index];
    } else {
        initial.port = kDefaultSshPort;
        initial.enabled = 1;
        copyConfigString(initial.user, sizeof(initial.user), "pi");
        if (seed != nullptr) {
            copyConfigString(initial.displayName, sizeof(initial.displayName), seed->name);
            copyConfigString(initial.host, sizeof(initial.host), seed->target);
            initial.port = seed->port;
        }
    }

    if (!editHostDraftFields(initial)) {
        secureClear(&initial, sizeof(initial));
        return canceledOutcome;
    }

    HostDraftTransaction transaction(initial, creating ? -1 : index);
    secureClear(&initial, sizeof(initial));
    if (!transaction.beginAttempt()) return canceledOutcome;

    while (true) {
        String error;
        const DraftAttemptResult result = connectHostDraft(transaction.profile(), error);
        if (result == DraftAttemptResult::Authenticated) {
            if (!transaction.authenticationSucceeded()) {
                closeTerminalSession(SessionEvent::RemoteClosed);
                transaction.discard();
                return canceledOutcome;
            }

            uint8_t savedIndex = 0;
            if (!persistAuthenticatedHostDraft(transaction, savedIndex)) {
                transaction.persistenceFailed();
                closeTerminalSession(SessionEvent::RemoteClosed);
                waitMessage("Connected, save failed", "Old settings kept", "Draft discarded", kError);
                transaction.discard();
                return canceledOutcome;
            }

            transaction.persistenceSucceeded();
            flow_.apply(SessionEvent::Authenticated);
            activeHostIndex_ = savedIndex;
            resetTerminalSessionState(true);
            return HostEditOutcome::Connected;
        }

        transaction.attemptFailed();
        String title = "SSH connection failed";
        String detail = error.isEmpty() ? String("Nothing saved") : clipped(error, 38);
        if (result == DraftAttemptResult::HostKeyMismatch) {
            title = "HOST KEY MISMATCH";
            detail = "Password was not sent";
            flow_.apply(SessionEvent::RemoteClosed);
        } else if (result == DraftAttemptResult::Canceled) {
            title = "SSH connection canceled";
            detail = "Nothing saved";
        }

        const String labels[] = {"Retry", "Edit", "Discard"};
        const String details[] = {detail, detail, detail};
        const int action = selectMenu(title, labels, details, 3, 0);
        if (action == 0 && transaction.retry()) continue;
        if (action == 1) {
            if (editHostDraftFields(transaction.profile())) {
                transaction.edit();
                transaction.beginAttempt();
            }
            continue;
        }
        transaction.discard();
        return canceledOutcome;
    }
}

bool AppController::editHostDraftFields(SshHostProfile& host) {
    char previousHost[kSshHostCapacity]{};
    copyConfigString(previousHost, sizeof(previousHost), host.host);
    const uint16_t previousPort = host.port;

    String name = host.displayName;
    String target = host.host;
    String port = String(host.port);
    String user = host.user;
    if (!inputText("SSH host", "Display name", name, kHostDisplayNameCapacity - 1, false, false) ||
        !inputText("SSH host", "Host or IP", target, kSshHostCapacity - 1, false, false) ||
        !inputText("SSH host", "Port", port, 5, false, false) ||
        !inputText("SSH host", "Username", user, kSshUserCapacity - 1, false, false)) {
        return false;
    }
    const long parsedPort = port.toInt();
    if (parsedPort < 1 || parsedPort > 65535) {
        waitMessage("Invalid port", "Use 1-65535", "Nothing saved", kError);
        return false;
    }
    const String passwordChoices[] = {"Prompt each connection", "Save password"};
    const int passwordMode = selectMenu("SSH password", passwordChoices, nullptr, 2, host.savePassword ? 1 : 0);
    if (passwordMode < 0) return false;
    String password = passwordMode == 1 ? String(host.password) : String();
    if (passwordMode == 1 &&
        !inputText("SSH password", "Password", password, kSshPasswordCapacity - 1, true, false)) {
        clearSensitiveString(password);
        return false;
    }

    if (!copyConfigString(host.displayName, sizeof(host.displayName), name.c_str()) ||
        !copyConfigString(host.host, sizeof(host.host), target.c_str()) ||
        !copyConfigString(host.user, sizeof(host.user), user.c_str()) ||
        !copyConfigString(host.password, sizeof(host.password), password.c_str())) {
        clearSensitiveString(password);
        waitMessage("Invalid host", "Value too long", "Nothing saved", kError);
        return false;
    }
    clearSensitiveString(password);
    host.port = static_cast<uint16_t>(parsedPort);
    host.savePassword = passwordMode == 1 ? 1 : 0;
    host.enabled = 1;
    if (host.savePassword == 0) secureClear(host.password, sizeof(host.password));

    if (strcmp(previousHost, host.host) != 0 || previousPort != host.port) {
        host.hostKeyTrusted = 0;
        secureClear(host.hostKeySha256, sizeof(host.hostKeySha256));
    }
    secureClear(previousHost, sizeof(previousHost));
    return true;
}

bool AppController::prepareHostDraftAttempt() {
    if (flow_.page() == AppPage::Settings) {
        if (flow_.sessionLive()) {
            if (!confirm("End current SSH?", "Required to test this host", "End session")) return false;
            closeTerminalSession(SessionEvent::ActiveConnectionChanged);
        } else {
            flow_.apply(SessionEvent::CancelSettings);
        }
    }
    return flow_.page() == AppPage::HostLauncher && WiFi.status() == WL_CONNECTED;
}

AppController::DraftAttemptResult AppController::connectHostDraft(SshHostProfile& host, String& error) {
    if (host.enabled == 0 || !flow_.apply(SessionEvent::HostSelected)) return DraftAttemptResult::Failed;
    operationDetail_ = host.displayName;
    ui_.progress("Connecting SSH", operationDetail_, 0, 0, true);
    if (!ssh_.start(host.host, host.port, host.user, error)) {
        flow_.apply(SessionEvent::RemoteClosed);
        return DraftAttemptResult::Failed;
    }

    if (host.hostKeyTrusted != 0) {
        if (!ssh_.verifyServerKey(host.hostKeySha256, sizeof(host.hostKeySha256))) {
            ssh_.disconnect();
            flow_.apply(SessionEvent::HostKeyMismatch);
            return DraftAttemptResult::HostKeyMismatch;
        }
    } else {
        ui_.fingerprint("Trust this host?", ssh_.serverFingerprint(), "Enter Trust  Fn+` Cancel", kWarning);
        transitionInputContext(InputContext::Dialog);
        while (true) {
            const KeyEvent eventValue = waitEvent(InputContext::Dialog);
            if (eventValue.type == KeyEventType::Back) {
                ssh_.disconnect();
                flow_.apply(SessionEvent::RemoteClosed);
                return DraftAttemptResult::Canceled;
            }
            if (eventValue.type == KeyEventType::Confirm) break;
        }
        if (!ssh_.copyServerKeyHash(host.hostKeySha256, kHostKeyHashSize)) {
            ssh_.disconnect();
            flow_.apply(SessionEvent::RemoteClosed);
            error = "Cannot copy host key";
            return DraftAttemptResult::Failed;
        }
        host.hostKeyTrusted = 1;
        ssh_.trustCurrentServerKey();
    }
    flow_.apply(SessionEvent::HostVerified);

    char password[kSshPasswordCapacity]{};
    if (host.savePassword != 0) {
        copyConfigString(password, sizeof(password), host.password);
    } else {
        String entered;
        if (!inputText(host.displayName, "SSH password", entered, kSshPasswordCapacity - 1, true, false)) {
            clearSensitiveString(entered);
            secureClear(password, sizeof(password));
            ssh_.disconnect();
            flow_.apply(SessionEvent::RemoteClosed);
            return DraftAttemptResult::Canceled;
        }
        copyConfigString(password, sizeof(password), entered.c_str());
        clearSensitiveString(entered);
    }

    ui_.progress("Authenticating SSH", host.displayName, 0, 0, true);
    const bool authenticated = ssh_.authenticatePassword(password, error);
    secureClear(password, sizeof(password));
    if (!authenticated) {
        ssh_.disconnect();
        flow_.apply(SessionEvent::RemoteClosed);
        return DraftAttemptResult::Failed;
    }
    return DraftAttemptResult::Authenticated;
}

bool AppController::persistAuthenticatedHostDraft(const HostDraftTransaction& transaction,
                                                  uint8_t& resultingIndex) {
    if (!transaction.canPersist()) return false;
    const int savedIndex = transaction.savedIndex();
    if (savedIndex >= 0 && static_cast<size_t>(savedIndex) >= config_.hostCount) return false;

    ProfileConfigData updated = config_;
    const bool changed = savedIndex < 0
                             ? HostCatalog::add(updated, transaction.profile())
                             : HostCatalog::replaceVerified(updated,
                                                            static_cast<size_t>(savedIndex),
                                                            transaction.profile());
    if (!changed || !profileStore_.save(updated)) {
        secureClear(&updated, sizeof(updated));
        return false;
    }
    resultingIndex = savedIndex < 0 ? config_.hostCount : static_cast<uint8_t>(savedIndex);
    config_ = updated;
    secureClear(&updated, sizeof(updated));
    ++configGeneration_;
    return true;
}

AppController::HostEditOutcome AppController::hostManagerMenu() {
    while (true) {
        String labels[kMaxSshHostProfiles + 2];
        for (uint8_t index = 0; index < config_.hostCount; ++index)
            labels[index] = String(config_.hosts[index].enabled ? "[ON] " : "[OFF] ") + config_.hosts[index].displayName;
        labels[config_.hostCount] = "Add host...";
        labels[config_.hostCount + 1] = "Back";
        const int selected = selectMenu("Manage hosts", labels, nullptr, config_.hostCount + 2, 0);
        if (selected < 0 || selected == config_.hostCount + 1) return HostEditOutcome::Stay;
        if (selected == config_.hostCount) {
            const HostEditOutcome outcome = editHostProfile(-1, true);
            if (outcome != HostEditOutcome::Stay) return outcome;
            continue;
        }
        const size_t index = static_cast<size_t>(selected);
        const String actions[] = {"Edit", config_.hosts[index].enabled ? "Disable" : "Enable",
                                  "Move up", "Move down", "Reset trust", "Delete", "Back"};
        const int action = selectMenu(config_.hosts[index].displayName, actions, nullptr, 7, 0);
        if (action == 0) {
            const HostEditOutcome outcome = editHostProfile(selected, false);
            if (outcome != HostEditOutcome::Stay) return outcome;
            continue;
        }
        ProfileConfigData updated = config_;
        bool changed = false;
        if (action == 1) changed = HostCatalog::setEnabled(updated, index, !config_.hosts[index].enabled);
        else if (action == 2) changed = HostCatalog::moveUp(updated, index);
        else if (action == 3) changed = HostCatalog::moveDown(updated, index);
        else if (action == 4 && confirm("Reset host trust?", "Verify again next time", "Reset"))
            changed = HostCatalog::resetTrust(updated, index);
        else if (action == 5 && confirm("Delete host?", config_.hosts[index].displayName, "Delete"))
            changed = HostCatalog::remove(updated, index);
        if (changed && profileStore_.save(updated)) { config_ = updated; ++configGeneration_; }
        else if (changed) waitMessage("Save failed", "Old settings kept", "", kError);
        secureClear(&updated, sizeof(updated));
    }
    return HostEditOutcome::Stay;
}

AppController::HostEditOutcome AppController::discoverHost() {
    SshServiceInfo services[kMaxSshDiscoveryResults]{};
    operationDetail_ = "_ssh._tcp via mDNS";
    const int found = discovery_.discover(services, kMaxSshDiscoveryResults, operationCallbacks());
    if (found == -2) {
        secureClear(services, sizeof(services));
        return HostEditOutcome::Stay;
    }
    if (found <= 0) {
        waitMessage("No SSH services", "Use Add host for IP/name", "mDNS only", kWarning);
        secureClear(services, sizeof(services));
        return HostEditOutcome::Stay;
    }
    String labels[kMaxSshDiscoveryResults];
    String details[kMaxSshDiscoveryResults];
    for (int index = 0; index < found; ++index) {
        labels[index] = services[index].name;
        details[index] = String(services[index].target) + ":" + services[index].port;
    }
    const int selected = selectMenu("Discovered SSH", labels, details, static_cast<size_t>(found), 0);
    HostEditOutcome outcome = HostEditOutcome::Stay;
    if (selected >= 0) outcome = editHostProfile(-1, true, &services[selected]);
    secureClear(services, sizeof(services));
    return outcome;
}

AppController::SshAttemptResult AppController::connectHostProfile(uint8_t index, String& error) {
    if (index >= config_.hostCount || config_.hosts[index].enabled == 0) return SshAttemptResult::Failed;
    flow_.apply(SessionEvent::HostSelected);
    SshHostProfile& host = config_.hosts[index];
    operationDetail_ = host.displayName;
    ui_.progress("Connecting SSH", operationDetail_, 0, 0, true);
    if (!ssh_.start(host.host, host.port, host.user, error)) {
        flow_.apply(SessionEvent::RemoteClosed);
        return SshAttemptResult::Failed;
    }

    if (host.hostKeyTrusted != 0) {
        if (!ssh_.verifyServerKey(host.hostKeySha256, sizeof(host.hostKeySha256))) {
            ssh_.disconnect();
            flow_.apply(SessionEvent::HostKeyMismatch);
            return SshAttemptResult::HostKeyMismatch;
        }
    } else {
        ui_.fingerprint("Trust this host?", ssh_.serverFingerprint(), "Enter Trust  Fn+` Cancel", kWarning);
        transitionInputContext(InputContext::Dialog);
        while (true) {
            const KeyEvent eventValue = waitEvent(InputContext::Dialog);
            if (eventValue.type == KeyEventType::Back) {
                ssh_.disconnect(); flow_.apply(SessionEvent::RemoteClosed); return SshAttemptResult::Canceled;
            }
            if (eventValue.type == KeyEventType::Confirm) break;
        }
        ProfileConfigData updated = config_;
        if (!ssh_.copyServerKeyHash(updated.hosts[index].hostKeySha256, kHostKeyHashSize)) {
            ssh_.disconnect(); flow_.apply(SessionEvent::RemoteClosed); error = "Cannot copy host key";
            secureClear(&updated, sizeof(updated)); return SshAttemptResult::Failed;
        }
        updated.hosts[index].hostKeyTrusted = 1;
        finalizeProfileConfig(updated);
        if (!validateProfileConfig(updated) || !profileStore_.save(updated)) {
            ssh_.disconnect(); flow_.apply(SessionEvent::RemoteClosed); error = "Cannot save host trust";
            secureClear(&updated, sizeof(updated)); return SshAttemptResult::Failed;
        }
        config_ = updated;
        secureClear(&updated, sizeof(updated));
        ++configGeneration_;
        ssh_.trustCurrentServerKey();
    }
    flow_.apply(SessionEvent::HostVerified);

    char password[kSshPasswordCapacity]{};
    if (host.savePassword != 0) {
        copyConfigString(password, sizeof(password), host.password);
    } else {
        String entered;
        if (!inputText(host.displayName, "SSH password", entered, kSshPasswordCapacity - 1, true, false)) {
            clearSensitiveString(entered); secureClear(password, sizeof(password)); ssh_.disconnect();
            flow_.apply(SessionEvent::RemoteClosed); return SshAttemptResult::Canceled;
        }
        copyConfigString(password, sizeof(password), entered.c_str());
        clearSensitiveString(entered);
    }
    ui_.progress("Authenticating SSH", host.displayName, 0, 0, true);
    const bool authenticated = ssh_.authenticatePassword(password, error);
    secureClear(password, sizeof(password));
    if (!authenticated) {
        ssh_.disconnect(); flow_.apply(SessionEvent::RemoteClosed); return SshAttemptResult::Failed;
    }
    flow_.apply(SessionEvent::Authenticated);
    activeHostIndex_ = index;
    resetTerminalSessionState(true);
    return SshAttemptResult::Success;
}

void AppController::settingsMenu() {
    const bool wasLive = flow_.sessionLive();
    const uint32_t generation = configGeneration_;
    flow_.apply(SessionEvent::OpenSettings);
    while (true) {
        const String labels[] = {"Manage Wi-Fi", "Manage SSH hosts", "Diagnostics", "About", "Back"};
        const int action = selectMenu("Settings", labels, nullptr, 5, 0);
        if (action < 0 || action == 4) break;
        if (action == 0) wifiManagerMenu();
        else if (action == 1) {
            const HostEditOutcome outcome = hostManagerMenu();
            if (outcome != HostEditOutcome::Stay) return;
        }
        else if (action == 2) diagnosticsPage();
        else if (action == 3)
            waitMessage("Cardputer SSH", "Generic SSH client", "4 Wi-Fi / 8 hosts", kAccent);
    }
    if (wasLive && generation != configGeneration_) {
        closeTerminalSession(SessionEvent::ActiveConnectionChanged);
    } else {
        flow_.apply(SessionEvent::CancelSettings);
    }
}

void AppController::diagnosticsPage() {
    DiagnosticsSnapshot snapshotValue{};
    snapshotValue.scanNamed = lastScanSummary_.namedCount;
    snapshotValue.scanHidden = lastScanSummary_.hiddenCount;
    snapshotValue.scanFailed = lastScanSummary_.failedChannels;
    snapshotValue.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    snapshotValue.wifiChannel = WiFi.status() == WL_CONNECTED ? static_cast<uint8_t>(WiFi.channel()) : 0;
    const KeyboardStatus& keys = keyboard_.status();
    if (keys.hasLastPressed) {
        snapshotValue.rawKeyRow = keys.lastPressed / kKeyboardColumns;
        snapshotValue.rawKeyColumn = keys.lastPressed % kKeyboardColumns;
    }
    snapshotValue.fnHeld = keys.fnHeld; snapshotValue.aaHeld = keys.aaHeld; snapshotValue.aaLocked = keys.aaLocked;
    snapshotValue.ctrlHeld = keys.ctrlHeld; snapshotValue.altHeld = keys.altHeld; snapshotValue.optHeld = keys.optHeld;
    snapshotValue.configValid = validateProfileConfig(config_);
    snapshotValue.wifiCount = config_.wifiCount; snapshotValue.hostCount = config_.hostCount;
    snapshotValue.freeHeap = ESP.getFreeHeap(); snapshotValue.page = static_cast<uint8_t>(flow_.page());
    snapshotValue.sessionLive = flow_.sessionLive();
    String lines[kDiagnosticLineCount + 1];
    for (size_t index = 0; index < kDiagnosticLineCount; ++index) {
        char text[64]{};
        formatDiagnosticLine(snapshotValue, index, text, sizeof(text));
        lines[index] = text;
    }
    lines[kDiagnosticLineCount] = "Back";
    selectMenu("Diagnostics", lines, nullptr, kDiagnosticLineCount + 1, 0);
}

void AppController::renderTerminal(bool force) {
    if (!flow_.sessionLive()) return;
    const uint32_t now = millis();
    const bool localMode = flow_.mode() == TerminalMode::Local;
    const KeyboardStatus& keyboardStatus = keyboard_.status();
    TerminalModifierState modifiers;
    modifiers.aaHeld = keyboardStatus.aaHeld;
    modifiers.aaLocked = keyboardStatus.aaLocked;
    modifiers.ctrlHeld = keyboardStatus.ctrlHeld;
    modifiers.altHeld = keyboardStatus.altHeld;
    modifiers.optHeld = keyboardStatus.optHeld;

    const bool shortcutsVisible =
        terminalUi_.shortcutsVisible(now, modifiers.optHeld);
    const bool localHasNewOutput = terminalShowNew(
        localMode, terminalViewport_.alternateSnapshot(),
        terminalViewport_.hasNewOutput());
    const TerminalCursor& cursor = terminal_.activeScreen().cursor();
    if (localMode || !cursor.visible) {
        cursorBlinkAt_ = now;
        cursorBlinkOn_ = true;
    } else if (now - cursorBlinkAt_ >= kCursorBlinkIntervalMs) {
        cursorBlinkAt_ = now;
        cursorBlinkOn_ = !cursorBlinkOn_;
    }
    const bool renderedCursorBlink =
        !localMode && cursor.visible && cursorBlinkOn_;

    uint8_t uiFlags = 0;
    if (modifiers.aaHeld) uiFlags |= 0x01;
    if (modifiers.aaLocked) uiFlags |= 0x02;
    if (modifiers.ctrlHeld) uiFlags |= 0x04;
    if (modifiers.altHeld) uiFlags |= 0x08;
    if (modifiers.optHeld) uiFlags |= 0x10;
    if (localMode) uiFlags |= 0x20;
    if (localHasNewOutput) uiFlags |= 0x40;

    const bool uiChanged =
        !terminalUiCached_ || uiFlags != lastTerminalUiFlags_ ||
        shortcutsVisible != lastShortcutsVisible_ ||
        renderedCursorBlink != lastRenderedCursorBlinkOn_;
    const bool screenChanged = terminal_.dirtyRows() != 0;
    if (!force && !terminalDirty_ && !screenChanged && !uiChanged) return;
    if (!force && now - lastFrameAt_ < kFrameIntervalMs) return;
    if (force) {
        ui_.invalidateTerminal();
        terminal_.invalidateAll();
    }

    lastFrameAt_ = now;
    const String host = activeHostIndex_ >= 0 && static_cast<size_t>(activeHostIndex_) < config_.hostCount
                            ? String(config_.hosts[activeHostIndex_].displayName) : String("SSH");
    UiStatus status;
    status.host = host.c_str();
    status.modifier = terminalModifierLabel(modifiers);
    status.rssi = WiFi.RSSI();
    const int32_t battery = M5Cardputer.Power.getBatteryLevel();
    status.batteryPercent = battery < 0 ? 0 : static_cast<uint8_t>(battery > 100 ? 100 : battery);
    status.localHasNewOutput = terminalViewport_.hasNewOutput();
    ui_.terminal(status, terminal_, terminalViewport_, localMode,
                 ssh_.hostKeyVerified(), shortcutsVisible, cursorBlinkOn_);
    terminal_.clearDirtyRows();
    terminalDirty_ = false;
    terminalUiCached_ = true;
    lastTerminalUiFlags_ = uiFlags;
    lastShortcutsVisible_ = shortcutsVisible;
    lastRenderedCursorBlinkOn_ = renderedCursorBlink;
}

void AppController::handleTerminalEvent(const KeyEvent& eventValue) {
    if (eventValue.type == KeyEventType::EndSession) {
        flow_.apply(SessionEvent::RequestEnd);
        if (confirm("End SSH session?", "Wi-Fi stays connected", "End session")) {
            flow_.apply(SessionEvent::ConfirmEnd);
            endSessionToLauncher();
        } else {
            flow_.apply(SessionEvent::CancelEnd);
            transitionInputContext(flow_.mode() == TerminalMode::Remote ? InputContext::RemoteTerminal
                                                                        : InputContext::LocalScroll);
            renderTerminal(true);
        }
        return;
    }
    if (eventValue.type == KeyEventType::OpenSettings) {
        settingsMenu();
        terminalDirty_ = true;
        if (flow_.sessionLive()) {
            transitionInputContext(flow_.mode() == TerminalMode::Remote ? InputContext::RemoteTerminal
                                                                        : InputContext::LocalScroll);
            renderTerminal(true);
        }
        return;
    }
    if (eventValue.type == KeyEventType::ToggleTerminalMode) {
        if (!flow_.apply(SessionEvent::ToggleTerminalMode)) return;
        const uint32_t now = millis();
        if (flow_.mode() == TerminalMode::Local) {
            terminalViewport_.enterLocal(terminal_);
        } else {
            terminalViewport_.leaveLocal();
            terminal_.clearDirtyRows();
            terminal_.invalidateAll();
        }
        terminalUi_.showShortcutsAt(now);
        cursorBlinkAt_ = now;
        cursorBlinkOn_ = true;
        transitionInputContext(flow_.mode() == TerminalMode::Remote ? InputContext::RemoteTerminal
                                                                    : InputContext::LocalScroll);
        terminalDirty_ = true;
        terminalUiCached_ = false;
        return;
    }
    if (flow_.mode() == TerminalMode::Local) {
        bool viewportChanged = true;
        if (eventValue.type == KeyEventType::MoveUp)
            terminalViewport_.scrollLines(1, terminal_);
        else if (eventValue.type == KeyEventType::MoveDown)
            terminalViewport_.scrollLines(-1, terminal_);
        else if (eventValue.type == KeyEventType::MoveLeft)
            terminalViewport_.scrollPages(1, terminal_);
        else if (eventValue.type == KeyEventType::MoveRight)
            terminalViewport_.scrollPages(-1, terminal_);
        else if (eventValue.type == KeyEventType::JumpBottom)
            terminalViewport_.jumpToBottom();
        else
            viewportChanged = false;
        if (viewportChanged) terminalDirty_ = true;
        return;
    }
    if (!flow_.sshWriteAllowed()) return;
    const EncodedKey encoded =
        TerminalKeyEncoder::encode(eventValue, terminal_.inputModes());
    if (encoded.length > 0 && !ssh_.write(reinterpret_cast<const char*>(encoded.bytes), encoded.length)) {
        closeTerminalSession(SessionEvent::RemoteClosed);
    }
}

void AppController::readSshOutput() {
    if (!flow_.sessionLive() || !ssh_.connected()) return;
    char buffer[256];
    for (int pass = 0; pass < 8; ++pass) {
        const SshReadResult readResult = ssh_.read(buffer, sizeof(buffer));
        if (sshReadTerminatesSession(readResult.status)) {
            closeTerminalSession(SessionEvent::RemoteClosed);
            return;
        }
        if (readResult.status == SshReadStatus::WouldBlock) break;
        terminal_.feed(reinterpret_cast<const uint8_t*>(buffer),
                       readResult.length);
        terminalViewport_.onTerminalChanged(terminal_);

        TerminalReply reply;
        while (terminal_.popReply(reply)) {
            if (!ssh_.write(reinterpret_cast<const char*>(reply.bytes),
                            reply.length)) {
                closeTerminalSession(SessionEvent::RemoteClosed);
                return;
            }
        }
        if (terminal_.replyOverflowed()) {
            closeTerminalSession(SessionEvent::RemoteClosed);
            return;
        }
        terminalDirty_ = terminalDirty_ || terminal_.dirtyRows() != 0;
    }
}

void AppController::serviceBackgroundSession() {
    if (flow_.sessionLive() && ssh_.connected()) readSshOutput();
}

void AppController::endSessionToLauncher() {
    ssh_.disconnect();
    activeHostIndex_ = -1;
    resetTerminalSessionState(false);
}

void AppController::resetTerminalSessionState(bool showShortcuts) {
    terminal_.reset();
    terminalViewport_.leaveLocal();
    terminalUi_ = TerminalUiModel{};
    const uint32_t now = millis();
    if (showShortcuts) terminalUi_.showShortcutsAt(now);
    cursorBlinkAt_ = now;
    cursorBlinkOn_ = true;
    terminalDirty_ = true;
    terminalUiCached_ = false;
    ui_.invalidateTerminal();
}

void AppController::closeTerminalSession(SessionEvent reason) {
    ssh_.disconnect();
    flow_.apply(reason);
    activeHostIndex_ = -1;
    resetTerminalSessionState(false);
}

String AppController::formatFingerprint(const uint8_t* hash, size_t length) {
    if (hash == nullptr || length == 0) return "";
    unsigned char encoded[64]{};
    size_t encodedLength = 0;
    if (mbedtls_base64_encode(encoded, sizeof(encoded) - 1, &encodedLength, hash, length) != 0) return "";
    while (encodedLength > 0 && encoded[encodedLength - 1] == '=') --encodedLength;
    encoded[encodedLength] = '\0';
    return "SHA256:" + String(reinterpret_cast<const char*>(encoded));
}

String AppController::clipped(const String& value, size_t maximum) {
    if (maximum == 0 || value.isEmpty()) return "";
    if (value.length() <= maximum) return value;
    return value.substring(0, maximum - 1) + "~";
}

}  // namespace app

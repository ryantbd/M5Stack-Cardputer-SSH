#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "AppConfig.h"
#include "Diagnostics.h"
#include "HostCatalog.h"
#include "HostDraftTransaction.h"
#include "KeyboardRouter.h"
#include "NvsConfigStorage.h"
#include "ProfileStore.h"
#include "SessionFlow.h"
#include "SshClient.h"
#include "SshDiscovery.h"
#include "TerminalEmulator.h"
#include "TerminalKeyEncoder.h"
#include "TerminalUiModel.h"
#include "TerminalViewport.h"
#include "TextEditModel.h"
#include "UiRenderer.h"
#include "WifiManager.h"

namespace app {

class AppController {
public:
    void begin();
    void loop();

private:
    enum class SshAttemptResult : uint8_t { Success, Canceled, HostKeyMismatch, Failed };
    enum class HostEditOutcome : uint8_t { Stay, ReturnToLauncher, Connected };
    enum class DraftAttemptResult : uint8_t { Authenticated, Canceled, HostKeyMismatch, Failed };

    static bool operationCanceledThunk(void* context);
    static void operationProgressThunk(const char* stage, uint16_t current, uint16_t total, void* context);

    OperationCallbacks operationCallbacks();
    bool operationCanceled();
    void operationProgress(const char* stage, uint16_t current, uint16_t total);
    bool pollEvent(InputContext context, KeyEvent& event);
    KeyEvent waitEvent(InputContext context);
    void transitionInputContext(InputContext context);
    int selectMenu(const String& title, const String* labels, const String* details,
                   size_t count, size_t initial, const String& step = "");
    bool inputText(const String& title, const String& label, String& value, size_t maximum,
                   bool masked, bool allowEmpty, const String& step = "");
    bool confirm(const String& title, const String& detail, const String& acceptLabel);
    void waitMessage(const String& title, const String& line1, const String& line2, uint16_t color);

    bool bootRecoveryRequested();
    void recoveryMenu();
    bool loadConfiguration();
    bool persistConfiguration(const char* failureTitle = "Save failed");
    int ensureWifiConfigured();
    bool connectConfiguredWifi();
    NetworkResult connectWifiProfile(uint8_t index);
    void wifiConnectionFlow(int directIndex = -1);
    int editWifiProfile(int index, bool creating, bool passwordOnly = false);
    void wifiManagerMenu();

    void hostLauncher();
    HostEditOutcome editHostProfile(int index, bool creating, const SshServiceInfo* seed = nullptr);
    bool editHostDraftFields(SshHostProfile& host);
    bool prepareHostDraftAttempt();
    DraftAttemptResult connectHostDraft(SshHostProfile& host, String& error);
    bool persistAuthenticatedHostDraft(const HostDraftTransaction& transaction, uint8_t& resultingIndex);
    HostEditOutcome hostManagerMenu();
    HostEditOutcome discoverHost();
    SshAttemptResult connectHostProfile(uint8_t index, String& error);
    void settingsMenu();
    void diagnosticsPage();

    void renderTerminal(bool force = false);
    void handleTerminalEvent(const KeyEvent& event);
    void readSshOutput();
    void serviceBackgroundSession();
    void endSessionToLauncher();
    void resetTerminalSessionState(bool showShortcuts);
    void closeTerminalSession(SessionEvent reason);

    static String formatFingerprint(const uint8_t* hash, size_t length);
    static String clipped(const String& value, size_t maximum);

    UiRenderer ui_;
    NvsConfigStorage configStorage_;
    ProfileStore profileStore_{configStorage_};
    WifiManager wifi_;
    SshDiscovery discovery_;
    SshClient ssh_;
    TerminalEmulator terminal_;
    TerminalViewport terminalViewport_;
    TerminalUiModel terminalUi_;
    KeyboardRouter keyboard_;
    ProfileConfigData config_{};
    SessionFlow flow_;
    WifiScanSummary lastScanSummary_{};

    String operationDetail_;
    int activeWifiIndex_ = -1;
    int activeHostIndex_ = -1;
    uint32_t configGeneration_ = 0;
    uint32_t lastFrameAt_ = 0;
    uint32_t cursorBlinkAt_ = 0;
    uint8_t lastTerminalUiFlags_ = 0;
    bool terminalDirty_ = true;
    bool cursorBlinkOn_ = true;
    bool terminalUiCached_ = false;
    bool lastShortcutsVisible_ = false;
    bool lastRenderedCursorBlinkOn_ = true;
};

}  // namespace app

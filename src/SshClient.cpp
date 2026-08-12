#include "SshClient.h"

#include <libssh_esp32.h>
#include <string.h>

#include "TerminalGeometry.h"

namespace {

constexpr uint32_t kWriteTimeoutMs = 5000;

String sshError(ssh_session session, const char* fallback) {
    if (session == nullptr) {
        return String(fallback);
    }

    const char* detail = ssh_get_error(session);
    if (detail == nullptr || detail[0] == '\0') {
        return String(fallback);
    }
    return String(fallback) + ": " + detail;
}

}  // namespace

SshClient::~SshClient() {
    disconnect();
}

bool SshClient::start(const char* host, uint16_t port, const char* user, String& error) {
    disconnect();
    error = "";

    if (host == nullptr || host[0] == '\0' || user == nullptr || user[0] == '\0' || port == 0) {
        error = "Invalid SSH target";
        return false;
    }

    libssh_begin();
    session_ = ssh_new();
    if (session_ == nullptr) {
        error = "Cannot allocate SSH session";
        return false;
    }

    unsigned int sshPort = port;
    long timeoutSeconds = 12;
    int verbosity = SSH_LOG_NOLOG;
    if (ssh_options_set(session_, SSH_OPTIONS_HOST, host) != SSH_OK ||
        ssh_options_set(session_, SSH_OPTIONS_PORT, &sshPort) != SSH_OK ||
        ssh_options_set(session_, SSH_OPTIONS_USER, user) != SSH_OK ||
        ssh_options_set(session_, SSH_OPTIONS_TIMEOUT, &timeoutSeconds) != SSH_OK ||
        ssh_options_set(session_, SSH_OPTIONS_LOG_VERBOSITY, &verbosity) != SSH_OK) {
        error = sshError(session_, "Cannot configure SSH session");
        disconnect();
        return false;
    }

    if (ssh_connect(session_) != SSH_OK) {
        error = sshError(session_, "SSH connection failed");
        disconnect();
        return false;
    }

    if (!readServerFingerprint(error)) {
        disconnect();
        return false;
    }
    return true;
}

bool SshClient::authenticatePassword(const char* password, String& error) {
    error = "";
    if (!sessionConnected() || !hostKeyVerified() || password == nullptr || password[0] == '\0') {
        error = "SSH session, host trust, or password invalid";
        return false;
    }

    const int authResult = ssh_userauth_password(session_, nullptr, password);
    if (authResult != SSH_AUTH_SUCCESS) {
        error = sshError(session_, "SSH authentication failed");
        disconnect();
        return false;
    }

    channel_ = ssh_channel_new(session_);
    if (channel_ == nullptr) {
        error = "Cannot allocate SSH channel";
        disconnect();
        return false;
    }

    if (ssh_channel_open_session(channel_) != SSH_OK) {
        error = sshError(session_, "Cannot open SSH channel");
        disconnect();
        return false;
    }

    if (ssh_channel_request_pty_size(channel_, "xterm-256color",
                                     app::kTerminalColumns,
                                     app::kTerminalRows) != SSH_OK) {
        error = sshError(session_, "Cannot request terminal");
        disconnect();
        return false;
    }

    if (ssh_channel_request_shell(channel_) != SSH_OK) {
        error = sshError(session_, "Cannot start remote shell");
        disconnect();
        return false;
    }

    ssh_set_blocking(session_, 0);
    return true;
}

bool SshClient::verifyServerKey(const uint8_t* expectedHash, size_t length) {
    if (expectedHash == nullptr || length != fingerprintHashLength_ || length != sizeof(fingerprintHash_)) {
        verified_ = false;
        return false;
    }

    uint8_t difference = 0;
    for (size_t index = 0; index < length; ++index) {
        difference |= fingerprintHash_[index] ^ expectedHash[index];
    }
    verified_ = difference == 0;
    return verified_;
}

void SshClient::trustCurrentServerKey() {
    verified_ = fingerprintHashLength_ == sizeof(fingerprintHash_);
}

bool SshClient::copyServerKeyHash(uint8_t* destination, size_t length) const {
    if (destination == nullptr || length != fingerprintHashLength_ || length != sizeof(fingerprintHash_)) {
        return false;
    }
    memcpy(destination, fingerprintHash_, length);
    return true;
}

void SshClient::disconnect() {
    if (channel_ != nullptr) {
        ssh_channel_send_eof(channel_);
        ssh_channel_close(channel_);
        ssh_channel_free(channel_);
        channel_ = nullptr;
    }

    if (session_ != nullptr) {
        ssh_disconnect(session_);
        ssh_free(session_);
        session_ = nullptr;
    }

    memset(fingerprintHash_, 0, sizeof(fingerprintHash_));
    fingerprintHashLength_ = 0;
    fingerprint_ = "";
    verified_ = false;
}

bool SshClient::connected() const {
    return session_ != nullptr && channel_ != nullptr && ssh_is_connected(session_) &&
           !ssh_channel_is_closed(channel_) && !ssh_channel_is_eof(channel_);
}

bool SshClient::sessionConnected() const {
    return session_ != nullptr && ssh_is_connected(session_);
}

app::SshReadResult SshClient::read(char* buffer, size_t length) {
    if (channel_ == nullptr || buffer == nullptr || length == 0) {
        return {app::SshReadStatus::Error, 0};
    }
    const int result = ssh_channel_read_nonblocking(
        channel_, buffer, static_cast<uint32_t>(length), 0);
    return app::classifySshReadResult(result, length, SSH_AGAIN, SSH_EOF);
}

bool SshClient::write(const char* data, size_t length) {
    if (!connected() || data == nullptr || length == 0) {
        return false;
    }

    const uint32_t startedAt = millis();
    size_t sent = 0;
    while (sent < length) {
        const int result = ssh_channel_write(channel_, data + sent, length - sent);
        if (result == SSH_AGAIN) {
            if (millis() - startedAt >= kWriteTimeoutMs) {
                return false;
            }
            delay(1);
            continue;
        }
        if (result <= 0) {
            return false;
        }
        sent += static_cast<size_t>(result);
    }
    return true;
}

bool SshClient::write(const String& data) {
    return write(data.c_str(), data.length());
}

const String& SshClient::serverFingerprint() const {
    return fingerprint_;
}

bool SshClient::hostKeyVerified() const {
    return verified_;
}

bool SshClient::readServerFingerprint(String& error) {
    ssh_key serverKey = nullptr;
    unsigned char* hash = nullptr;
    size_t hashLength = 0;
    char* printable = nullptr;

    if (ssh_get_server_publickey(session_, &serverKey) != SSH_OK) {
        error = sshError(session_, "Cannot read server host key");
        return false;
    }

    if (ssh_get_publickey_hash(serverKey, SSH_PUBLICKEY_HASH_SHA256, &hash, &hashLength) != SSH_OK) {
        ssh_key_free(serverKey);
        error = sshError(session_, "Cannot hash server host key");
        return false;
    }

    if (hashLength != sizeof(fingerprintHash_)) {
        ssh_clean_pubkey_hash(&hash);
        ssh_key_free(serverKey);
        error = "Unexpected host-key hash length";
        return false;
    }
    memcpy(fingerprintHash_, hash, hashLength);
    fingerprintHashLength_ = hashLength;

    printable = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hashLength);
    if (printable == nullptr) {
        ssh_clean_pubkey_hash(&hash);
        ssh_key_free(serverKey);
        error = "Cannot format server host key";
        return false;
    }

    fingerprint_ = printable;
    ssh_string_free_char(printable);
    ssh_clean_pubkey_hash(&hash);
    ssh_key_free(serverKey);
    return true;
}

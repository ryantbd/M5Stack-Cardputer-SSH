# Architecture

```text
M5Cardputer ADV                         SSH server
+---------------------------+          +-----------------------+
| semantic keyboard + LCD   |  Wi-Fi  | OpenSSH server        |
| profiles + NVS + history  |---------| PTY + remote shell    |
| LibSSH-ESP32 client       |  SSH    | No companion service  |
| WireGuard (optional)      |  (VPN)  |                       |
+---------------------------+          +-----------------------+
```

## Components

- `AppController` owns setup, recovery, profile management, the host launcher,
  the REMOTE/LOCAL terminal flow, and the USB serial setup commands (`keys`,
  `wifi-add`, `host-add`, `vpn-import`). Wi-Fi startup stops at the launcher
  and SSH begins only after the user selects a host.
- `KeyboardRouter` is the only normal runtime component that interprets the
  M5Cardputer keyboard matrix. It emits bounded semantic events, applies chord
  precedence, key repeat, and release gating across page transitions.
- `TextEditModel` edits caller-owned fixed-capacity buffers with a cursor,
  backward/forward deletion, masked slices, and explicit sensitive clearing.
- `TerminalKeyEncoder` converts only REMOTE events into fixed byte sequences.
  Global and LOCAL events encode to zero bytes.
- `ProfileStore` persists a CRC-protected V2 blob containing up to four Wi-Fi
  profiles and eight generic SSH hosts, and safely migrates the legacy V1 key.
- `WifiManager` scans channels 1–13 synchronously; `WifiProfileSelector` orders
  visible/hidden fallback candidates without dynamic allocation.
- `SshDiscovery` imports at most eight generic `_ssh._tcp` mDNS services. It
  performs no Pi-specific hostname fallback or `/24` port scan.
- `SessionFlow` makes launcher, connection, REMOTE/LOCAL, Settings, end-session,
  and network recovery transitions explicit.
- `Diagnostics` formats numeric runtime state without credentials or profile
  blobs.
- `SshClient` separates fingerprint retrieval from authentication, asks the
  server which methods it offers, tries the device key before a password,
  requests a fixed 40×13 `xterm-256color` PTY, and uses typed non-blocking
  channel reads. It also generates and describes the device's Ed25519 key.
- `DeviceIdentity` holds the device's OpenSSH-format Ed25519 private key in a
  CRC-protected `identity` blob. Only the public half is ever shown or printed.
- `VpnConfig` stores the single WireGuard peer in a CRC-protected `vpn` blob
  and parses wg-quick text; a missing `PrivateKey` keeps the device's own key.
- `WireGuardTunnel` drives WireGuard-ESP32's `wireguardif` from lwIP's thread.
  The interface netmask comes from `AllowedIPs`, so only that subnet is routed
  through it (unless it is `0.0.0.0/0`), and it sets a persistent keepalive.
- `UiRenderer` draws every screen through one 8-bit 240 x 135 sprite.
- `TerminalEmulator` incrementally decodes bounded UTF-8 and xterm control
  sequences into fixed primary and alternate `TerminalScreen` instances. It
  owns cursor/input modes, a bounded terminal-reply queue, dirty rows, and the
  120-row `TerminalHistory` ring.
- `TerminalViewport` combines primary history with the live screen in LOCAL
  and owns the fixed alternate-screen inspection snapshot. It never writes to
  SSH.

## Input and terminal flow

```text
4x14 coordinates -> KeyboardRouter -> semantic KeyEvent
                                  | REMOTE -> TerminalKeyEncoder -> SSH write
                                  | LOCAL  -> TerminalViewport    -> no write
                                  | setup  -> menu/text/dialog actions
```

Every physical coordinate has independent press/release edges. `Fn`, `Aa`,
`Ctrl`, `Alt`, and `Opt` are resolved before non-modifier edges, so holding a
modifier does not suppress a later key. A short unused Aa tap locks the alternate
layer; a held Aa applies it temporarily. Directional keys repeat after 350 ms
and then every 150 ms. Backward/forward deletion repeats only in text fields
and REMOTE. Context transitions suppress input until the full matrix releases.

REMOTE writes printable characters immediately and lets the remote SSH
shell handle line editing, history, and Tab completion. LOCAL continues reading
the SSH channel but only accepts scroll, bottom, Settings, and mode-toggle
events. Switching back to REMOTE jumps to the newest output.

## Configuration and security

The firmware contains no compile-time credential header. Setup builds a
candidate configuration in bounded RAM, tests Wi-Fi, displays the negotiated
SHA-256 host-key fingerprint, obtains explicit trust, and tests SSH login. Only
then does `ProfileStore` verify the complete write by reading it back and
validating its CRC.

The initial connection is TOFU, unless `host-add` pinned the key over USB.
Later connections compare the raw 32-byte hash in constant time before any
authentication. A mismatch is a hard stop and no credentials are sent. Settings edits use a
temporary copy; failed validation or NVS writes leave the persisted config
unchanged.

Credentials are stored in ordinary ESP32 NVS, not a secure element. That
includes the device's SSH and WireGuard private keys, which are generated on
the device and never exported. Physical flash extraction remains a threat, so a
dedicated unprivileged remote account without passwordless sudo is recommended,
and a lost device's public keys should be removed from its servers.

The WireGuard tunnel starts only once NTP has set the clock: handshakes carry a
TAI64N timestamp, and a peer rejects one older than the last it accepted, so an
unset clock would fail every handshake after the first.

### Profile configuration migration boundary

The bounded V2 model stores up to four Wi-Fi profiles and eight SSH hosts in
the `config-v2` key. On first V2 load, `ProfileStore` can import the original
single-profile `config` blob, write `config-v2`, and accept it only after a
complete read-back and CRC check. Migration never overwrites or removes the
legacy key, so a failed migration leaves the previous firmware usable.

The generic host launcher now uses V2 directly. Ordinary firmware uploads do
not erase NVS; only the explicit Clear All action removes the configuration
keys, together with the `identity` and `vpn` blobs.

## Bounds and verified build

- Wi-Fi timeout: 15 seconds; SSH handshake timeout: 12 seconds.
- Wi-Fi results: 20; SSH mDNS imports: 8; no address-range port scan.
- Keyboard snapshot: 56 bits; semantic queue: 8 events; per-key repeat state.
- Terminal: 40 columns, 13 visible rows, 120 completed scrollback lines.
- Framebuffer: one 8-bit 240 x 135 sprite (about 32.4 KB).
- VPN: one IPv4 peer; wg-quick text up to 2,048 bytes; NTP wait 15 seconds;
  on-screen USB import times out after 120 seconds.
- Strict ADV build: 97,644 bytes RAM (29.8%) and 1,347,589 bytes Flash (40.3%).

All project C++ sources compile with `-Wall -Wextra -Werror`. The native CI
suite covers configuration/migration, scanning/selection, physical keyboard
routing, session flow, diagnostics, terminal encoding, scroll arithmetic, wg-quick
parsing, and the identity and VPN stores.
Third-party LibSSH-ESP32 C sources
currently emit upstream const/deprecation warnings outside the project-source
warning policy.

## Limitations

The terminal implements a bounded VT100/xterm subset for Bash, Vim, nano, top,
htop, and tmux. It intentionally omits CJK/double-width layout, combining and
bidirectional text, mouse protocols, terminal graphics, OSC clipboard, and
live PTY resize. SSH login uses a password or the device's own Ed25519 key;
importing other private keys and SSH agents are not supported. The VPN supports
one IPv4 WireGuard peer.

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.0] - 2026-01-13

### ✨ Added
- **Captive Portal v2**: Robust auto-popup support for iOS 14+ and Android 11+.
  - **Strict DNS Strategy**: Explicitly answers A-Records (IPv4) with Gateway IP while rejecting AAAA (IPv6) queries with "No Data". This forces modern OSes to fallback to IPv4 instead of failing silently.
  - **"Serve Everywhere" HTTP**: Returns Portal HTML (200 OK) for *any* requested URL (e.g. `/generate_204`, `apple.com`), ensuring compatibility with restrictive clients that block Redirects.
- **Verbose Logging**: Added detailed QTYPE/QNAME logging for DNS troubleshooting.

### 🔧 Changed
- **DNS Server**: Switched from "Hijack All" to "Strict QTYPE Parsing" to respect DNS protocol standards.
- **HTTP Handlers**: Removed 302 Redirect logic; `index_handler` now serves content directly for all Host headers.
- **Project Structure**: Bumped version to `1.4.0`.

### 🐛 Fixed
- **Crash/Warning**: Fixed "no slots left for registering handler" by increasing `max_uri_handlers` to 24.
- **mDNS**: Improved stability during SoftAP <-> Station transitions.

## [1.3.0] - 2026-01-08
### Added
- AHT20 Sensor Support with I2C driver.
- iOS HomeKit Integration (HAP) with dedicated event handling.
- OTA History & Rollback support.

### Changed
- Refactored Web UI to be togglable via RainMaker.
- Moved WiFi provisioning to RainMaker-first approach.

## [1.2.0] - 2026-01-05
### Added
- AC State Persistence (NVS).
- Custom Brand Dropdown in Web UI.

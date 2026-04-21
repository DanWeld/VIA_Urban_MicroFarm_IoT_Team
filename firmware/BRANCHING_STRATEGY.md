# Git Branching Strategy - IoT Team

## Overview

This document defines the Git branching strategy for the **VIA Urban MicroFarm IoT Team**. We follow **Git Flow** with C/MQTT/Mosquitto firmware development practices.

**Repository**: https://github.com/DanWeld/VIA_Urban_MicroFarm_IoT_Team

---

##  Branch Structure

### Main Branches (Protected)

#### `main` - Production Release
- **Purpose**: Stable, tested firmware releases for deployment
- **Protection Rules**:
  - ✅ Require pull request reviews (min 1 reviewer)
  - ✅ Require status checks to pass (CI/CD pipeline)
  - ✅ Require branches to be up to date
  - ✅ Dismiss stale pull request approvals
  - ✅ Block direct pushes (all changes via PR)
- **Merging**: Only from `release/*` or `hotfix/*` branches
- **Tag**: Create semantic version tags (v1.0.0, v1.0.1, etc.)

#### `develop` - Integration Branch
- **Purpose**: Main development branch where features are integrated and tested
- **Protection Rules**:
  - ✅ Require pull request reviews (min 1 reviewer)
  - ✅ Require status checks to pass
  - ✅ Require branches to be up to date
  - ✅ Block direct pushes
- **Merging**: From `feature/*`, `bugfix/*`, and `test/*` branches
- **Status**: Should always be in working/buildable state

### Support Branches (Temporary)

#### `feature/*` - Feature Development
- **Naming**: `feature/mqtt-publish-sensor-data`, `feature/wifi-reconnect`
- **Created from**: `develop`
- **Merged back to**: `develop`
- **Deleted after**: Merge to `develop`
- **Purpose**: New features, sensor drivers, MQTT improvements

**Example**:
```bash
git checkout develop
git pull origin develop
git checkout -b feature/mqtt-publish-sensor-data
```

#### `bugfix/*` - Bug Fixes
- **Naming**: `bugfix/dht11-timeout-issue`, `bugfix/mqtt-disconnect-hang`
- **Created from**: `develop`
- **Merged back to**: `develop`
- **Deleted after**: Merge
- **Purpose**: Non-critical bug fixes for development

**Example**:
```bash
git checkout develop
git pull origin develop
git checkout -b bugfix/mqtt-disconnect-hang
```

#### `hotfix/*` - Critical Production Fixes
- **Naming**: `hotfix/sensor-data-corruption`, `hotfix/mqtt-broker-timeout`
- **Created from**: `main`
- **Merged back to**: Both `main` AND `develop`
- **Deleted after**: Merge
- **Purpose**: Critical bugs found in production that need immediate fixing
- **CI/CD**: Bypass normal testing (use `--no-verify` if needed)

**Example**:
```bash
git checkout main
git pull origin main
git checkout -b hotfix/mqtt-broker-timeout
# ... fix and test on hardware ...
git push -u origin hotfix/mqtt-broker-timeout
# Create PR to main, then PR to develop
```

#### `release/*` - Release Preparation
- **Naming**: `release/v1.0.0`, `release/v1.1.0`
- **Created from**: `develop`
- **Merged back to**: Both `main` AND `develop`
- **Deleted after**: Merge
- **Purpose**: Final testing, bug fixes, and version bumping before release

**Example**:
```bash
git checkout develop
git pull origin develop
git checkout -b release/v1.0.0
# Update version in platformio.ini, README, etc.
# Final testing on hardware
git push -u origin release/v1.0.0
```

#### `test/*` - Testing & Experimentation
- **Naming**: `test/new-uart-driver`, `test/dht11-calibration`
- **Created from**: `develop`
- **Merged back to**: `develop` (or deleted if unsuccessful)
- **Deleted after**: Merge or closure
- **Purpose**: Experimental features, major driver refactoring, POC testing

---

##  Commit Message Convention

Follow **Conventional Commits** format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types:
- **feat**: New feature (e.g., new sensor driver)
- **fix**: Bug fix
- **docs**: Documentation changes (README, comments, MQTT protocol)
- **test**: Test additions or modifications
- **refactor**: Code refactoring (no feature or fix)
- **chore**: Build, dependencies, config changes
- **ci**: CI/CD pipeline changes (.github/workflows)
- **perf**: Performance improvements

### Examples:

```
feat(mqtt): add QoS 2 support for device commands

- Implement QoS 2 handshake in wifi.c
- Update MQTT topic structure
- Add retry logic for failed publishes

Closes #45
```

```
fix(dht11): correct temperature offset calibration

The DHT11 was reading 2°C higher than actual.
Updated calibration constant in dht11.c

Fixes #38
```

```
docs(readme): update build instructions for ESP32
```

```
chore: bump PlatformIO version to 6.1.0
```

---

## 🔄 Daily Workflow for IoT Team

### 1️⃣ **Starting a New Feature**

```powershell
# Update develop with latest changes
git checkout develop
git pull origin develop

# Create feature branch
git checkout -b feature/your-feature-name

# Start coding in VS Code with PlatformIO
# Compile: pio run -e esp32
# Test: pio test -e native
```

### 2️⃣ **Committing Changes**

```powershell
# Stage your changes
git add src/your_file.c include/your_file.h

# Commit with meaningful message
git commit -m "feat(sensor): add DHT11 temperature averaging

- Average last 5 readings for stability
- Update sensor calibration constants
- Add debug logging for temperature values
"

# Push to GitHub
git push origin feature/your-feature-name
```

### 3️⃣ **Creating a Pull Request**

Go to: https://github.com/DanWeld/VIA_Urban_MicroFarm_IoT_Team/pulls

- Click **"New Pull Request"**
- **Base**: `develop`
- **Compare**: `feature/your-feature-name`
- Fill out the PR template (hardware tested, tests passing, etc.)
- Link related issues: `Closes #XX`
- Request reviewer (team lead or senior developer)

### 4️⃣ **Code Review & Feedback**

- Reviewer checks code quality, hardware compatibility, MQTT protocol compliance
- Waiting for approvals? Make additional commits as needed:

```powershell
# Make requested changes
# Commit again (don't squash yet)
git add src/your_file.c
git commit -m "fix: address PR feedback on sensor error handling"
git push origin feature/your-feature-name
# PR updates automatically
```

### 5️⃣ **Merging to Develop**

**After PR approval**:

- Reviewer clicks **"Squash and merge"** (default strategy)
- Delete branch on GitHub
- Pull latest develop locally:

```powershell
git checkout develop
git pull origin develop
```

---

##  Release Process

### Step 1: Create Release Branch

```powershell
git checkout develop
git pull origin develop
git checkout -b release/v1.0.0

# Update version numbers
# - platformio.ini: version = 1.0.0
# - README.md: ## Firmware Version: v1.0.0
# - src/main.c: const char FW_VERSION[] = "1.0.0";

git add platformio.ini README.md src/main.c
git commit -m "chore: bump version to v1.0.0"
git push -u origin release/v1.0.0
```

### Step 2: Final Testing on Hardware

- Compile all board environments:
  ```bash
  pio run -e arduino_uno
  pio run -e esp32
  ```
- Run unit tests:
  ```bash
  pio test -e native
  ```
- Hardware testing checklist:
  - [ ] All sensors reading correctly
  - [ ] MQTT connectivity to Mosquitto broker stable
  - [ ] Message publish/subscribe working
  - [ ] Network recovery after disconnection
  - [ ] Power consumption acceptable

### Step 3: Merge to Main

Create PR: `release/v1.0.0` → `main`
- Approval required
- Create Git tag after merge:
  ```powershell
  git checkout main
  git pull origin main
  git tag -a v1.0.0 -m "Release v1.0.0 - Stable MQTT firmware"
  git push origin v1.0.0
  ```

### Step 4: Merge Back to Develop

Create PR: `release/v1.0.0` → `develop`
- Fast-track approval (no new testing required)
- Delete release branch after merge

---

##  Hotfix Process (Critical Issues)

**When production firmware has a critical bug** (e.g., MQTT connection loss, sensor data corruption):

```powershell
# Create hotfix from main
git checkout main
git pull origin main
git checkout -b hotfix/mqtt-broker-timeout

# Fix the issue (quick, focused change)
# Test thoroughly on hardware

git add src/wifi.c
git commit -m "fix(mqtt): increase broker connection timeout to 30s

Production issue: firmware losing MQTT connection after 15s of inactivity.
Root cause: broker timeout too aggressive.
Solution: Increase timeout and add keep-alive pings.

Fixes #120
"

git push -u origin hotfix/mqtt-broker-timeout
```

**Then**:
1. PR to `main` - urgent review & merge
2. Tag the hotfix: `git tag -a v1.0.1 -m "Hotfix: MQTT timeout issue"`
3. PR to `develop` - merge without delay

---

## Branch Naming Quick Reference

| Branch Type | Pattern | Example | From | To |
|------------|---------|---------|------|-----|
| Feature | `feature/*` | `feature/wifi-scan` | `develop` | `develop` |
| Bug Fix | `bugfix/*` | `bugfix/uart-overflow` | `develop` | `develop` |
| Hotfix | `hotfix/*` | `hotfix/mqtt-timeout` | `main` | `main` + `develop` |
| Release | `release/*` | `release/v1.0.0` | `develop` | `main` + `develop` |
| Test/POC | `test/*` | `test/dht-calibration` | `develop` | `develop` |

---

##  Branch Protection Rules (GitHub)

### For `main` branch:
- ✅ Require pull request reviews (1 reviewer)
- ✅ Require status checks to pass (compile.yml workflow)
- ✅ Require branches to be up to date
- ✅ Dismiss stale pull request approvals
- ✅ Require code reviews from code owners (optional)

### For `develop` branch:
- ✅ Require pull request reviews (1 reviewer)
- ✅ Require status checks to pass
- ✅ Require branches to be up to date

---

##  Team Roles

| Role | Responsibilities |
|------|-----------------|
| **Team Lead** | Merge PRs, manage releases, GitHub settings |
| **Developer** | Create feature/bugfix branches, write code, submit PRs |
| **Reviewer** | Review PRs, test on hardware, approve/request changes |
| **Hardware Tester** | Verify firmware on physical hardware before release |

---

##  Setup Checklist

- [ ] Repository created on GitHub: `VIA_Urban_MicroFarm_IoT_Team`
- [ ] `main` branch exists and protected
- [ ] `develop` branch exists and protected
- [ ] All team members can clone repository
- [ ] `.gitignore` configured for PlatformIO/C projects
- [ ] `.github/workflows/compile.yml` set up for CI/CD
- [ ] `.github/pull_request_template.md` configured
- [ ] First feature branch created and tested
- [ ] First PR merged via proper workflow
- [ ] Team trained on branching strategy
- [ ] Release process documented and tested

---

##  References

- **Git Flow**: https://nvie.com/posts/a-successful-git-branching-model/
- **Conventional Commits**: https://www.conventionalcommits.org/
- **GitHub Flow**: https://guides.github.com/introduction/flow/
- **PlatformIO Docs**: https://docs.platformio.org/
- **Mosquitto Protocol**: See [`doc/MQTT_PROTOCOL.md`](doc/MQTT_PROTOCOL.md)

---

**Last Updated**: April 16, 2026  
**Version**: 1.0.0  
**Owner**: IoT Team SEP4

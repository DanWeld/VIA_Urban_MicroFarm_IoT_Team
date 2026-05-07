# Git Branching Strategy - IoT Team

## Overview

This document defines the Git branching strategy for the **VIA Urban MicroFarm IoT Team**.

**Repository**: https://github.com/DanWeld/VIA_Urban_MicroFarm_IoT_Team

---

## Branch Structure

### Main Branches (Protected)

#### `main` - Stable Code
- Only contains working, tested code
- Never push directly — always via Pull Request from `develop`
- Tagged at the end of each sprint with a version number

#### `develop` - Integration Branch
- Main working branch where features are integrated
- All feature branches are created from here and merged back here
- Should always be in a buildable state

### Feature Branches (Temporary)

#### `feature/*` - Feature Development
- **Naming**: `feature/mqtt-publish`, `feature/dht11-driver`
- Created from `develop`, merged back to `develop`
- Deleted after merge
- One branch per task

---

## Daily Workflow

### 1. Start a new task

```bash
git checkout develop
git pull origin develop
git checkout -b feature/your-task-name
```

### 2. Commit your changes

Keep commits small and focused on one thing.

```bash
git add src/your_file.c
git commit -m "feat(mqtt): add sensor data publish function"
git push origin feature/your-task-name
```

### 3. Open a Pull Request

- Go to GitHub and open a PR
- Base: `develop`
- Compare: `feature/your-task-name`
- Request a review from a teammate
- Delete the branch after merge

### 4. Merge to main (end of sprint)

When `develop` is stable and tested, open a PR from `develop` to `main` and create a version tag:

```bash
git checkout main
git pull origin main
git tag -a v1.0.0 -m "Sprint 1 release"
git push origin v1.0.0
```

---

## Commit Message Convention

Follow this format:

```
<type>(<scope>): <short description>
```

**Types:**
- `feat` — new feature or driver
- `fix` — bug fix
- `docs` — documentation changes
- `test` — test additions or changes
- `refactor` — code restructuring without feature change
- `chore` — build, config, or dependency changes
- `ci` — CI/CD pipeline changes

**Examples:**
```
feat(mqtt): add QoS 1 publish for sensor data
fix(dht11): correct temperature offset calibration
docs(readme): update broker setup instructions
chore: update platformio version
```

---

## Branch Naming Quick Reference

| Branch | Pattern | Example | From | To |
|--------|---------|---------|------|----|
| Feature | `feature/*` | `feature/mqtt-publish` | `develop` | `develop` |

---

## Branch Protection Rules

### `main`
- Require pull request review (1 reviewer)
- Require status checks to pass (CI/CD)
- Block direct pushes
- Nobody may approve or merge their own pull request

### `develop`
- Require pull request review (1 reviewer)
- Block direct pushes
- Nobody may approve or merge their own pull request

---

## Team Roles

| Role | Responsibilities |
|------|-----------------|
| **Team Lead** | Merge PRs to main, manage releases, GitHub settings |
| **Developer** | Create feature branches, write code, submit PRs |
| **Reviewer** | Review PRs, approve or request changes |

---

## Setup Checklist

- [ ] `main` branch exists and protected
- [ ] `develop` branch exists and protected
- [ ] All team members can clone the repository
- [ ] `.gitignore` configured for PlatformIO/C projects
- [ ] CI/CD workflow set up in `.github/workflows/`
- [ ] First feature branch created and merged via PR

---

## References

- [Git Good Practices (course slides)](../doc/)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Git Rebase explained](https://medium.com/osedea/git-rebase-powerful-command-507bbac4a234)

---

**Last Updated**: 23th of April 2026
**Owner**: IoT Team Lead

# Windows Release Automation Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Publish a Windows build pipeline that produces a releasable Stream Deck plugin package from Git tags.

**Architecture:** Keep the existing CMake and deployment path intact, but build on GitHub-hosted Windows runners where MSVC and Qt are available. Publish the packaged `.streamDeckPlugin` file as both a workflow artifact and a GitHub Release asset so the Windows PC only needs to download the release.

**Tech Stack:** GitHub Actions, CMake, Ninja, MSVC, Qt 6, PowerShell

---

### Task 1: Make the modified Discord IPC dependency publishable

**Files:**
- Modify: `.gitmodules`

**Step 1: Update the submodule URL**

Set `deps/qtdiscordipc` to a GitHub repository owned by `VincentBurke` so the main repository can reference a reachable commit.

**Step 2: Push the dependency commit**

Run:

```bash
git -C deps/qtdiscordipc push origin HEAD:master
```

Expected: the fork contains the multi-target IPC commit referenced by the main repo.

**Step 3: Commit**

```bash
git add .gitmodules deps/qtdiscordipc
git commit -m "build: publish discord ipc fork"
```

### Task 2: Add Windows release automation

**Files:**
- Create: `.github/workflows/windows-release.yml`

**Step 1: Build on Windows**

Configure GitHub Actions to check out submodules, install Qt, configure CMake, build, and install the plugin in `Release`.

**Step 2: Package the plugin**

Compress the generated `cz.danol.discordmixer.sdPlugin` directory into a `.streamDeckPlugin` asset.

**Step 3: Publish outputs**

Upload the package as a workflow artifact for manual runs and attach it to GitHub Releases when the workflow runs from a `v*` tag.

**Step 4: Commit**

```bash
git add .github/workflows/windows-release.yml
git commit -m "ci: add windows release workflow"
```

### Task 3: Prepare release metadata

**Files:**
- Modify: `dist/manifest.json`
- Modify: `README.md`

**Step 1: Align release metadata**

Point repository URLs at the maintained GitHub repository and set the manifest version for the new release.

**Step 2: Commit**

```bash
git add dist/manifest.json README.md
git commit -m "chore: prepare v2.1.1 release"
```

### Task 4: Push and publish

**Files:**
- Test: GitHub Actions run for `.github/workflows/windows-release.yml`

**Step 1: Push branch**

```bash
git push origin master
```

**Step 2: Tag the release**

```bash
git tag -a v2.1.1 -m "v2.1.1"
git push origin v2.1.1
```

**Step 3: Verify outputs**

Expected:
- the Windows workflow succeeds
- a GitHub Release for `v2.1.1` exists
- the release contains a `.streamDeckPlugin` asset

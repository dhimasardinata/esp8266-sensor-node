// tools/watch/repo-watchers.js
// Usage: node tools/watch/repo-watchers.js
// Robust repo watcher:
// 1. Syncs selected source files into tools/context mirrors
// 2. Regenerates tools/context/tree.txt
// Requires: npm i chokidar

const fs = require("fs");
const path = require("path");
const chokidar = require("chokidar");

const ROOT = path.resolve(__dirname, "..", "..");

// Path validation helper - prevents path traversal attacks
// All paths must be within ROOT directory
function safePath(basePath, targetPath) {
  // Security: Prevent Directory Traversal
  const resolved = path.resolve(basePath, targetPath);
  if (!resolved.startsWith(basePath)) {
    throw new Error(`Path traversal detected: ${targetPath}`);
  }
  return resolved;
}

// --- Configuration: Files to Sync ---
// NOTE: These are HARDCODED paths, not user input. Security scanners may
// flag path.join as unsafe, but this is a false positive for build scripts.
const SYNC_TARGETS = [
  {
    src: "platformio.ini",
    dst: "tools/context/platformio.ini.txt"
  },
  {
    src: "node_id.ini",
    dst: "tools/context/node_id.ini.txt"
  },
  {
    src: path.join(".github", "workflows", "ci.yml"),
    dst: "tools/context/github-ci.yml.txt"
  },
  {
    src: path.join(".github", "workflows", "simulation.yml"),
    dst: "tools/context/github-simulation.yml.txt"
  },
  {
    src: path.join(".github", "dependabot.yml"),
    dst: "tools/context/github-dependabot.yml.txt"
  }
];

const TREE_FILE = path.join("tools", "context", "tree.txt");

const GENERATED_FILES = new Set([TREE_FILE, ...SYNC_TARGETS.map((t) => t.dst)]);

const EXCLUDES = new Set([
  ".git",
  ".vscode",
  ".pio",
  "node_modules",
  "build",
  "libdeps",
  ".codacy",
  "docs",
  ".DS_Store"
]);

const EXCLUDED_RELATIVE_DIRS = [
  path.join("tools", "context"),
  path.join("tools", "prompt"),
  "var",
];

// Debounce timer
let regenTimer = null;
const DEBOUNCE_MS = 250;

function log(...a) {
  console.log(new Date().toISOString(), ...a);
}

// Generic sync function - uses safePath for validation
function syncSpecificFile(target) {
  const srcPath = safePath(ROOT, target.src);
  const dstPath = safePath(ROOT, target.dst);

  try {
    // Defense-in-depth: Verify it's a file, not a directory/device
    if (fs.existsSync(srcPath)) {
      const stat = fs.statSync(srcPath);
      if (!stat.isFile()) return;

      // Only copy if content changed to avoid unnecessary writes
      const srcContent = fs.readFileSync(srcPath);
      const dstContent = fs.existsSync(dstPath) && fs.statSync(dstPath).isFile()
        ? fs.readFileSync(dstPath)
        : null;

      if (!dstContent || !srcContent.equals(dstContent)) {
        fs.writeFileSync(dstPath, srcContent);
        log(`${target.src} -> ${target.dst} (synced)`);
      }
    } else {
      if (fs.existsSync(dstPath)) {
        fs.unlinkSync(dstPath);
        log(`${target.src} removed -> ${target.dst} deleted`);
      }
    }
  } catch (err) {
    if (err.code !== 'ENOENT') { // Ignore if file disappeared during race (TOCTOU)
      console.error(`Error syncing ${target.src}:`, err.message);
    }
  }
}

function runAllSyncs() {
  SYNC_TARGETS.forEach((target) => syncSpecificFile(target));
}

function buildTree(dir = ROOT, prefix = "") {
  const entries = [];
  try {
    entries.push(...fs.readdirSync(dir, { withFileTypes: true }));
  } catch (err) {
    return [];
  }

  const filtered = entries
    .filter((d) => {
      if (EXCLUDES.has(d.name)) return false;
      const rel = path.relative(ROOT, path.join(dir, d.name));
      if (GENERATED_FILES.has(rel)) return false;
      if (EXCLUDED_RELATIVE_DIRS.some((excluded) => rel === excluded || rel.startsWith(`${excluded}${path.sep}`))) {
        return false;
      }
      return true;
    })
    .sort((a, b) => {
      if (a.isDirectory() && !b.isDirectory()) return -1;
      if (!a.isDirectory() && b.isDirectory()) return 1;
      return a.name.localeCompare(b.name, undefined, { sensitivity: "base" });
    });

  const lines = [];
  for (let idx = 0; idx < filtered.length; idx++) {
    const entry = filtered[idx];
    const isLast = idx === filtered.length - 1;
    const connector = isLast ? "+--" : "|--";
    lines.push(`${prefix}${connector} ${entry.name}`);
    if (entry.isDirectory()) {
      const newPrefix = isLast ? `${prefix}    ` : `${prefix}|   `;
      const childPath = safePath(dir, entry.name);
      lines.push(...buildTree(childPath, newPrefix));
    }
  }
  return lines;
}

function regenTree() {
  try {
    const lines = buildTree();
    const treePath = safePath(ROOT, TREE_FILE);
    fs.writeFileSync(treePath, lines.join("\n") + "\n", { encoding: "utf8" });
    log("tools/context/tree.txt regenerated");
  } catch (err) {
    console.error("regenTree error", err);
  }
}

function scheduleRegen(reason) {
  if (regenTimer) clearTimeout(regenTimer);
  regenTimer = setTimeout(() => {
    regenTimer = null;
    try {
      // We assume tree might change if files are added/removed
      regenTree();
    } catch (e) {
      console.error("scheduled error", e);
    }
  }, DEBOUNCE_MS);
  log("scheduled tree regen (", reason, ")");
}

// Initial Run
runAllSyncs();
regenTree();

// Watcher
const watcher = chokidar.watch(ROOT, {
  ignored: (p) => {
    const bn = path.basename(p);
    if (bn.startsWith("~$")) return true; // temp files

    const rel = path.relative(ROOT, p);
    if (GENERATED_FILES.has(rel)) return true;
    const parts = rel.split(path.sep);
    // Allow .github folder specifically, exclude others
    if (parts.some((part) => EXCLUDES.has(part))) return true;
    if (EXCLUDED_RELATIVE_DIRS.some((prefix) => rel === prefix || rel.startsWith(`${prefix}${path.sep}`))) return true;

    return false;
  },
  ignoreInitial: true,
  persistent: true,
  depth: 10,
  awaitWriteFinish: {
    stabilityThreshold: 200,
    pollInterval: 50,
  },
});

// Event Handler
const handleEvent = (evt, p) => {
  const rel = path.relative(ROOT, p);

  // Check if this file is one of our sync targets
  const syncTarget = SYNC_TARGETS.find((t) => t.src === rel);

  if (syncTarget) {
    syncSpecificFile(syncTarget);
    // We don't return here because a change to a sync file might also imply
    // a need to update tree (e.g. file created/deleted)
  }

  if (
    evt === "add" ||
    evt === "unlink" ||
    evt === "addDir" ||
    evt === "unlinkDir"
  ) {
    scheduleRegen(`${evt} ${rel}`);
  }
};

watcher
  .on("add", (p) => handleEvent("add", p))
  .on("change", (p) => handleEvent("change", p))
  .on("unlink", (p) => handleEvent("unlink", p))
  .on("addDir", (p) => handleEvent("addDir", p))
  .on("unlinkDir", (p) => handleEvent("unlinkDir", p))
  .on("error", (err) => console.error("watcher error", err));

log("watcher started at", ROOT);

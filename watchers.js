// watcher.js
// Usage: node watcher.js
// Robust file watcher:
// 1. Syncs specific config files to root .txt files (platformio.ini, ci.yml, etc.)
// 2. Regenerates tree.txt structure
// Requires: npm i chokidar

const fs = require("fs");
const path = require("path");
const chokidar = require("chokidar");

const ROOT = process.cwd();

// --- Configuration: Files to Sync ---
// Add any other files you want to mirror to root .txt here
const SYNC_TARGETS = [
  {
    src: "platformio.ini",
    dst: "platformio.ini.txt",
  },
  {
    src: "node_id.ini",
    dst: "node_id.ini.txt",
  },
  {
    src: path.join(".github", "workflows", "ci.yml"),
    dst: "ci.yml.txt",
  },
  {
    src: path.join(".github", "dependabot.yml"),
    dst: "dependabot.yml.txt",
  },
];

const TREE_FILE = "tree.txt";

// Files/Folders to ignore in tree.txt
// Also includes the destination files from SYNC_TARGETS so they don't clutter the tree
const GENERATED = new Set([TREE_FILE, ...SYNC_TARGETS.map((t) => t.dst)]);

const EXCLUDES = new Set([
  ".git",
  ".vscode",
  ".pio",
  "node_modules",
  "build",
  "libdeps",
  ".codacy",
  "docs",
  ".DS_Store",
]);

// Debounce timer
let regenTimer = null;
const DEBOUNCE_MS = 250;

function log(...a) {
  console.log(new Date().toISOString(), ...a);
}

// Generic sync function
function syncSpecificFile(target) {
  const srcPath = path.join(ROOT, target.src);
  const dstPath = path.join(ROOT, target.dst);

  try {
    if (fs.existsSync(srcPath)) {
      // Only copy if content changed to avoid unnecessary writes
      const srcContent = fs.readFileSync(srcPath);
      const dstContent = fs.existsSync(dstPath)
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
    console.error(`Error syncing ${target.src}:`, err.message);
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
    .filter((d) => !EXCLUDES.has(d.name) && !GENERATED.has(d.name))
    .sort((a, b) => {
      if (a.isDirectory() && !b.isDirectory()) return -1;
      if (!a.isDirectory() && b.isDirectory()) return 1;
      return a.name.localeCompare(b.name, undefined, { sensitivity: "base" });
    });

  const lines = [];
  filtered.forEach((entry, idx) => {
    const isLast = idx === filtered.length - 1;
    const connector = isLast ? "+--" : "|--";
    lines.push(`${prefix}${connector} ${entry.name}`);
    if (entry.isDirectory()) {
      const newPrefix = isLast ? `${prefix}    ` : `${prefix}|   `;
      const childPath = path.join(dir, entry.name);
      lines.push(...buildTree(childPath, newPrefix));
    }
  });
  return lines;
}

function regenTree() {
  try {
    const lines = buildTree();
    const treePath = path.join(ROOT, TREE_FILE);
    fs.writeFileSync(treePath, lines.join("\n") + "\n", { encoding: "utf8" });
    log("tree.txt regenerated");
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
    if (GENERATED.has(bn)) return true;
    if (bn.startsWith("~$")) return true; // temp files

    const rel = path.relative(ROOT, p);
    const parts = rel.split(path.sep);
    // Allow .github folder specifically, exclude others
    if (parts.some((part) => EXCLUDES.has(part))) return true;

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

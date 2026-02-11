
const fs = require('fs');
const vm = require('vm');

// Load crypto.js but manually REMOVE the fix to simulate the bug
let cryptoCode = fs.readFileSync('./data/crypto.js', 'utf8');
// Remove the fix line: if (thisWords[index] === undefined) thisWords[index] = 0;
cryptoCode = cryptoCode.replace(/if\s*\(thisWords\[index\]\s*===\s*undefined\)\s*thisWords\[index\]\s*=\s*0;/g, '// Fix removed for reproduction');

const sandbox = {
    window: {},
    console: console
};
sandbox.window = sandbox;

try {
    vm.createContext(sandbox);
    vm.runInContext(cryptoCode, sandbox);
} catch (e) {
    console.error("Error loading crypto.js:", e);
    process.exit(1);
}

const CryptoJS = sandbox.CryptoJS;

// Reproduce login medini failure
try {
    const cmd = "login medini"; // 12 chars
    console.log(`Testing command: '${cmd}'`);
    
    // Simulate terminal.html logic
    const now = 1700000000;
    const textWA = CryptoJS.enc.Utf8.parse(cmd);
    
    // Use manual concat logic from older terminal.html or just raw WordArray concat
    // terminal.html v1 used: const combinedWords = [now].concat(textWA.words);
    // But we are testing CryptoJS.lib.WordArray.concat primarily (as user pointed to it).
    
    // Let's emulate creating a WordArray and concatenating to it, triggering the bug
    const wa1 = CryptoJS.lib.WordArray.create([now], 4); 
    // wa1 has 1 word.
    
    // Concatenate textWA (3 words: 'logi', 'n me', 'dini')
    console.log("Applying concat...");
    wa1.concat(textWA);
    
    // Expected: wa1 should have 4 words.
    // Index 0: now
    // Index 1: logi
    // Index 2: n me
    // Index 3: dini
    
    console.log("Concat result words:", wa1.words);
    console.log("Concat result sigBytes:", wa1.sigBytes);
    
    // If bug exists, one of the words might be NaN or corrupt?
    for (let i = 0; i < wa1.words.length; i++) {
        if (Number.isNaN(wa1.words[i])) {
            console.error(`Word at index ${i} is NaN! Bug reproduced.`);
            process.exit(1);
        }
    }
    
    // Now encrypt
    const RAW_KEY = "12345678901234567890123456789012";
    const key = CryptoJS.enc.Utf8.parse(RAW_KEY);
    const iv = CryptoJS.lib.WordArray.random(16);
    
    console.log("Encrypting...");
    CryptoJS.AES.encrypt(wa1, key, { iv: iv, mode: CryptoJS.mode.CBC, padding: CryptoJS.pad.Pkcs7 });
    console.log("Encryption success!");

} catch (e) {
    console.error("Crashed:", e);
}

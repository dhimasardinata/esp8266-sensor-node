
const fs = require('fs');
const vm = require('vm');

// Load crypto.js content
const cryptoCode = fs.readFileSync('./data/crypto.js', 'utf8');

// Mock window/global
const sandbox = {
    window: {},
    console: console
};
sandbox.window = sandbox;

// Execute crypto.js in sandbox
try {
    vm.createContext(sandbox);
    vm.runInContext(cryptoCode, sandbox);
} catch (e) {
    console.error("Error loading crypto.js:", e);
    process.exit(1);
}

const CryptoJS = sandbox.CryptoJS;

// Mock the encryption function from terminal.html
const encryptMessage = function (plaintext) {
    if (!CryptoJS) return null;

    // 1. Get Timestamp (Seconds)
    // Use fixed timestamp for reproducibility
    const now = 1700000000;

    // 2. Prepare Data Buffer: [4 bytes Timestamp] [Plaintext]
    const textWA = CryptoJS.enc.Utf8.parse(plaintext);

    // Explicitly create WordArray for timestamp to ensure safe concatenation
    const tsArray = CryptoJS.lib.WordArray.create([now], 4);
    tsArray.concat(textWA);

    const combinedWA = tsArray;

    // 3. Encrypt
    const RAW_KEY = "12345678901234567890123456789012";
    const key = CryptoJS.enc.Utf8.parse(RAW_KEY);
    const iv = CryptoJS.lib.WordArray.random(16);

    try {
        const encrypted = CryptoJS.AES.encrypt(combinedWA, key, {
            iv: iv,
            mode: CryptoJS.mode.CBC,
            padding: CryptoJS.pad.Pkcs7,
        });

        const ivB64 = CryptoJS.enc.Base64.stringify(iv);
        const cipherB64 = CryptoJS.enc.Base64.stringify(encrypted.ciphertext);
        return ivB64 + ":" + cipherB64;
    } catch (e) {
        console.error("Internal Encryption Error:", e);
        throw e;
    }
};

try {
    console.log("--- TEST START ---");

    // Test case 1: Short command (single block)
    console.log("Testing 'login'...");
    const statusEnc = encryptMessage("login");
    console.log("SUCCESS: 'login' Encrypted length:", statusEnc.length);

    // Test case 2: Long command (multi block)
    console.log("\nTesting 'login medini123'...");
    const loginEnc = encryptMessage("login medini123");
    console.log("SUCCESS: 'login medini123' Encrypted length:", loginEnc.length);

    // Verify decryption of the long command to be sure
    console.log("\nVerifying decryption...");

    // Decrypt logic from terminal.html
    const decryptMessage = function (payload) {
        const colonIndex = payload.indexOf(":");
        const ivB64 = payload.substring(0, colonIndex);
        const cipherB64 = payload.substring(colonIndex + 1);

        const key = CryptoJS.enc.Utf8.parse("12345678901234567890123456789012");
        const iv = CryptoJS.enc.Base64.parse(ivB64);
        const ciphertext = CryptoJS.enc.Base64.parse(cipherB64);

        const decrypted = CryptoJS.AES.decrypt({ ciphertext: ciphertext }, key, {
            iv: iv,
            mode: CryptoJS.mode.CBC,
            padding: CryptoJS.pad.Pkcs7,
        });

        // Remove timestamp
        const words = decrypted.words;
        if (words.length > 0) {
            words.shift();
            decrypted.sigBytes -= 4;
        }

        return decrypted.toString(CryptoJS.enc.Utf8);
    };

    const decrypted = decryptMessage(loginEnc);
    console.log("Decrypted text:", decrypted);
    if (decrypted !== "login medini123") {
        throw new Error(`Decryption mismatch! Expected 'login medini123', got '${decrypted}'`);
    }
    console.log("Decryption verified.");

} catch (e) {
    console.error("\n!!! TEST FAILED !!!");
    console.error(e);
    process.exit(1);
}

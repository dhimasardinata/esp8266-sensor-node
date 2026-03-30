
const fs = require('fs');
const vm = require('vm');

const cryptoCode = fs.readFileSync('./data/crypto.js', 'utf8');
const sandbox = { window: {}, console: console };
sandbox.window = sandbox;
vm.createContext(sandbox);
vm.runInContext(cryptoCode, sandbox);
const CryptoJS = sandbox.CryptoJS;

function test(plaintext) {
    console.log(`\nTesting: "${plaintext}"`);
    const now = 1700000000;
    const textWA = CryptoJS.enc.Utf8.parse(plaintext);
    const tsArray = CryptoJS.lib.WordArray.create([now], 4);
    tsArray.concat(textWA);

    console.log("CombinedWA sigBytes:", tsArray.sigBytes);
    console.log("CombinedWA words.length:", tsArray.words.length);

    // Padding should add 16 bytes (4 words)
    // Total should be 32 bytes (8 words)

    const key = CryptoJS.enc.Utf8.parse("12345678901234567890123456789012");
    const iv = CryptoJS.lib.WordArray.random(16);

    try {
        const encrypted = CryptoJS.AES.encrypt(tsArray, key, {
            iv: iv,
            mode: CryptoJS.mode.CBC,
            padding: CryptoJS.pad.Pkcs7,
        });
        console.log("Encrypted sigBytes:", encrypted.ciphertext.sigBytes);
        console.log("Encrypted words.length:", encrypted.ciphertext.words.length);
        console.log("Encryption success.");
    } catch (e) {
        console.error("Encryption failed:", e);
    }
}

test("login medini");

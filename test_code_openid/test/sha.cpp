#include "mbedtls/sha256.h"

void generateAESKeyFromPasswordAndSalt(const char *password, const unsigned char *salt, size_t salt_len, unsigned char aes_key[32]) {
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256, 1 = SHA-224

    // Hash password first
    mbedtls_sha256_update_ret(&ctx, (const unsigned char *)password, strlen(password));

    // Then add salt
    mbedtls_sha256_update_ret(&ctx, salt, salt_len);

    mbedtls_sha256_finish_ret(&ctx, aes_key);
    mbedtls_sha256_free(&ctx);
}

void setup() {
    Serial.begin(115200);

    const char *user_password = "MySecretPassword123!";
    // Example 16-byte salt (random and unique for each user/file)
    const unsigned char salt[16] = {
        0x21, 0x34, 0xA9, 0x55, 0x76, 0x4B, 0x2E, 0x8F,
        0x90, 0xC3, 0x1D, 0x7E, 0xFA, 0x62, 0xB5, 0x88
    };

    unsigned char aes_key[32];

    generateAESKeyFromPasswordAndSalt(user_password, salt, sizeof(salt), aes_key);

    Serial.println("Generated AES-256 Key with salt:");
    for (int i = 0; i < 32; i++) {
        Serial.printf("%02X ", aes_key[i]);
    }
    Serial.println();
}

void loop() {}

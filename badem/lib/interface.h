#ifndef BADEM_INTERFACE_H
#define BADEM_INTERFACE_H

#if __cplusplus
extern "C" {
#endif

typedef unsigned char * badem_uint128; // 16byte array for public and private keys
typedef unsigned char * badem_uint256; // 32byte array for public and private keys
typedef unsigned char * badem_uint512; // 64byte array for signatures
typedef void * badem_transaction;

// Convert amount bytes 'source' to a 39 byte not-null-terminated decimal string 'destination'
void badem_uint128_to_dec (const badem_uint128 source, char * destination);
// Convert public/private key bytes 'source' to a 64 byte not-null-terminated hex string 'destination'
void badem_uint256_to_string (const badem_uint256 source, char * destination);
// Convert public key bytes 'source' to a 65 byte non-null-terminated account string 'destination'
void badem_uint256_to_address (badem_uint256 source, char * destination);
// Convert public/private key bytes 'source' to a 128 byte not-null-terminated hex string 'destination'
void badem_uint512_to_string (const badem_uint512 source, char * destination);

// Convert 39 byte decimal string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int badem_uint128_from_dec (const char * source, badem_uint128 destination);
// Convert 64 byte hex string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int badem_uint256_from_string (const char * source, badem_uint256 destination);
// Convert 128 byte hex string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int badem_uint512_from_string (const char * source, badem_uint512 destination);

// Check if the null-terminated string 'account' is a valid xrb account number
// Return 0 on correct, nonzero on invalid
int badem_valid_address (const char * account);

// Create a new random number in to 'destination'
void badem_generate_random (badem_uint256 destination);
// Retrieve the deterministic private key for 'seed' at 'index'
void badem_seed_key (const badem_uint256 seed, int index, badem_uint256);
// Derive the public key 'pub' from 'key'
void badem_key_account (badem_uint256 key, badem_uint256 pub);

// Sign 'transaction' using 'private_key' and write to 'signature'
char * badem_sign_transaction (const char * transaction, const badem_uint256 private_key);
// Generate work for 'transaction'
char * badem_work_transaction (const char * transaction);

#if __cplusplus
} // extern "C"
#endif

#endif // BADEM_INTERFACE_H

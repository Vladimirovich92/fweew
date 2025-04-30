#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_LENGTH 32
#define RIPEMD160_DIGEST_LENGTH 20

// secp256k1 constants
#define P_0 0xFFFFFC2F
#define P_1 0xFFFFFFFE
#define P_2 0xFFFFFFFF
#define P_3 0xFFFFFFFF
#define P_4 0xFFFFFFFF
#define P_5 0xFFFFFFFF
#define P_6 0xFFFFFFFF
#define P_7 0xFFFFFFFF

#define G_X_0 0xD0364141
#define G_X_1 0xBFD25E8C
#define G_X_2 0xAF48A03B
#define G_X_3 0xBAAEDCE6
#define G_X_4 0xFFFFFFFE
#define G_X_5 0xFFFFFFFF
#define G_X_6 0xFFFFFFFF
#define G_X_7 0x79BE667E

#define G_Y_0 0x9833E73F
#define G_Y_1 0xA4E6F8B5
#define G_Y_2 0x3CE6FAAD
#define G_Y_3 0x2F241D76
#define G_Y_4 0x00000000
#define G_Y_5 0x00000000
#define G_Y_6 0x00000000
#define G_Y_7 0x550C7DC3

// Big number representation
typedef struct {
    uint d[8];
} bignum;

typedef struct {
    bignum x;
    bignum y;
    bool infinity;
} point;

// SHA256
#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_SIG0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_SIG1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_EP0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_EP1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

__constant uint sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void sha256(const uchar *data, uint len, uchar *hash) {
    uint h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint w[64];
    uint a, b, c, d, e, f, g, h_temp;

    uint padded_len = ((len + 8 + 63) / 64) * 64;
    uchar padded[128] = {0};
    for (uint i = 0; i < len; ++i) padded[i] = data[i];
    padded[len] = 0x80;
    for (uint i = len + 1; i < padded_len - 8; ++i) padded[i] = 0;
    ulong bit_len = len * 8;
    for (int i = 0; i < 8; ++i) padded[padded_len - 8 + i] = (bit_len >> (56 - i * 8)) & 0xff;

    for (uint block = 0; block < padded_len; block += 64) {
        for (int i = 0; i < 16; ++i) {
            w[i] = (padded[block + i * 4] << 24) |
                   (padded[block + i * 4 + 1] << 16) |
                   (padded[block + i * 4 + 2] << 8) |
                   padded[block + i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = SHA256_EP1(w[i - 2]) + w[i - 7] + SHA256_EP0(w[i - 15]) + w[i - 16];
        }

        a = h[0]; b = h[1]; c = h[2]; d = h[3];
        e = h[4]; f = h[5]; g = h[6]; h_temp = h[7];

        for (int i = 0; i < 64; ++i) {
            uint t1 = h_temp + SHA256_SIG1(e) + SHA256_CH(e, f, g) + sha256_k[i] + w[i];
            uint t2 = SHA256_SIG0(a) + SHA256_MAJ(a, b, c);
            h_temp = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += h_temp;
    }

    for (int i = 0; i < 8; ++i) {
        hash[i * 4] = (h[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xff;
        hash[i * 4 + 3] = h[i] & 0xff;
    }
}

// RIPEMD160
__constant uint ripemd160_k[5] = {0x00000000, 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xa953fd4e};
__constant uint ripemd160_kp[5] = {0x50a28be6, 0x5c4dd124, 0x6d703ef3, 0x7a6d76e9, 0x00000000};

#define RIPEMD160_ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define RIPEMD160_F0(x, y, z) ((x) ^ (y) ^ (z))
#define RIPEMD160_F1(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define RIPEMD160_F2(x, y, z) (((x) | ~(y)) ^ (z))
#define RIPEMD160_F3(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define RIPEMD160_F4(x, y, z) ((x) ^ ((y) | ~(z)))

void ripemd160(const uchar *data, uint len, uchar *hash) {
    uint h[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    uint w[16];
    uint a, b, c, d, e, ap, bp, cp, dp, ep;

    uint padded_len = ((len + 8 + 63) / 64) * 64;
    uchar padded[128] = {0};
    for (uint i = 0; i < len; ++i) padded[i] = data[i];
    padded[len] = 0x80;
    for (uint i = len + 1; i < padded_len - 8; ++i) padded[i] = 0;
    ulong bit_len = len * 8;
    for (int i = 0; i < 8; ++i) padded[padded_len - 8 + i] = (bit_len >> (56 - i * 8)) & 0xff;

    for (uint block = 0; block < padded_len; block += 64) {
        for (int i = 0; i < 16; ++i) {
            w[i] = (padded[block + i * 4] << 24) |
                   (padded[block + i * 4 + 1] << 16) |
                   (padded[block + i * 4 + 2] << 8) |
                   padded[block + i * 4 + 3];
        }

        a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
        ap = h[0]; bp = h[1]; cp = h[2]; dp = h[3]; ep = h[4];

        // Left rounds
        for (int i = 0; i < 80; ++i) {
            uint f, k, s, idx;
            if (i < 16) { f = RIPEMD160_F0(b, c, d); k = ripemd160_k[0]; s = (11 * i) % 32; idx = i; }
            else if (i < 32) { f = RIPEMD160_F1(b, c, d); k = ripemd160_k[1]; s = (13 * i) % 32; idx = (5 * i + 1) % 16; }
            else if (i < 48) { f = RIPEMD160_F2(b, c, d); k = ripemd160_k[2]; s = (15 * i) % 32; idx = (3 * i + 5) % 16; }
            else if (i < 64) { f = RIPEMD160_F3(b, c, d); k = ripemd160_k[3]; s = (17 * i) % 32; idx = (7 * i) % 16; }
            else { f = RIPEMD160_F4(b, c, d); k = ripemd160_k[4]; s = (19 * i) % 32; idx = (2 * i + 3) % 16; }

            uint temp = RIPEMD160_ROL(a + f + w[idx] + k, s) + e;
            a = e; e = d; d = RIPEMD160_ROL(c, 10); c = b; b = temp;
        }

        // Right rounds
        for (int i = 0; i < 80; ++i) {
            uint f, k, s, idx;
            if (i < 16) { f = RIPEMD160_F4(bp, cp, dp); k = ripemd160_kp[0]; s = (23 * i) % 32; idx = (7 * i) % 16; }
            else if (i < 32) { f = RIPEMD160_F3(bp, cp, dp); k = ripemd160_kp[1]; s = (19 * i) % 32; idx = (5 * i + 1) % 16; }
            else if (i < 48) { f = RIPEMD160_F2(bp, cp, dp); k = ripemd160_kp[2]; s = (17 * i) % 32; idx = (3 * i + 5) % 16; }
            else if (i < 64) { f = RIPEMD160_F1(bp, cp, dp); k = ripemd160_kp[3]; s = (15 * i) % 32; idx = i; }
            else { f = RIPEMD160_F0(bp, cp, dp); k = ripemd160_kp[4]; s = (11 * i) % 32; idx = (2 * i + 3) % 16; }

            uint temp = RIPEMD160_ROL(ap + f + w[idx] + k, s) + ep;
            ap = ep; ep = dp; dp = RIPEMD160_ROL(cp, 10); cp = bp; bp = temp;
        }

        uint t = h[1] + c + dp;
        h[1] = h[2] + d + ep;
        h[2] = h[3] + e + ap;
        h[3] = h[4] + a + bp;
        h[4] = h[0] + b + cp;
        h[0] = t;
    }

    for (int i = 0; i < 5; ++i) {
        hash[i * 4] = (h[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xff;
        hash[i * 4 + 3] = h[i] & 0xff;
    }
}

// Base58
__constant char base58_alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

void base58_encode(const uchar *input, uint len, __global uchar *output, uint max_len) {
    for (uint i = 0; i < max_len; ++i) output[i] = 0;

    uint zeros = 0;
    for (uint i = 0; i < len && input[i] == 0; ++i) zeros++;

    uchar temp[128] = {0};
    uint temp_len = 0;
    for (uint i = zeros; i < len; ++i) {
        ulong carry = input[i];
        uint j;
        for (j = 0; (j < temp_len || carry) && j < 128; ++j) {
            carry += 256ULL * (j < temp_len ? temp[j] : 0);
            temp[j] = carry % 58;
            carry /= 58;
        }
        if (j > temp_len) temp_len = j;
    }

    uint out_len = 0;
    for (uint i = 0; i < zeros && out_len < max_len; ++i) {
        output[out_len++] = base58_alphabet[0];
    }
    for (uint i = temp_len; i > 0 && out_len < max_len; --i) {
        output[out_len++] = base58_alphabet[temp[i - 1]];
    }
    if (out_len < max_len) output[out_len] = 0;
}

// secp256k1 arithmetic
void bignum_zero(bignum *a) {
    for (int i = 0; i < 8; ++i) a->d[i] = 0;
}

bool bignum_is_zero(const bignum *a) {
    for (int i = 0; i < 8; ++i) if (a->d[i] != 0) return false;
    return true;
}

void bignum_copy(bignum *dest, const bignum *src) {
    for (int i = 0; i < 8; ++i) dest->d[i] = src->d[i];
}

void bignum_add(const bignum *a, const bignum *b, bignum *res) {
    ulong carry = 0;
    for (int i = 0; i < 8; ++i) {
        carry += (ulong)a->d[i] + b->d[i];
        res->d[i] = carry & 0xFFFFFFFF;
        carry >>= 32;
    }

    bignum p = {{P_0, P_1, P_2, P_3, P_4, P_5, P_6, P_7}};
    if (carry || bignum_cmp(res, &p) >= 0) {
        carry = 0;
        for (int i = 0; i < 8; ++i) {
            carry += (ulong)res->d[i] - p.d[i];
            res->d[i] = carry & 0xFFFFFFFF;
            carry = (carry >> 32) ? 0xFFFFFFFF : 0;
        }
    }
}

void bignum_sub(const bignum *a, const bignum *b, bignum *res) {
    bignum p = {{P_0, P_1, P_2, P_3, P_4, P_5, P_6, P_7}};
    long carry = 0;
    for (int i = 0; i < 8; ++i) {
        carry += (long)a->d[i] - b->d[i];
        res->d[i] = carry & 0xFFFFFFFF;
        carry >>= 32;
    }
    if (carry < 0) {
        carry = 0;
        for (int i = 0; i < 8; ++i) {
            carry += (ulong)res->d[i] + p.d[i];
            res->d[i] = carry & 0xFFFFFFFF;
            carry >>= 32;
        }
    }
}

int bignum_cmp(const bignum *a, const bignum *b) {
    for (int i = 7; i >= 0; --i) {
        if (a->d[i] > b->d[i]) return 1;
        if (a->d[i] < b->d[i]) return -1;
    }
    return 0;
}

void bignum_mul(const bignum *a, const bignum *b, bignum *res) {
    bignum p = {{P_0, P_1, P_2, P_3, P_4, P_5, P_6, P_7}};
    bignum temp = {{0}};
    for (int i = 0; i < 8; ++i) {
        ulong carry = 0;
        for (int j = 0; j < 8 - i; ++j) {
            carry += (ulong)a->d[i] * b->d[j] + temp.d[i + j];
            temp.d[i + j] = carry & 0xFFFFFFFF;
            carry >>= 32;
        }
    }

    // Montgomery reduction (simplified)
    for (int i = 0; i < 8; ++i) {
        ulong carry = 0;
        for (int j = 0; j < 8; ++j) {
            carry += (ulong)temp.d[j] + (ulong)res->d[j];
            res->d[j] = carry & 0xFFFFFFFF;
            carry >>= 32;
        }
    }
    if (bignum_cmp(res, &p) >= 0) {
        bignum_sub(res, &p, res);
    }
}

void bignum_mod_inverse(bignum *a, const bignum *p, bignum *res) {
    bignum u, v, x1, x2, temp;
    bignum_copy(&u, a);
    bignum_copy(&v, p);
    bignum_zero(&x1); x1.d[0] = 1;
    bignum_zero(&x2);

    while (!bignum_is_zero(&u)) {
        while (!(u.d[0] & 1)) {
            for (int i = 0; i < 7; ++i) u.d[i] = (u.d[i] >> 1) | (u.d[i + 1] << 31);
            u.d[7] >>= 1;
            if (x1.d[0] & 1) {
                bignum_add(&x1, p, &x1);
            }
            for (int i = 0; i < 7; ++i) x1.d[i] = (x1.d[i] >> 1) | (x1.d[i + 1] << 31);
            x1.d[7] >>= 1;
        }
        while (!(v.d[0] & 1)) {
            for (int i = 0; i < 7; ++i) v.d[i] = (v.d[i] >> 1) | (v.d[i + 1] << 31);
            v.d[7] >>= 1;
            if (x2.d[0] & 1) {
                bignum_add(&x2, p, &x2);
            }
            for (int i = 0; i < 7; ++i) x2.d[i] = (x2.d[i] >> 1) | (x2.d[i + 1] << 31);
            x2.d[7] >>= 1;
        }
        if (bignum_cmp(&u, &v) >= 0) {
            bignum_sub(&u, &v, &u);
            bignum_sub(&x1, &x2, &x1);
        } else {
            bignum_sub(&v, &u, &v);
            bignum_sub(&x2, &x1, &x2);
        }
    }
    if (bignum_cmp(&v, &((bignum){{1, 0, 0, 0, 0, 0, 0, 0}})) != 0) {
        bignum_zero(res);
        return;
    }
    if (bignum_cmp(&x2, p) >= 0) {
        bignum_sub(&x2, p, &x2);
    }
    if (bignum_cmp(&x2, &((bignum){{0}})) < 0) {
        bignum_add(&x2, p, &x2);
    }
    bignum_copy(res, &x2);
}

void point_add(point *p1, point *p2, point *res) {
    bignum p = {{P_0, P_1, P_2, P_3, P_4, P_5, P_6, P_7}};
    if (p1->infinity) {
        *res = *p2;
        return;
    }
    if (p2->infinity) {
        *res = *p1;
        return;
    }

    bignum lambda, temp, x3, y3;
    if (bignum_cmp(&p1->x, &p2->x) == 0 && bignum_cmp(&p1->y, &p2->y) == 0) {
        // Point doubling
        bignum three = {{3, 0, 0, 0, 0, 0, 0, 0}};
        bignum two_y, x1_sq, num, denom;

        bignum_mul(&p1->x, &p1->x, &x1_sq);
        bignum_mul(&three, &x1_sq, &num);
        bignum_add(&num, &((bignum){{7, 0, 0, 0, 0, 0, 0, 0}}), &num); // a = 0 for secp256k1

        bignum_add(&p1->y, &p1->y, &two_y);
        bignum_mod_inverse(&two_y, &p, &denom);
        bignum_mul(&num, &denom, &lambda);
    } else {
        // Point addition
        bignum x2_x1, y2_y1;
        bignum_sub(&p2->x, &p1->x, &x2_x1);
        bignum_sub(&p2->y, &p1->y, &y2_y1);
        bignum_mod_inverse(&x2_x1, &p, &temp);
        bignum_mul(&y2_y1, &temp, &lambda);
    }

    bignum_mul(&lambda, &lambda, &temp);
    bignum_sub(&temp, &p1->x, &x3);
    bignum_sub(&x3, &p2->x, &x3);

    bignum_sub(&p1->x, &x3, &temp);
    bignum_mul(&lambda, &temp, &y3);
    bignum_sub(&y3, &p1->y, &y3);

    res->x = x3;
    res->y = y3;
    res->infinity = false;
}

void point_mul(const bignum *k, point *res) {
    point G = {
        {{G_X_0, G_X_1, G_X_2, G_X_3, G_X_4, G_X_5, G_X_6, G_X_7}},
        {{G_Y_0, G_Y_1, G_Y_2, G_Y_3, G_Y_4, G_Y_5, G_Y_6, G_Y_7}},
        false
    };
    point temp = { .infinity = true };
    point Q = { .infinity = true };

    for (int i = 7; i >= 0; --i) {
        for (int j = 31; j >= 0; --j) {
            if (!Q.infinity) {
                point_add(&Q, &Q, &temp);
                Q = temp;
            }
            if (k->d[i] & (1U << j)) {
                if (Q.infinity) {
                    Q = G;
                } else {
                    point_add(&Q, &G, &temp);
                    Q = temp;
                }
            }
        }
    }
    *res = Q;
}

// Main kernel
__kernel void generateAddresses(__global uchar *output, __global const uchar *passphrases, ulong totalPhrases) {
    size_t gid = get_global_id(0);
    if (gid >= totalPhrases) return;

    // Output buffer: 34 (P2PKH comp) + 34 (P2PKH uncomp) + 64 (priv key) + 52 (WIF)
    size_t offset = gid * (34 + 34 + 64 + 52);

    // Read passphrase
    __global const uchar *passphrase = passphrases + gid * 101;
    uint passLen = 0;
    for (uint i = 0; i < 100 && passphrase[i] != 0; ++i) {
        passLen++;
    }

    uchar passphrase_local[100];
    for (uint i = 0; i < passLen && i < 100; ++i) {
        passphrase_local[i] = passphrase[i];
    }

    // SHA256 for private key
    uchar hash[32];
    uchar priv_key_bytes[32];
    sha256(passphrase_local, passLen, hash);
    for (uint i = 0; i < 32; ++i) {
        priv_key_bytes[i] = hash[i];
    }

    // Private key in hex
    for (int i = 0; i < 32; ++i) {
        uchar byte = priv_key_bytes[i];
        output[offset + 68 + i * 2] = (byte >> 4) + (byte >> 4 < 10 ? '0' : 'a' - 10);
        output[offset + 68 + i * 2 + 1] = (byte & 0x0F) + ((byte & 0x0F) < 10 ? '0' : 'a' - 10);
    }
    output[offset + 132] = 0;

    // secp256k1 public key
    bignum priv_key;
    for (int i = 0; i < 8; ++i) {
        priv_key.d[i] = (priv_key_bytes[i * 4] << 24) |
                        (priv_key_bytes[i * 4 + 1] << 16) |
                        (priv_key_bytes[i * 4 + 2] << 8) |
                        priv_key_bytes[i * 4 + 3];
    }

    point pub_key;
    point_mul(&priv_key, &pub_key);

    uchar pubkey_comp[33], pubkey_uncomp[65];
    pubkey_uncomp[0] = 0x04;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 4; ++j) {
            pubkey_uncomp[1 + i * 4 + j] = (pub_key.x.d[i] >> (24 - j * 8)) & 0xff;
            pubkey_uncomp[33 + i * 4 + j] = (pub_key.y.d[i] >> (24 - j * 8)) & 0xff;
        }
    }
    pubkey_comp[0] = (pub_key.y.d[0] & 1) ? 0x03 : 0x02;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 4; ++j) {
            pubkey_comp[1 + i * 4 + j] = (pub_key.x.d[i] >> (24 - j * 8)) & 0xff;
        }
    }

    // P2PKH addresses
    uchar hash160[20], address[25];
    uchar p2pkh_comp[34] = {0}, p2pkh_uncomp[34] = {0};

    // Compressed address
    sha256(pubkey_comp, 33, hash);
    ripemd160(hash, 32, hash160);
    address[0] = 0x00;
    for (int i = 0; i < 20; ++i) address[i + 1] = hash160[i];
    sha256(address, 21, hash);
    sha256(hash, 32, hash);
    for (int i = 0; i < 4; ++i) address[21 + i] = hash[i];
    base58_encode(address, 25, p2pkh_comp, 34);
    for (int i = 0; i < 34 && p2pkh_comp[i] != 0; ++i) {
        output[offset + i] = p2pkh_comp[i];
    }

    // Uncompressed address
    sha256(pubkey_uncomp, 65, hash);
    ripemd160(hash, 32, hash160);
    address[0] = 0x00;
    for (int i = 0; i < 20; ++i) address[i + 1] = hash160[i];
    sha256(address, 21, hash);
    sha256(hash, 32, hash);
    for (int i = 0; i < 4; ++i) address[21 + i] = hash[i];
    base58_encode(address, 25, p2pkh_uncomp, 34);
    for (int i = 0; i < 34 && p2pkh_uncomp[i] != 0; ++i) {
        output[offset + 34 + i] = p2pkh_uncomp[i];
    }

    // WIF
    uchar wif[38] = {0};
    wif[0] = 0x80;
    for (int i = 0; i < 32; ++i) wif[i + 1] = priv_key_bytes[i];
    wif[33] = 0x01;
    sha256(wif, 34, hash);
    sha256(hash, 32, hash);
    for (int i = 0; i < 4; ++i) wif[34 + i] = hash[i];
    base58_encode(wif, 38, output + offset + 132, 52);
}
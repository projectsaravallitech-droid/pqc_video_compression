#include "header.h"

KeyTransfer::KeyTransfer() {
    kem = OQS_KEM_new(OQS_KEM_alg_kyber_512);
}

KeyTransfer::~KeyTransfer() {
    if (kem) {
        OQS_KEM_free(kem);
    }
}

bool KeyTransfer::generateReceiverKeyPair(std::vector<uint8_t>& public_key_out, std::vector<uint8_t>& secret_key_out) {
    if (!kem) return false;
    public_key_out.resize(kem->length_public_key);
    secret_key_out.resize(kem->length_secret_key);
    return OQS_KEM_keypair(kem, public_key_out.data(), secret_key_out.data()) == OQS_SUCCESS;
}

bool KeyTransfer::senderEncapsulate(const std::vector<uint8_t>& receiver_pub_key, std::vector<uint8_t>& ciphertext_out, std::vector<uint8_t>& shared_secret_out) {
    if (!kem || receiver_pub_key.size() != kem->length_public_key) return false;
    ciphertext_out.resize(kem->length_ciphertext);
    shared_secret_out.resize(kem->length_shared_secret);
    return OQS_KEM_encaps(kem, ciphertext_out.data(), shared_secret_out.data(), receiver_pub_key.data()) == OQS_SUCCESS;
}

bool KeyTransfer::receiverDecapsulate(const std::vector<uint8_t>& secret_key, const std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& shared_secret_out) {
    if (!kem || secret_key.size() != kem->length_secret_key || ciphertext.size() != kem->length_ciphertext) return false;
    shared_secret_out.resize(kem->length_shared_secret);
    return OQS_KEM_decaps(kem, shared_secret_out.data(), ciphertext.data(), secret_key.data()) == OQS_SUCCESS;
}

//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2020 Drift Solutions / Indy Sams            |
|        More information available at https://www.shoutirc.com        |
|                                                                      |
|                    This file is part of RadioBot.                    |
|                                                                      |
|   RadioBot is free software: you can redistribute it and/or modify   |
| it under the terms of the GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or   |
|                 (at your option) any later version.                  |
|                                                                      |
|     RadioBot is distributed in the hope that it will be useful,      |
|    but WITHOUT ANY WARRANTY; without even the implied warranty of    |
|     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the     |
|             GNU General Public License for more details.             |
|                                                                      |
|  You should have received a copy of the GNU General Public License   |
|  along with RadioBot. If not, see <https://www.gnu.org/licenses/>.   |
\**********************************************************************/
//@AUTOHEADER@END@

#include "ircbot.h"
#if defined(ENABLE_OPENSSL)
#include <openssl/x509.h>
#include <openssl/rsa.h>

/* Generates a 2048-bit RSA key. */
EVP_PKEY * make_priv_key() {
    /* Allocate memory for the EVP_PKEY structure. */
    EVP_PKEY * pkey = EVP_PKEY_new();
    if (pkey == NULL) {
        ib_printf(_("%s: Error creating private key!\n"), IRCBOT_NAME);
        return NULL;
    }

    /* Generate the RSA key and assign it to pkey. */
    RSA * rsa = RSA_generate_key(2048, RSA_F4, NULL, NULL);
    if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
		ib_printf(_("%s: Error generating 2048-bit RSA key!\n"), IRCBOT_NAME);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    /* The key has been generated, return it. */
    return pkey;
}

/* Generates a self-signed x509 certificate. */
X509 * make_x509(EVP_PKEY * pkey) {
    /* Allocate memory for the X509 structure. */
    X509 * x509 = X509_new();
    if (x509 == NULL) {
		ib_printf(_("%s: Error creating X509 structure!\n"), IRCBOT_NAME);
        return NULL;
    }

    /* Set the serial number. */
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    /* This certificate is valid from now until exactly one year from now. */
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 60 * 60 * 24 * 365 * 10);

    /* Set the public key for our certificate. */
    X509_set_pubkey(x509, pkey);

    /* We want to copy the subject name to the issuer name. */
    X509_NAME * name = X509_get_subject_name(x509);

    /* Set the country code and common name. */
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (unsigned char *)"US",        -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (unsigned char *)"ShoutIRC RadioBot", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"*", -1, -1, 0);

    /* Now set the issuer name. */
    X509_set_issuer_name(x509, name);

    /* Actually sign the certificate with our key. */
    if(!X509_sign(x509, pkey, EVP_sha256())) {
		ib_printf(_("%s: Error signing certificate!\n"), IRCBOT_NAME);
        X509_free(x509);
        return NULL;
    }

    return x509;
}

bool make_tls_cert(const char * fn) {
    /* Generate the key. */
    ib_printf(_("%s: Creating TLS certificate...\n"), IRCBOT_NAME);

    EVP_PKEY * pkey = make_priv_key();
    if (pkey == NULL) {
        return false;
	}

    /* Generate the certificate. */
    X509 * x509 = make_x509(pkey);
    if(x509 == NULL) {
        EVP_PKEY_free(pkey);
        return false;
    }

	BIO * fp = BIO_new_file(fn, "wb");
	if (fp == NULL) {
	    ib_printf(_("%s: Error opening file '%s' for output!\n"), IRCBOT_NAME, fn);
		EVP_PKEY_free(pkey);
		X509_free(x509);
        return false;
	}

    /* Write the key to disk. */
    bool ret = PEM_write_bio_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
    if (!ret) {
	    ib_printf(_("%s: Error writing private key to file!\n"), IRCBOT_NAME);
		EVP_PKEY_free(pkey);
		X509_free(x509);
		BIO_free(fp);
        return false;
	}

	BIO_write(fp, "\r\n", 2);

    /* Write the certificate to disk. */
    ret = PEM_write_bio_X509(fp, x509);
    if (!ret) {
	    ib_printf(_("%s: Error writing x509 cert to file!\n"), IRCBOT_NAME);
		EVP_PKEY_free(pkey);
		X509_free(x509);
		BIO_free(fp);
        return false;
	}

    EVP_PKEY_free(pkey);
    X509_free(x509);
	BIO_free(fp);

	return ret;
}

#endif

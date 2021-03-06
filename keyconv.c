#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include "winglue.h"
#endif

#include "util.h"

const char *version = "0.17";


static void
usage(const char *progname)
{
	fprintf(stderr,
"Vanitygen keyconv %s\n"
"Usage: %s [-8] [-e|-E <password>] [<key>]\n"
"-8            Output key in PKCS#8 form\n"
"-e            Encrypt output key, prompt for password\n"
"-E <password> Encrypt output key with <password> (UNSAFE)\n",
		version, progname);
}


int
main(int argc, char **argv)
{
	char pwbuf[128];
	char ecprot[128];
	char pbuf[1024];
	const char *key_in;
	const char *pass_in = NULL;
	EC_KEY *pkey;
	int parameter_group = -1;
	int privtype, addrtype;
	int pkcs8 = 0;
	int pass_prompt = 0;
	int opt;
	int res;

	while ((opt = getopt(argc, argv, "8E:e")) != -1) {
		switch (opt) {
		case '8':
			pkcs8 = 1;
			break;
		case 'E':
			if (pass_prompt) {
				usage(argv[0]);
				return 1;
			}
			pass_in = optarg;
			if (!vg_check_password_complexity(pass_in, 1))
				fprintf(stderr,
					"WARNING: Using weak password\n");
			break;
		case 'e':
			if (pass_in) {
				usage(argv[0]);
				return 1;
			}
			pass_prompt = 1;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		res = fread(pbuf, 1, sizeof(pbuf) - 1, stdin);
		pbuf[res] = '\0';
		key_in = pbuf;
	} else {
		key_in = argv[optind];
	}

	OpenSSL_add_all_algorithms();

	pkey = EC_KEY_new_by_curve_name(NID_secp256k1);

	res = vg_decode_privkey_any(pkey, &privtype, key_in, NULL);
	if (res < 0) {
		if (EVP_read_pw_string(pwbuf, sizeof(pwbuf),
				       "Enter import password:", 0) ||
		    !vg_decode_privkey_any(pkey, &privtype, key_in, pwbuf))
			return 1;
	}

	if (!res) {
		fprintf(stderr, "ERROR: Unrecognized key format\n");
		return 1;
	}

	if (pass_prompt) {
		res = EVP_read_pw_string(pwbuf, sizeof(pwbuf),
					 "Enter password:", 1);
		if (res)
			return 1;
		pass_in = pwbuf;
		if (!vg_check_password_complexity(pwbuf, 1))
			fprintf(stderr, "WARNING: Using weak password\n");
	}

	switch (privtype) {
	case 128: addrtype = 0; break;
	case 239: addrtype = 111; break;
	default:  addrtype = 0; break;
	}

	if (pkcs8) {
		res = vg_pkcs8_encode_privkey(pbuf, sizeof(pbuf),
					      pkey, pass_in);
		if (!res) {
			fprintf(stderr,
				"ERROR: Could not encode private key\n");
			return 1;
		}
		printf("%s", pbuf);
	}

	else if (pass_in) {
		res = vg_protect_encode_privkey(ecprot, pkey, privtype,
						parameter_group, pass_in);

		if (!res) {
			fprintf(stderr, "ERROR: could not password-protect "
				"private key\n");
			return 1;
		}

		vg_encode_address(pkey, addrtype, pwbuf);
		printf("Address: %s\n", pwbuf);
		printf("Protkey: %s\n", ecprot);
	}

	else {
		vg_encode_address(pkey, addrtype, ecprot);
		printf("Address: %s\n", ecprot);
		vg_encode_privkey(pkey, privtype, ecprot);
		printf("Privkey: %s\n", ecprot);
	}

	OPENSSL_cleanse(pwbuf, sizeof(pwbuf));

	EC_KEY_free(pkey);
	return 0;
}

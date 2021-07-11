#pragma once
#include <guiddef.h>
#include <combaseapi.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/pkcs12.h>
#include <rpcdce.h>

typedef GUID uuid_t;
typedef __int32 u_int32_t;


EVP_PKEY* PEM_read_PrivateKey(FILE* fp, EVP_PKEY** x, pem_password_cb* cb, void* u) {
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	void* buf = malloc(size);
	fseek(fp, 0, SEEK_SET);
	int a = fread(buf, 1, size, fp);
	BIO* bioPK = BIO_new_mem_buf(buf, a);
	X509* x509;
	x509 = PEM_read_bio_PrivateKey(bioPK, x, cb, u);
	if (x != NULL) *x = x509;
	return x509;
}

int PEM_write_PrivateKey(FILE* fp, EVP_PKEY* x, const EVP_CIPHER* enc, unsigned char* kstr, int klen, pem_password_cb* cb, void* u) {
	BIO* bioPK = BIO_new(BIO_s_mem());
	if (bioPK == NULL)return 1;
	PEM_write_bio_PrivateKey(bioPK, x, enc, kstr, klen, cb, u);
	BUF_MEM* mem;
	BIO_get_mem_ptr(bioPK, &mem);
	fwrite(mem->data, mem->length, 1, fp);
	return 0;
}

X509* PEM_read_X509(FILE* fp, X509** x, pem_password_cb* cb, void* u) {
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	void* buf = malloc(size);
	fseek(fp, 0, SEEK_SET);
	int a = fread(buf, 1, size, fp);
	BIO* bioPK = BIO_new_mem_buf(buf, a);
	X509* x509; 
	x509 = PEM_read_bio_X509 (bioPK, x, cb, u);
	if(x != NULL) *x = x509;
	return x509;
}

int PEM_write_X509(FILE* fp, X509* x) {
	BIO* bioPK = BIO_new(BIO_s_mem());
	if (bioPK == NULL)return 1;
	PEM_write_bio_X509(bioPK, x);
	BUF_MEM* mem;
	BIO_get_mem_ptr(bioPK, &mem);
	fwrite(mem->data, mem->length, 1, fp);
	return 0;
}

int i2d_PKCS12_fp(FILE* fp, PKCS12* cert) {
	BIO* bioPK = BIO_new(BIO_s_mem());
	if (bioPK == NULL)return 1;
	i2d_PKCS12_bio(bioPK, cert);
	BUF_MEM* mem;
	BIO_get_mem_ptr(bioPK, &mem);
	fwrite(mem->data, mem->length, 1, fp);
	return 0;
}

int uuid_generate_random(uuid_t *uuid) {
	return CoCreateGuid(uuid);
}

int uuid_unparse(uuid_t *uuid, char str[37]) {
	sprintf(str,
		"%02lx-%02lx-%02lx-%02lx",
		uuid->Data1,uuid->Data2,uuid->Data3,uuid->Data4		
	);
}
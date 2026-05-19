#include <stdint.h>
#include "base64.h"

/** @brief Base64 alphabet used for encoding. */
const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Calculates the buffer size required to hold a base64-encoded string.
 *
 * @param inlen Length of the input data in bytes.
 * @return Size of the base64 output, not including a null terminator.
 */
static size_t b64EncodedSize(size_t inlen)
{
	size_t ret;

	ret = inlen;
	if (inlen % 3 != 0)
		ret += 3 - (inlen % 3);
	ret /= 3;
	ret *= 4;

	return ret;
}

/**
 * @brief Encodes binary data as a null-terminated base64 string.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller
 * using delete[].
 *
 * @param in  Pointer to the input data to encode.
 * @param len Length of the input data in bytes.
 * @return    Newly allocated null-terminated base64 string, or NULL if @p in
 *            is NULL or @p len is zero.
 */
char* b64Encode(unsigned char* in, size_t len)
{
	char*    out;
	size_t   elen;
	size_t   i;
	size_t   j;
	uint32_t v;

	if (in == NULL || len == 0)
		return NULL;

	elen = b64EncodedSize(len);
	out  = new char[elen+1];
	out[elen] = '\0';

	for (i=0, j=0; i<len; i+=3, j+=4) {
		v = in[i];
		v = i+1 < len ? v << 8 | in[i+1] : v << 8;
		v = i+2 < len ? v << 8 | in[i+2] : v << 8;

		out[j]   = b64chars[(v >> 18) & 0x3F];
		out[j+1] = b64chars[(v >> 12) & 0x3F];
		if (i+1 < len) {
			out[j+2] = b64chars[(v >> 6) & 0x3F];
		} else {
			out[j+2] = '=';
		}
		if (i+2 < len) {
			out[j+3] = b64chars[v & 0x3F];
		} else {
			out[j+3] = '=';
		}
	}

	return out;
}

///////////////////////////////////////////////////////////////////////////////
// BGStringTemplates.h
// Written by Bjoern Ganster since 2004
// Licensed under the LGPL
// Template versions of basic string functions
///////////////////////////////////////////////////////////////////////////////

#ifndef BGSTRINGTEMPLATES__H
#define BGSTRINGTEMPLATES__H

#include "BGBase.h"

#include <cstdint>

#define FILE_NUMBER FILE_NO_BGSTRINGTEMPLATES_H

///////////////////////////////////////////////////////////////////////////////
// bgstrlen, version with MaxChars

template<typename C>
size_t bgstrlen (const C* aStr, size_t MaxChars)
{
	if (aStr == nullptr) {
		return 0;
	}

	// Find end of aStr without exceeding MaxChars
	//SET_SCOPE;
	size_t i = 0;

	while (i < MaxChars && aStr[i] != 0) {
		i++;
	}

	return i;
}

///////////////////////////////////////////////////////////////////////////////
// bgstrlen, version without MaxChars

template<typename C>
size_t bgstrlen (const C* aStr)
{
	if (aStr == nullptr) {
		return 0;
	}

	SET_SCOPE;
	size_t i = 0;

	// Find end of a 
	while (aStr[i] != 0) {
		i++;
	}

	RETURN i;
}

///////////////////////////////////////////////////////////////////////////////
// Compare two strings - they may have different character size, but must
// use the same code point numbers

template<typename C, typename D>
int bgstrcmp (const C* a, const D* b)
{
	// If the pointers are equal, the strings are equal
	if (a == (C*) b) {
		return 0;
	}

	// If at least one of the strings is NULL, it's an easy decision
	SET_SCOPE;
	if (a == NULL) {
		/*if (b == NULL) { // we know that a != b
			RETURN 0;
		} else {*/
			if (b[0] != 0) {
				RETURN -1;
			} else {
				RETURN 0;
			}
		//}
	} else { // a != NULL
		if (a[0] != 0) {
			if (b == NULL) {
				RETURN 1;
			}
		} else if (b == NULL) {
			RETURN 0;
		}
	}

	// Compare until we reach a difference or the end of at least one string
	SET_OP;
	size_t i = 0;
	while (true) {
		if (a[i] == 0 || b[i] == 0 || a[i] != b[i]) {
			RETURN a[i]-b[i];
		}
		++i;
	}

	RETURN 0;
}

///////////////////////////////////////////////////////////////////////////////
// Compare two strings, length-limited - strings may have different character 
// size, but must use the same code point numbers

// Since max applies to a and b simultaneously, passing the length of the shorter
// string to this function could result in an incomplete or wrong result ...

template<typename C, typename D>
int bgstrcmp(const C* a, const D* b, size_t max)
{
	// If the pointers are equal, the strings are equal
	if (a == (C*) b || max == 0) {
		return 0;
	}

	// If at least one of the strings is NULL, it's an easy decision
	SET_SCOPE;
	if (a == NULL) {
		/*if (b == NULL) { // we know that a != b
			RETURN 0;
		} else {*/
			if (b[0] != 0) {
				RETURN -1;
			} else {
				RETURN 0;
			}
		//}
	} else { // a != NULL
		if (a[0] != 0) {
			if (b == NULL) {
				RETURN 1;
			}
		} else if (b == NULL) {
			RETURN 0;
		}
	}

	// Compare until we reach a difference or the end of at least one string
	SET_OP;
	size_t i = 0;
	while (i < max) {
		if (a[i] == 0 || b[i] == 0 || a[i] != b[i]) {
			RETURN a[i]-b[i];
		}
		++i;
	}

	RETURN 0;
}

///////////////////////////////////////////////////////////////////////////////
// String copy with size limit
// Assumes direct conversion betweeen C and D is possible (if necessary)
// Returns the number of characters copied

template<typename C, typename D>
size_t bgstrcpy (C* target, const D* source, size_t maxCount)
{
	if (target == NULL || maxCount == 0) {
		return 0;
	}

	SET_SCOPE;
	size_t i = 0;
	if (source != nullptr) {
		while (i < maxCount-1 && source[i] != 0) {
			target[i] = (C)(source[i]);
			++i;
		}
	}

	if (i < maxCount) {
		target[i] = 0;
	} else {
		target[maxCount-1] = 0; // Safe, because maxCount == 0 is an early exit
	}

	RETURN i;
}

///////////////////////////////////////////////////////////////////////////////
// bgstrcat

template<typename S, typename T>
void bgstrcat (T* a, const S* b, size_t MaxChars)
{
	if (a == nullptr || b == nullptr) {
		return;
	}

	// Find end of a 
	SET_SCOPE;
	size_t i = 0;
	while (i < MaxChars-1 && a[i] != 0) {
		i++;
	}

	// Append b to a
	size_t j = 0;
	while (i+j < MaxChars-1 && b[j] != 0) {
		a[i+j] = b[j];
		j++;
	}

	// Null-Terminate result
	a[i+j] = 0;
	RETURN;
}

///////////////////////////////////////////////////////////////////////////////
// Scan a string for a character
// Find c with a search from start to stop, assuming start <= stop

template<typename C>
bool bgstrscan (const C* str, C c, size_t start, size_t stop, size_t& pos)
{
	SET_SCOPE;
	if (str == NULL) {
		RETURN false;
	}

	pos = start;

	while (str[pos] != c && pos < stop)
		pos++;

	bool result = false;
	if (pos < stop) {
		result = (str[pos] == c);
	}

	RETURN result;
}

///////////////////////////////////////////////////////////////////////////////
// Reverse scan for a character

template<typename C>
bool bgstrrscan (const C* str, C c, size_t start, size_t stop, size_t& pos)
{
	SET_SCOPE;
	if (str == NULL) {
		RETURN false;
	}

	SET_OP;
	pos = start;

	while (str[pos] != c && pos > stop) {
		pos--;
	}

	SET_OP;
	bool result = (str[pos] == c);
	RETURN result;
}

// Replace all occurrences of seekChar in str with replChar and return the number of replacements
template <typename C>
size_t replace (C* str, C seekChar, C replChar)
{
	SET_SCOPE;
	size_t replaced = 0;

	if (str != nullptr) {
		while (str[0] != 0) {
			if (str[0] == seekChar) {
				str[0] = replChar;
				++replaced;
			}
			++str;
		}
	}

	RETURN replaced;
}


// Check if str begins with substr
template<typename C>
bool beginsWith(const C* str, const C* substr)
{
	SET_SCOPE;
	if (substr == nullptr) {
		RETURN true;
	}

	if (str == nullptr && substr[0] != 0) {
		RETURN false;
	}

	while (substr[0] != 0) {
		if (str[0] != substr[0]) {
			RETURN false;
		}
		++str;
		++substr;
	}

	RETURN true;
}

template<typename C>
int64_t bgstrToI(const C* str, int base = 10)
{
	if (str == nullptr || base < 2 || base > 36) {
		return 0;
	}
	
	SET_SCOPE;
	int64_t sign = 1;
		if (str[0] == '-') {
		sign = -1;
			++str;
		}

	int64_t result = 0;
	bool goOn = true;
	do {
		C c = str[0];
		int c1 = c - '0';
		if (c1 >= 0 && c1 < base) {
			result = base * result + sign * c1;
		} else {
			int c2 = c - 'a' + 10;
			if (c2 >= 0 && c2 < base) {
				result = base * result + sign * c2;
			} else {
				int c3 = c - 'A' + 10;
				if (c3 >= 0 && c3 < base) {
					result = base * result + sign * c3;
				} else {
					goOn = false;
				}
			}
		}
		++str;
	} while (goOn);

		RETURN result;
}

template<typename C>
double bgstrToD(const C* str)
{
	SET_SCOPE;
	double result = 0.0;
	bool negate = false;

	if (str != nullptr) {
		if (str[0] == '-') {
			negate = true;
			++str;
		}
		while (str[0] >= '0' && str[0] <= '9') {
			int cipher = str[0] - '0';
			result = 10 * result + cipher;
			++str;
		}

		if (str[0] == '.' || str[0] == ',') {
			++str;
			double factor = 0.1;
			while (str[0] >= '0' && str[0] <= '9') {
				int cipher = str[0] - '0';
				result += factor * cipher;
				factor *= 0.1;
				++str;
			}
		}
	}

	if (negate) {
		RETURN -result;
	} else {
		RETURN result;
	}
}

template<typename C>
C latinLowerCase(C c)
{
	SET_SCOPE;
	if (c >= 'A' && c <= 'Z') {
		RETURN (c - 'A' + 'a');
	} else {
		RETURN c;
	}
}

template<typename C>
void copyToLowerCase(C* target, const C* source)
{
	SET_SCOPE;
	
	if (target != nullptr) {
		if (source != nullptr) {
			SET_OP;
			while(*source != 0) {
				*target = latinLowerCase(*source);
				++source;
				++target;
			}
		}
		SET_OP;
		*target = 0;
	}
	
	RETURN;
}

template<typename C>
void appendInt(C* chars, int64_t num, int base, size_t maxCount)
{
	SET_SCOPE;
	if (num == 0) {
		bgstrcat(chars, "0", maxCount);
	} else {
		size_t start = bgstrlen(chars, maxCount);
		if (num < 0) {
			chars[start] = '-';
			num = -num;
			++start;
		}
		int digits = 0;
		char buf[50];
		const char* digitChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		// Build an array with the digits, reversed
		SET_OP;
		while (num != 0) {
			int digit = (num % base);
			num = num / base;
			buf[digits] = digitChars[digit];
			++digits;
		}

		// If the buffer does not suffice, copy as much as possible without exceeding the buffer
		SET_OP;
		while (start + 1 < maxCount && digits > 0) {
			--digits;
			chars[start] = buf[digits];
			++start;
		}

		chars[start] = 0;
	}

	RETURN;
}

#undef FILE_NUMBER

#endif

////////////////////////////////////////////////////////////////////////////////
// BGCPString implementation
// A class that stores a string and knows its codepage
// Copyright Bjoern Ganster 
// Developed since 2011
////////////////////////////////////////////////////////////////////////////////

#include "BGBase.h"
#include "BGCPString.h"
#include "BGException.h"

#include <assert.h>

#include <ctime>
#include <iostream>

#define FILE_NUMBER FILE_NO_BGCPSTRING_CPP

const BGCPString BGCPString::EOL = "\n";
#ifdef _WIN32
const BGCPString BGCPString::OSPathSepStr = "\\";
#else
const BGCPString BGCPString::OSPathSepStr = "/";
#endif

size_t lengthIfOK(size_t len, size_t maxLen)
{
	if (len <= maxLen)
		return len;
	else
		return 0;
}

// Return number of bytes used by one character - code pages 932 to 950 still need testing
// A return value of 0 indicates an invalid encoding
size_t getCharSize(const void* word, WORD cp, size_t maxLen)
{
	SET_SCOPE;
	if (word == NULL || maxLen < 1) {
		RETURN 0;
	}

	const unsigned char* cptr = ((const unsigned char*)(word));
	const char16_t* wptr = ((const char16_t*)(word));
	switch (cp) {
	case CP_UTF8:
		// We could test validity of the characters - multibyte UTF-8 sequences
		// require the highest bit set in all character bytes after the first
		if ((cptr[0] & 128) == 0) {
			RETURN 1;
		} else {
			// todo: test if subsequent bytes start with bits 10
			if ((cptr[0] & 224) == 192) {
				SET_OP;
				size_t result = lengthIfOK(2, maxLen);
				RETURN result;
			} else {
				if ((cptr[0] & 240) == 224) {
					SET_OP;
					size_t result = lengthIfOK(3, maxLen);
					RETURN result;
				} else {
					SET_OP;
					size_t result = lengthIfOK(4, maxLen);
					RETURN result;
				}
			}
		}
	case CP_UTF16_LE:
		if (maxLen < 2) {
			RETURN 0;
		}
		if (wptr[0] >= 0xD800 && wptr[0] <= 0xDBFF) {
			if (maxLen < 4) {
				RETURN 0;
			}
			if (wptr[1] >= 0xDC00 && wptr[1] <= 0xDFFF) {
				RETURN 4;
			} else {
				RETURN 0; // Cut-off surrogate pair
			}
		} else {
			RETURN 2;
		}
	case CP_UTF16_BE:
		if (maxLen < 2) {
			RETURN 0;
		}
		if (wptr[0] >= 0xD800 && wptr[0] <= 0xDBFF) {
			if (maxLen < 4) {
				RETURN 0;
			}
			if (wptr[1] >= 0xDC00 && wptr[1] <= 0xDFFF) {
				RETURN 4;
			} else {
				RETURN 0; // Cut-off surrogate pair
			}
		} else {
			RETURN 2;
		}
	case 932:
		if ((cptr[0] > 0x80 && cptr[0] < 0xa0) || cptr[0] >= 0xe0) {
			SET_OP;
			size_t result = lengthIfOK(2, maxLen);
			RETURN result;
		} else {
			RETURN 1;
		}
	case 936:
	case 949:
	case 950:
		if (cptr[0] > 0x80) {
			SET_OP;
			size_t result = lengthIfOK(2, maxLen);
			RETURN result;
		} else {
			RETURN 1;
		}
	default:
		RETURN 1;
	}

	RETURN 1;
}

int getTerminatorSize(WORD cp)
{
	switch (cp) {
	case CP_UTF32:
		return 4;
	case CP_UTF16_LE:
	case CP_UTF16_BE:
		return 2;
	case 932:
	case 936:
	case 949:
	case 950:
	default:
		return 1;
	}
}

// todo: special codepages for outlook terminate strings with characters 
// that differ from 0, here we assume zero-terminated strings!
void terminateStr(void* StrEnd, WORD cp)
{
	SET_SCOPE;
	int ts = getTerminatorSize(cp);
	memset(StrEnd, 0, ts);
	RETURN;
}

// Test if word is zero-length, i.e. points to a terminator for the given code page
bool isTerminator(const char* word, WORD cp)
{
	SET_SCOPE;
	if (word == NULL) {
		RETURN true;
	}

	int terminatorSize = getTerminatorSize(cp);

	switch (terminatorSize) {
	case 1:
		// UTF-8: valid multibyte tuples always have the high bit set in all bytes 
		RETURN (word[0] == 0);
	case 2:
		RETURN (((const char16_t*)(word))[0] == 0);
	default:
		while (terminatorSize > 0) {
			if (word[0] == 0) {
				++word;
				--terminatorSize;
			} else {
				RETURN false;
			}
		}
		RETURN true;
	}
}

// Return number of bytes in string
size_t getCharCount(const char* word, size_t maxBytes, WORD cp)
{
	SET_SCOPE;
	if (word == NULL) {
		RETURN 0;
	}

	size_t result = 0;
	while (!isTerminator(word, cp) && maxBytes > 0) {
		size_t charSize = getCharSize(word, cp, maxBytes);

		if (charSize > 0 && maxBytes >= charSize) {
			++result;
			maxBytes -= charSize;
			word += charSize;
		} else
			maxBytes = 0; // invalid character - abort counting
	}

	RETURN result;
}

// Return number of bytes used for the given number of characters
size_t countBytesForChars(const char* word, size_t charCount, WORD cp, int maxLen)
{
	SET_SCOPE;
	if (word == NULL) {
		RETURN 0;
	}

	size_t result = 0;
	while (charCount > 0 && !isTerminator(word, cp)) {
		size_t charSize = getCharSize(word, cp, maxLen);
		if (charSize > 0) {
			result += charSize;
			word += charSize;
			--charCount;
			maxLen -= (int)charSize;
		} else
			charCount = 0; // invalid character - abort
	}

	RETURN result;
}

// Default constructor
BGCPString::BGCPString()
: VirtualClass("BGCPString", BGCPStringID),
  m_chars(m_internalBuf),
  m_codepage(CP_ASCII),
  m_byteCount(0),
  m_reserved(internalBufSize),
  m_terminatorSize(1)
{
	m_internalBuf[0] = 0;
}

#define UNEXPECTED_CODEPAGE_IN_STRING_CONSTRUCTION "Unexpected codepage in string construction"

// Construct from a string with known length
// cp alone defines the type for str
BGCPString::BGCPString(const void* str, size_t byteCount, WORD cp)
: VirtualClass("BGCPString", BGCPStringID),
  m_chars(m_internalBuf),
  m_codepage(cp),
  m_byteCount(0),
  m_reserved(internalBufSize)
{
	SET_SCOPE;
	m_internalBuf[0] = 0;
	m_terminatorSize = getTerminatorSize(cp);
	if (cp == CP_UTF16_LE || cp == CP_UTF16_BE) {
		assign((const char16_t*)str, byteCount, cp);
	} else {
		assign((const char*)str, byteCount, cp);
	}
	RETURN;
}

// Construct from a string with unknown length
// cp alone defines the type for str
BGCPString::BGCPString(const void* str, WORD cp)
: VirtualClass("BGCPString", BGCPStringID),
  m_chars(m_internalBuf),
  m_codepage(cp),
  m_byteCount(0),
  m_reserved(internalBufSize)
{
	SET_SCOPE;
	m_internalBuf[0] = 0;
	m_terminatorSize = getTerminatorSize(cp);

	if (cp == CP_UTF16_LE || cp == CP_UTF16_BE) {
		size_t charCount = bgstrlen((char16_t*)str);
		assign(str, 2*charCount, cp);
	} else {
		size_t len = bgstrlen((char*)str);
		assign(str, len, cp);
	}

	RETURN;
}

// Copy constructor
BGCPString::BGCPString(const BGCPString& other)
: VirtualClass("BGCPString", BGCPStringID),
  m_chars(m_internalBuf),
  m_codepage(other.m_codepage),
  m_byteCount(0),
  m_reserved(internalBufSize),
  m_terminatorSize(other.m_terminatorSize)
{
	SET_SCOPE;
	operator=(other);
	RETURN;
}

// Move constructor
BGCPString::BGCPString(BGCPString&& other) 
: VirtualClass("BGCPString", BGCPStringID),
  m_chars(m_internalBuf),
  m_codepage(other.m_codepage),
  m_byteCount(0),
  m_reserved(internalBufSize),
  m_terminatorSize(other.m_terminatorSize)
{
	if (other.m_chars == other.m_internalBuf) {
		// Real copy needed
		operator=(other);
	} else {
		// Steal string from other
		m_chars = other.m_chars;
		m_byteCount = other.m_byteCount;
		m_reserved = other.m_reserved;

		other.m_chars = other.m_internalBuf;
		other.m_internalBuf[0] = 0;
		other.m_byteCount = 0;
		other.m_reserved = other.internalBufSize;
	}
}

// Destructor
BGCPString::~BGCPString()
{
	SET_SCOPE;
	if (m_chars != NULL) {
		m_chars[0] = 0;
		if (m_chars != m_internalBuf) {
			SET_OP;
			traceFree(m_chars, BGCPStringBufID, m_reserved);
		}
	}
	SET_OP;
	m_chars = NULL;
	m_byteCount = 0;
	m_reserved = 0;
	m_terminatorSize = 0;
	RETURN;
}

// Check if the string is empty
bool BGCPString::empty() const
{
	return (m_byteCount == 0);
}

// Determine string length in bytes
size_t BGCPString::getByteCount(const char* str, WORD cp, size_t maxLen)
{
	SET_SCOPE;
	if (str == NULL) {
		RETURN 0;
	}

	const char* curr = str;
	while (!isTerminator(curr, cp) && maxLen > 0) {
		size_t cs = ::getCharSize(curr, cp, maxLen);
		if (cs <= maxLen) {
			curr += cs;
			maxLen -= (int)cs;
		} else {
			maxLen = 0; // end of buffer, abort
		}
	}

	RETURN (curr - str);
}

// Assign copy from other BGCPString
void BGCPString::operator=(const BGCPString& str)
{
	SET_SCOPE;
	m_codepage = str.m_codepage;
	m_terminatorSize = str.m_terminatorSize;
	resizeBuffer(str.m_byteCount);
	memcpy(m_chars, str.m_chars, str.m_byteCount + m_terminatorSize);
	m_byteCount = str.m_byteCount;
	RETURN;
}

// Assign move from other BGCPString
void BGCPString::operator=(BGCPString&& other)
{
	SET_SCOPE;
	clear();

	m_codepage = other.m_codepage;
	m_terminatorSize = other.m_terminatorSize;
	m_byteCount = other.m_byteCount;
	m_reserved = other.m_reserved;

	if (other.m_chars == other.m_internalBuf) {
		m_chars = m_internalBuf;
		memcpy(m_chars, other.m_chars, other.m_byteCount + other.m_terminatorSize);
	} else {
		m_chars = other.m_chars;
		other.m_chars = nullptr;
		other.m_reserved = 0;
		other.m_byteCount = 0;
	}

	RETURN;
}

// Compare to other BGCPString
bool BGCPString::operator==(const BGCPString& other) const
{
	SET_SCOPE;

	// This test can be done without conversion, so do it first
	auto cc1 = getCharCount();
	auto cc2 = other.getCharCount();
	if (cc1 != cc2) {
		RETURN false;
	}

	// Actually compare the strings
	if (m_codepage == other.m_codepage) {
		int cmpRes = 0;
		if (m_codepage == CP_UTF16_LE || m_codepage == CP_UTF16_BE) {
			const char16_t* p1 = get<char16_t>();
			const char16_t* p2 = other.get<char16_t>();
			cmpRes = bgstrcmp(p1, p2);
		} else {
			const char* p1 = get<char>();
			const char* p2 = other.get<char>();
			cmpRes = bgstrcmp(p1, p2);
		}
		RETURN (cmpRes == 0);
	} else {
		BGCPString tmp1 = (*this);
		tmp1.convertToCP(CP_UTF16_LE);
		BGCPString tmp2 = other;
		tmp2.convertToCP(CP_UTF16_LE);
		char16_t* p1 = tmp1.get<char16_t>();
		char16_t* p2 = tmp2.get<char16_t>();
		int cmpRes = bgstrcmp(p1, p2);
		RETURN (cmpRes == 0);
	}
}

// Copy part of a string
bool BGCPString::assign(const void* str, size_t byteCount, WORD cp)
{
	SET_SCOPE;
	m_codepage = cp;
	m_terminatorSize = getTerminatorSize(cp);
	resizeBuffer(byteCount + m_terminatorSize);
	memcpy(m_chars, str, byteCount);
	setByteCount(byteCount);
	RETURN true;
}

// Set length of string and add terminator
// Does not check whether a new character starts at the given offset!
void BGCPString::setByteCount(size_t newLen)
{
	SET_SCOPE;
	assert(newLen + m_terminatorSize <= m_reserved);
	m_byteCount = newLen;
	terminateStr(m_chars + newLen, m_codepage);
	RETURN;
}

// Clear string
void BGCPString::clear()
{
	SET_SCOPE;
	setByteCount(0);
	RETURN;
}

// Append part of a string
bool BGCPString::appendPart(const char* str, size_t byteCount, WORD cp)
{
	SET_SCOPE;
	if (byteCount == 0) {
		RETURN true;
	}

	if (cp == m_codepage) {
		resizeBuffer(m_byteCount + byteCount);
		memcpy(m_chars + m_byteCount, str, byteCount);
		m_byteCount += byteCount;
	} else {
		BGCPString help;
		help.assign(str, byteCount, cp);
		operator+=(help);
	}

	RETURN true;
}

// Make sure buffer is large enough to hold newSize bytes
void BGCPString::resizeBuffer(size_t newSize)
{
	SET_SCOPE;
	if (newSize + m_terminatorSize >= m_reserved) {
		size_t newReserve = 2 * newSize + m_terminatorSize;
		SET_OP;
		char* newChars = (char*) traceAlloc(newReserve, "BGCPStringBuf", BGCPStringBufID);
		if (newChars == nullptr) { 
			throw BGException(getStackTrace(), "Bad allocation");
		}
		SET_OP;
		memcpy(newChars, m_chars, m_byteCount + m_terminatorSize);
		SET_OP;
		if (m_chars != NULL && m_chars != m_internalBuf) {
			SET_OP;
			traceFree(m_chars, BGCPStringBufID, m_reserved);
		}
		m_chars = newChars;
		m_reserved = newReserve;
	}

	RETURN;
}

// Append another BGCPString
bool BGCPString::operator+=(const BGCPString& other)
{
	SET_SCOPE;
	bool result = true;
	
	if (m_byteCount == 0) {
		operator=(other);
	} else {
		if (m_codepage == other.m_codepage) {
			resizeBuffer(m_byteCount + other.m_byteCount);
			memcpy(&m_chars[m_byteCount], other.m_chars, other.m_byteCount + m_terminatorSize);
			m_byteCount += other.m_byteCount;
		} else {
			if (m_codepage == CP_UTF16_LE) {
				BGCPString copy = other;
				copy.convertToCP(CP_UTF16_LE);
				result = operator+=(copy);
			} else {
				convertToCP(CP_UTF16_LE);
				result = operator+=(other);
			}
		}
	}

	RETURN result;
}

// Convert to codepage
bool BGCPString::convertToCP(WORD cp)
{
	// Cover some simple cases
	SET_SCOPE;
	if (cp == m_codepage) {
		RETURN true;
	}

	// todo: add more shortcuts for common simple cases ...
	if (m_byteCount == 0) {
		SET_OP;
		m_codepage = cp;
		m_terminatorSize = getTerminatorSize(cp);
		terminateStr(m_chars, m_codepage);
		RETURN true;
	}

	if (m_codepage == CP_ASCII && cp == CP_UTF8) {
		m_codepage = CP_UTF8;
		RETURN true;
	}

	if (m_codepage == CP_ASCII && cp == CP_UTF16_LE) {
		SET_OP;
		size_t newCharReserve = m_byteCount + 1;
		char16_t* tempBuf = (char16_t*)traceAlloc(2*newCharReserve, "BGCPStringBuf", BGCPStringBufID);
		if (tempBuf == nullptr) { 
			throw BGException(getStackTrace(), "Bad allocation");
		}
		bgstrcpy(tempBuf, m_chars, newCharReserve);
		if (m_chars != NULL && m_chars != m_internalBuf) {
			traceFree(m_chars, BGCPStringBufID, m_reserved);
		}
		m_chars = (char*) tempBuf;
		m_reserved = 2 * newCharReserve;
		m_terminatorSize = 2;
		m_byteCount *= 2;
		m_codepage = CP_UTF16_LE;
		RETURN true;
	}
	
	// Convert UTF-8 to UTF-16?
	if (m_codepage == CP_UTF8 
	&&  cp == CP_UTF16_LE)
	{
		SET_OP;
		size_t inputOffset = 0;
		size_t charCount = getCharCount();
		// At most, every character requires 4 bytes in UTF-16
		char16_t* temp = (char16_t*)traceAlloc(4*charCount+2, "BGCPStringBuf", BGCPStringBufID);
		size_t outputOffset = 0;
		
		while (inputOffset < m_byteCount) {
			uint32_t c = (unsigned char)(m_chars[inputOffset]);
			
			// Decode input char
			if (c >= 128) {
				if ((c & 224) == 192) {
					++inputOffset;
					if (inputOffset < m_byteCount) {
						unsigned char d = m_chars[inputOffset];
						c = ((c & 0x1f) << 6) + (d & 0x3F);
						++inputOffset;
					}
				} else if ((c & 240) == 224) {
					if (inputOffset+2 < m_byteCount) {
						unsigned char c2 = m_chars[inputOffset+1];
						unsigned char c3 = m_chars[inputOffset+2];
						c = ((c & 15) << 12)+ ((c2 & 0x3F) << 6) + (c3 & 0x3F);
					}
					inputOffset += 3;
				} else { //if ((c & 248) == 240) {
					if (inputOffset+3 < m_byteCount) {
						unsigned char c2 = m_chars[inputOffset+1];
						unsigned char c3 = m_chars[inputOffset+2];
						unsigned char c4 = m_chars[inputOffset+3];
						c = ((c & 7) << 18) + ((c2 & 0x3F) << 12) + ((c3 & 0x3F) << 6) + (c4 & 0x3f);
					}
					inputOffset += 4;
				}
			} else {
				++inputOffset;
			}
			
			// Write output char
			if (c <= 10000) {
				temp[outputOffset] = (uint16_t)(c);
				++outputOffset;
			} else {
				c -= 0x10000;
				uint16_t c1 = 0xd800 + (c >> 10);
				uint16_t c2 = 0xdc00 + (c & 0x3ff);
				temp[outputOffset] = c1;
				++outputOffset;
				temp[outputOffset] = c2;
				++outputOffset;
			}
		}
		
		if (m_chars != nullptr && m_chars != m_internalBuf) {
			traceFree(m_chars, BGCPStringBufID, m_byteCount);
		}
		temp[outputOffset] = 0;
		m_chars = (char*)temp;
		m_byteCount = 2*outputOffset;
		m_codepage = CP_UTF16_LE;
		m_terminatorSize = 2;
		RETURN true;
	}

	// Convert UTF-16 to UTF-8?
	if (m_codepage == CP_UTF16_LE
	&&  cp == CP_UTF8)
	{
		SET_OP;
		size_t inputOffset = 0;
		size_t charCount = getCharCount();
		size_t newBufSize = 4*charCount+20;
		char* temp = (char*)traceAlloc(newBufSize, "BGCPStringBuf", BGCPStringBufID);
		size_t outputOffset = 0;
		char16_t* input = (char16_t*)(m_chars);
		
		while (inputOffset < charCount) {
			// Decode UTF-16
			char32_t c = input[inputOffset];
			if (c >= 0xd800 && c < 0xdc00 && 2*inputOffset+2 < m_byteCount) {
				++inputOffset;
				char16_t d = input[inputOffset];
				++inputOffset;
				// todo: check range of d?
				c = ((c-0xd800) << 10) + (d-0xdc00) + 0x10000;
			} else {
				++inputOffset;
			}
			
			assert(outputOffset < newBufSize);
			
			// Encode UTF-8
			if (c < 128) {
				temp[outputOffset] = (char)c;
				++outputOffset;
			} else if (c < 0x800) {
				temp[outputOffset] = (char) (192 + (c >> 6));
				++outputOffset;
				temp[outputOffset] = (char) (128 + (c & 0x3F));
				++outputOffset;				
			} else if (c <= 0xFFff) {
				temp[outputOffset] = (char) (224 + (c >> 12));
				++outputOffset;
				temp[outputOffset] = (char) (128 + ((c >> 6) & 0x3F));
				++outputOffset;				
				temp[outputOffset] = (char) (128 + (c &0x3F));
				++outputOffset;				
			} else {
				temp[outputOffset] = (char) (240 + (c >> 18));
				++outputOffset;
				temp[outputOffset] = (char) (128 + ((c >> 12) & 0x3F));
				++outputOffset;				
				temp[outputOffset] = (char) (128 + ((c >> 6) & 0x3F));
				++outputOffset;				
				temp[outputOffset] = (char) (128 + (c & 0x3f));
				++outputOffset;				
			}
		}

		if (m_chars != nullptr && m_chars != m_internalBuf) {
			traceFree(m_chars, BGCPStringBufID, m_byteCount);
		}
		temp[outputOffset] = 0;
		m_chars = (char*)temp;
		m_byteCount = outputOffset;
		m_codepage = CP_UTF8;
		m_terminatorSize = 1;

		RETURN true;
	}

	// Use Windows function to convert the string
	SET_OP;
	#ifdef WIN32
	if (m_codepage != CP_UTF16_LE) {
		// Convert to UTF-16
		SET_OP;
		int ts = getTerminatorSize(CP_UTF16_LE);
		int bytesNeeded = 2 * MultiByteToWideChar(m_codepage, 0, m_chars, (int)m_byteCount, NULL, 0);
		SET_OP;
		if (bytesNeeded > 0) {
			SET_OP;
			char* tempBuf = (char*)traceAlloc(bytesNeeded + ts, "BGCPStringBuf", BGCPStringBufID);
			if (tempBuf == nullptr) { 
				throw BGException(getStackTrace(), "Bad allocation");
			}
			SET_OP;
			MultiByteToWideChar(m_codepage, 0, (const char*)m_chars, (int)m_byteCount, 
				(wchar_t*)tempBuf, bytesNeeded + 2);
			if (m_chars != NULL && m_chars != m_internalBuf) {
				SET_OP;
				traceFree(m_chars, BGCPStringBufID, m_reserved);
			}
			m_chars = tempBuf;
			m_codepage = CP_UTF16_LE;
			m_reserved = bytesNeeded + ts;
			m_terminatorSize = ts;
			setByteCount(bytesNeeded);
		} else {
			// Error: non-empty string reduced to zero length
			SET_OP;
			clear();
		}
	}

	if (cp != CP_UTF16_LE) {
		// Convert to final codepage
		// UTF-8 requires flags=0 and both final parameters NULL
		SET_OP;
		int ts = getTerminatorSize(cp);
		int bytesNeeded =
			WideCharToMultiByte(cp, 0, (wchar_t*)m_chars,
				(int)(m_byteCount / 2), NULL, 0, NULL, NULL);

		if (bytesNeeded > 0) {
			SET_OP;
			// Can't use resizeBuffer() because we need a second buffer
			char* tempBuf = (char*)traceAlloc(bytesNeeded + ts, "BGCPStringBuf", BGCPStringBufID);
			if (tempBuf == nullptr) { 
				throw BGException(getStackTrace(), "Bad allocation");
			}
			SET_OP;
			WideCharToMultiByte(cp, 0, (wchar_t*)m_chars,
				(int)(m_byteCount / 2) + 1, tempBuf, bytesNeeded, NULL, NULL);
			SET_OP;
			if (m_chars != NULL && m_chars != m_internalBuf) {
				SET_OP;
				traceFree(m_chars, BGCPStringBufID, m_reserved);
			}
			SET_OP;
			m_chars = tempBuf;
			m_reserved = bytesNeeded + ts;
			m_terminatorSize = ts;
			m_codepage = cp;
			setByteCount(bytesNeeded);
		} else {
			// Error: non-empty string reduced to zero length
			SET_OP;
			clear();
		}
	}

	SET_OP;
	m_terminatorSize = getTerminatorSize(cp);
	RETURN true;
	#else
	SET_OP;
	log("I don't know how to convert a string from cp ", BGCPString::fromInt(m_codepage), 
		" to ", BGCPString::fromInt(cp));
	logStackTrace();
	RETURN false;
	#endif
}

// Create string that represents an integer
BGCPString BGCPString::fromInt(INT64 num, int base)
{
	SET_SCOPE;
	BGCPString result;
	if (num == 0) {
		result.add<char>("0");
	} else {
		int digits = 0;
		const char* digitChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		if (num < 0) {
			result.add<char>("-");
			num = -num;
		}

		// Build an array with the digits, reversed
		const int bufSize = 50;
		char buf[bufSize];
		while (num != 0 && digits < bufSize) {
			int digit = (num % base);
			num = num / base;
			buf[digits] = digitChars[digit];
			++digits;
		}

		while (digits > 0) {
			--digits;
			result.add<char>(&buf[digits]);
		}
	}
	
	RETURN result;
}

// Create a string that represents a double
// custom solution is twice as fast for big data ...
BGCPString BGCPString::fromDouble(double d, int precision)
{
	SET_SCOPE;
	BGCPString result;

	if (d < 0.0) {
		result.add<char>("-");
		d = -d;
	}

	int i = (int) d;
	result += BGCPString::fromInt(i);
	d -= i;

	if (d > 0 && precision > 0) {
		result.add<char>(".");
		const char* digits = "0123456789";
		while (d > 0 && precision > 0) {
			int digit = (int) (10*d);
			result.add<char>(&digits[digit]);
			--precision ;
			d = 10.0 * d - digit;
		}
	}

	RETURN result;
}

// Convert string to a double
double BGCPString::toDouble() const
{
	SET_SCOPE;
	if (empty()) {
		RETURN 0.0;
	}

	double result = 0.0;
	switch (m_codepage) {
		case CP_UTF16_BE:
			{
				BGCPString copy;
				copy.convertToCP(CP_UTF16_LE);
				result = bgstrToD<char16_t>((char16_t*)copy.m_chars);
			}
			break;
		case CP_UTF16_LE:
			result = bgstrToD<char16_t>((char16_t*)m_chars);
			break;
		default:
			result = bgstrToD<char>(m_chars);
			break;
	}
	RETURN result;
}

// Get codepage currently used by the internal representation
WORD BGCPString::getCP() const
{
	return m_codepage;
}

// Get number of bytes used by current internal string representation
// The returned value does not include the terminating zero
size_t BGCPString::getByteCount() const
{
	return m_byteCount;
}

// Get number of characters in current string
size_t BGCPString::getCharCount() const
{
	return ::getCharCount(m_chars, m_byteCount, m_codepage);
}

// Cut off string at specified number of characters
void BGCPString::setCharCount(size_t newCount)
{
	SET_SCOPE;
	size_t i = 0;

	while (newCount > 0 && i < m_reserved) {
		if (!isTerminator(&m_chars[i], m_codepage)) {
			size_t charSize = getCharSize(i);

			if (charSize > 0) {
				i += charSize;
				--newCount;
			} else
				newCount = 0;
		} else
			newCount = 0;

	}

	setByteCount(i);
	RETURN;
}

INT64 BGCPString::toInt() const
{
	SET_SCOPE;
	switch (m_codepage) {
		case CP_UTF16_BE:
			{
				BGCPString copy;
				copy.convertToCP(CP_UTF16_LE);
				RETURN bgstrToI<char16_t>((char16_t*)copy.m_chars);
			}
		case CP_UTF16_LE:
			RETURN bgstrToI<char16_t>((char16_t*)m_chars);
		default:
			RETURN bgstrToI(m_chars);
	}
}

// Append windows error message to string
#ifdef _WIN32
BGCPString BGCPString::getOSError(DWORD winErrCode)
{
	SET_SCOPE;
	BGCPString result;
	const size_t winErrSize = 500;
	TCHAR winErr[winErrSize];
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, winErrCode, 0, (TCHAR*) (&winErr), winErrSize, NULL);
	result = BGCPString(winErr, CP_SYSTEM);
	RETURN result;
}
#else
BGCPString BGCPString::getOSError(int code)
{
	SET_SCOPE;
	switch (code) {
		case E2BIG: RETURN BGCPString("Argument list too long", CP_ASCII);
		case EACCES: RETURN BGCPString("Permission denied", CP_ASCII);
		case EADDRINUSE: RETURN BGCPString("Address already in use", CP_ASCII);
		case EADDRNOTAVAIL: RETURN BGCPString("Address not available", CP_ASCII);
		case EAFNOSUPPORT: RETURN BGCPString("Address family not supported", CP_ASCII);
		case EAGAIN: RETURN BGCPString("Resource temporarily unavailable (may be the same value as EWOULDBLOCK)", CP_ASCII);
		case EALREADY: RETURN BGCPString("Connection already in progress", CP_ASCII);
		case EBADE: RETURN BGCPString("Invalid exchange", CP_ASCII);
		case EBADF: RETURN BGCPString("Bad file descriptor", CP_ASCII);
		case EBADFD: RETURN BGCPString("File descriptor in bad state", CP_ASCII);
		case EBADMSG: RETURN BGCPString("Bad message", CP_ASCII);
		case EBADR: RETURN BGCPString("Invalid request descriptor", CP_ASCII);
		case EBADRQC: RETURN BGCPString("Invalid request code", CP_ASCII);
		case EBADSLT: RETURN BGCPString("Invalid slot", CP_ASCII);
		case EBUSY: RETURN BGCPString("Device or resource busy", CP_ASCII);
		case ECANCELED: RETURN BGCPString("Operation canceled", CP_ASCII);
		case ECHILD: RETURN BGCPString("No child processes", CP_ASCII);
		case ECHRNG: RETURN BGCPString("Channel number out of range", CP_ASCII);
		case ECOMM: RETURN BGCPString("Communication error on send", CP_ASCII);
		case ECONNABORTED: RETURN BGCPString("Connection aborted", CP_ASCII);
		case ECONNREFUSED: RETURN BGCPString("Connection refused", CP_ASCII);
		case ECONNRESET: RETURN BGCPString("Connection reset", CP_ASCII);
		case EDEADLK: RETURN BGCPString("Resource deadlock avoided", CP_ASCII);
		/*case EDEADLOCK: RETURN BGCPString("On most architectures, a synonym for EDEADLK. "
			"On some architectures (e.g., Linux MIPS, PowerPC), it is a separate "
			"error code \"File locking deadlock error\"", CP_ASCII);*/
		case EDESTADDRREQ: RETURN BGCPString("Destination address required", CP_ASCII);       
		case EDOM: RETURN BGCPString("Mathematics argument out of domain of functio", CP_ASCII);
		case EDQUOT: RETURN BGCPString("Disk quota exceeded", CP_ASCII);
		case EEXIST: RETURN BGCPString("File exists", CP_ASCII);
		case EFAULT: RETURN BGCPString("Bad address", CP_ASCII);
		case EFBIG: RETURN BGCPString("File too large", CP_ASCII);
		case EHOSTDOWN: RETURN BGCPString("Host is down", CP_ASCII);
		case EHOSTUNREACH: RETURN BGCPString("Host is unreachable", CP_ASCII);
		case EHWPOISON: RETURN BGCPString("Memory page has hardware error", CP_ASCII);
		case EIDRM: RETURN BGCPString("Identifier removed", CP_ASCII);
		case EILSEQ: RETURN BGCPString(" Invalid or incomplete multibyte or wide characte", CP_ASCII);
		case EINPROGRESS: RETURN BGCPString("Operation in progress", CP_ASCII);
		case EINTR: RETURN BGCPString("Interrupted function call; see signal(7)", CP_ASCII);
		case EINVAL: RETURN BGCPString("Invalid argument", CP_ASCII);
		case EIO: RETURN BGCPString("Input/output error", CP_ASCII);
		case EISCONN: RETURN BGCPString("Socket is connected", CP_ASCII);
		case EISDIR: RETURN BGCPString("Is a directory", CP_ASCII);
		case EISNAM: RETURN BGCPString("Is a named type file", CP_ASCII);
		case EKEYEXPIRED: RETURN BGCPString("Key has expired", CP_ASCII);
		case EKEYREJECTED: RETURN BGCPString("Key was rejected by service", CP_ASCII);
		case EKEYREVOKED: RETURN BGCPString("Key has been revoked", CP_ASCII);
		case EL2HLT: RETURN BGCPString("Level 2 halted", CP_ASCII);
		case EL2NSYNC: RETURN BGCPString("Level 2 not synchronized", CP_ASCII);
		case EL3HLT: RETURN BGCPString("Level 3 halted", CP_ASCII);
		case EL3RST: RETURN BGCPString("Level 3 reset", CP_ASCII);
		case ELIBACC: RETURN BGCPString("Cannot access a needed shared library", CP_ASCII);
		case ELIBBAD: RETURN BGCPString("Accessing a corrupted shared library", CP_ASCII);
		case ELIBMAX: RETURN BGCPString("Attempting to link in too many shared libraries", CP_ASCII);
		case ELIBSCN: RETURN BGCPString(".lib section in a.out corrupte", CP_ASCII);
		case ELIBEXEC: RETURN BGCPString("Cannot exec a shared library directly", CP_ASCII);
		//case ELNRANGE: RETURN BGCPString("Link number out of range", CP_ASCII);
		case ELOOP: RETURN BGCPString("Too many levels of symbolic links", CP_ASCII);
		case EMEDIUMTYPE: RETURN BGCPString("Wrong medium type", CP_ASCII);
		case EMFILE: RETURN BGCPString("Too many open files. Commonly caused by exceeding the "
			"RLIMIT_NOFILE resource limit described in getrlimit(2).", CP_ASCII);
		case EMLINK: RETURN BGCPString("Too many links", CP_ASCII);
		case EMSGSIZE: RETURN BGCPString("Message too long", CP_ASCII);
		case EMULTIHOP: RETURN BGCPString("Multihop attempted", CP_ASCII);
		case ENAMETOOLONG: RETURN BGCPString("Filename too long", CP_ASCII);
		case ENETDOWN: RETURN BGCPString("Network is down", CP_ASCII);
		case ENETRESET: RETURN BGCPString("Connection aborted by network", CP_ASCII);
		case ENETUNREACH: RETURN BGCPString("Network unreachable", CP_ASCII);
		case ENFILE: RETURN BGCPString("Too many open files in system.  On "
			"Linux, this is probably a result of encountering the /proc/sys/fs/file-max limit.", CP_ASCII);
		case ENOANO: RETURN BGCPString("No anode", CP_ASCII);
		case ENOBUFS: RETURN BGCPString("No buffer space available (POSIX.1 (XSI STREAMS option))", CP_ASCII);
		case ENODATA: RETURN BGCPString("No message is available on the STREAM head read queue", CP_ASCII);
		case ENODEV: RETURN BGCPString("No such device", CP_ASCII);
		case ENOENT: RETURN BGCPString("No such file or directory. Typically, this error results when "
			"a specified path name does not exist, or one of the components in the "
			"directory prefix of a pathname does not exist, or the specified pathname "
			"is a dangling symbolic link.", CP_ASCII);
		case ENOEXEC: RETURN BGCPString("Exec format error", CP_ASCII);
		case ENOKEY: RETURN BGCPString("Required key not available", CP_ASCII);
		case ENOLCK: RETURN BGCPString("No locks available", CP_ASCII);
		case ENOLINK: RETURN BGCPString("Link has been severed", CP_ASCII);
		case ENOMEDIUM: RETURN BGCPString("No medium found", CP_ASCII);
		case ENOMEM: RETURN BGCPString("Not enough space/cannot allocate memor", CP_ASCII);
		case ENOMSG: RETURN BGCPString("No message of the desired type", CP_ASCII);
		case ENONET: RETURN BGCPString("Machine is not on the network", CP_ASCII);
		case ENOPKG: RETURN BGCPString("Package not installed", CP_ASCII);
		case ENOPROTOOPT: RETURN BGCPString("Protocol not available", CP_ASCII);
		case ENOSPC: RETURN BGCPString("No space left on device", CP_ASCII);
		case ENOSR: RETURN BGCPString("No STREAM resources (POSIX.1 (XSI STREAMS option))", CP_ASCII);
		case ENOSTR: RETURN BGCPString("Not a STREAM (POSIX.1 (XSI STREAMS option))", CP_ASCII);
		case ENOSYS: RETURN BGCPString("Function not implemented", CP_ASCII);
		case ENOTBLK: RETURN BGCPString("Block device required", CP_ASCII);
		case ENOTCONN: RETURN BGCPString("The socket is not connected", CP_ASCII);
		case ENOTDIR: RETURN BGCPString("Not a directory", CP_ASCII);
		case ENOTEMPTY: RETURN BGCPString("Directory not empty", CP_ASCII);
		case ENOTRECOVERABLE: RETURN BGCPString("State not recoverable (POSIX.1-2008)", CP_ASCII);
		case ENOTSOCK: RETURN BGCPString("Not a socket", CP_ASCII);
		case ENOTSUP: RETURN BGCPString("Operation not supported", CP_ASCII);
		case ENOTTY: RETURN BGCPString("Inappropriate I/O control operation", CP_ASCII);
		case ENOTUNIQ: RETURN BGCPString("Name not unique on network", CP_ASCII);
		case ENXIO: RETURN BGCPString("No such device or address", CP_ASCII);
		//case EOPNOTSUPP: RETURN BGCPString("Operation not supported on socket", CP_ASCII);
		case EOVERFLOW: RETURN BGCPString("Value too large to be stored in data typ", CP_ASCII);
		case EOWNERDEAD: RETURN BGCPString("Owner died (POSIX.1-2008)", CP_ASCII);
		case EPERM: RETURN BGCPString("Operation not permitted", CP_ASCII);
		case EPFNOSUPPORT: RETURN BGCPString("Protocol family not supported", CP_ASCII);
		case EPIPE: RETURN BGCPString("Broken pipe", CP_ASCII);
		case EPROTO: RETURN BGCPString("Protocol error", CP_ASCII);
		case EPROTONOSUPPORT: RETURN BGCPString("Protocol not supported", CP_ASCII);
		case EPROTOTYPE: RETURN BGCPString("Protocol wrong type for socket", CP_ASCII);
		case ERANGE: RETURN BGCPString("Result too large (POSIX.1, C99)", CP_ASCII);
		case EREMCHG: RETURN BGCPString("Remote address changed", CP_ASCII);
		case EREMOTE: RETURN BGCPString("Object is remote", CP_ASCII);
		case EREMOTEIO: RETURN BGCPString("Remote I/O error", CP_ASCII);
		case ERESTART: RETURN BGCPString("Interrupted system call should be restarted", CP_ASCII);
		case ERFKILL: RETURN BGCPString("Operation not possible due to RF-kill", CP_ASCII);
		case EROFS: RETURN BGCPString("Read-only filesystem", CP_ASCII);
		case ESHUTDOWN: RETURN BGCPString("Cannot send after transport endpoint shutdown", CP_ASCII);
		case ESPIPE: RETURN BGCPString("Invalid seek", CP_ASCII);
		case ESOCKTNOSUPPORT: RETURN BGCPString("Socket type not supported", CP_ASCII);
		case ESRCH: RETURN BGCPString("No such process", CP_ASCII);
		case ESTALE: RETURN BGCPString("Stale file handle", CP_ASCII);
		case ESTRPIPE: RETURN BGCPString("Streams pipe error", CP_ASCII);
		case ETIME: RETURN BGCPString("Timer expired", CP_ASCII);
		case ETIMEDOUT: RETURN BGCPString("Connection timed out", CP_ASCII);
		case ETOOMANYREFS: RETURN BGCPString("Too many references: cannot splice", CP_ASCII);
		case ETXTBSY: RETURN BGCPString("Text file busy", CP_ASCII);
		case EUCLEAN: RETURN BGCPString("Structure needs cleaning", CP_ASCII);
		case EUNATCH: RETURN BGCPString("Protocol driver not attached", CP_ASCII);
		case EUSERS: RETURN BGCPString("Too many users", CP_ASCII);
		//case EWOULDBLOCK: RETURN BGCPString("Operation would block (may be same value as EAGAIN)", CP_ASCII);
		case EXDEV: RETURN BGCPString("Improper link", CP_ASCII);
		case EXFULL: RETURN BGCPString("Exchange full", CP_ASCII);
		default:
			RETURN BGCPString("Unknown error code ", CP_ASCII) + BGCPString::fromInt(code);
	}
}
#endif

// Print this string to the console
void BGCPString::print() const
{
	SET_SCOPE;
	#ifdef _WIN32
	switch (m_codepage) {
	case CP_UTF16_LE:
	{
		wprintf(L"%s", (wchar_t*)m_chars);
		break;
	}
	default:
		COUT << ((char*)m_chars);
		break;
	}
	#else
	if (m_codepage == CP_UTF8 || m_codepage == CP_ASCII) {
		printf("%s", m_chars);
	} else {
		BGCPString copy = *this;
		copy.convertToCP(CP_UTF8);
		printf("%s", copy.m_chars);
	}
	#endif
	RETURN;
}

// Print this string to the console and add a line feed
void BGCPString::printLn() const
{
	SET_SCOPE;
	print();
	EOL.print();
	RETURN;
}

// Create size string, e.g. 1024MB for 1024*1024
BGCPString BGCPString::sizeStr(int64_t size)
{
	SET_SCOPE;
	BGCPString result;
	
	if (size < 2 * 1024) {
		// Report size in bytes
		result = BGCPString::fromInt(size);
		result += BGCPString(" bytes");
	} else if (size < 2 * 1024 * 1024) {
		// Report size in kbytes
		result = fromDouble((double) size / 1024.0);
		result += BGCPString(" kB");
	} else if (size < ((int64_t) (1024 * 1024 * 1024))) {
		// Report size in MBytes
		result = fromDouble((double) size / (1024.0 * 1024.0), 1);
		result += BGCPString(" MB");
	} else {
		// Report size in GBytes
		result = fromDouble((double) size / (1024.0 * 1024.0 * 1024.0), 1);
		result += BGCPString(" GB");
	}

	RETURN result;
}

// Check if this string ends with another string
bool BGCPString::endsWith(const BGCPString& other) const
{
	SET_SCOPE;
	if (other.empty()) {
		RETURN true;
	}

	// todo: if other has chars that cannot be expressed in m_codepage, 
	// the following code may fail. Instead, if other has a different CP,
	// both strings should be converted to UTF-16-LE
	BGCPString otherSameCP = other;
	otherSameCP.convertToCP(m_codepage);

	if (otherSameCP.m_byteCount > m_byteCount) {
		RETURN false;
	}

	// This should work for codepages ASCII, CP1252, UTF-8, UTF-16 (LE+BE)
	// It works for unicode because the start byte of a character has a 
	// different encoding than the following bytes ...
	// Not sure about some other cases, though ...
	size_t offset = m_byteCount - otherSameCP.m_byteCount;

	if (m_codepage == CP_UTF16_LE || m_codepage == CP_UTF16_BE) {
		BGCPString rest((char16_t*)(m_chars+offset), m_codepage);
		RETURN(rest == otherSameCP);
	} else {
		BGCPString rest(m_chars+offset, m_codepage);
		RETURN(rest == otherSameCP);
	}
}

// Remove other string from the end of this string
// todo: assumes that the encodings are identical ...
bool BGCPString::reduceBy(const BGCPString& other)
{
	SET_SCOPE;
	if (endsWith(other)) {
		BGCPString otherSameCP = other;
		otherSameCP.convertToCP(m_codepage);
		m_byteCount -= otherSameCP.m_byteCount;
		terminateStr(m_chars + m_byteCount, m_codepage);
		RETURN true;
	} else {
		RETURN false;
	}
}

bool BGCPString::saveToFile(const PathChar* fn) const
{
	return ::saveToFile(m_chars, m_byteCount, fn);
}

// Get the part of a path that defines the directory
// Windows needs special treatment since there are two characters allowed as path separators
BGCPString getDirectory(const BGCPString& filename)
#ifdef _WIN32
{
	BGCPString result = filename;
	const PathChar* pc = result.getAsPath();
	size_t pos = 0;
	size_t lastPathSep = 0;

	for (size_t cc = result.getCharCount(); pos < cc; ++pos) {
		if (pc[pos] == '\\' || pc[pos] == '/') {
			lastPathSep = pos;
		}
	}

	if (lastPathSep > 0) {
		result.setByteCount(2*lastPathSep);
	}

	return result;
}
#else
{
	SET_SCOPE;
	size_t lastPathSep;
	BGCPString result = filename;

	SET_OP;
	if (filename.findLast(OSPathSep, 0, filename.getByteCount(), lastPathSep)) {
		SET_OP;
		result = filename.getPart(0, lastPathSep);
	}

	RETURN result;
}
#endif

BGCPString BGCPString::time(int64_t ms)
{
	if (ms < 1000) {
		return BGCPString::fromInt(ms) + BGCPString(" ms");
	}

	if (ms < 60 * 1000) {
		return BGCPString::fromDouble(ms / 1000.0, 1) + BGCPString("s");
	}

	BGCPString result = BGCPString::fromInt(ms / 60000) + " min";
	ms %= 60000;
	if (ms > 1000) {
		result += BGCPString(" ") + BGCPString::fromInt(ms / 1000) + " s";
	}
	return result;
}

// Load string from file
bool BGCPString::loadFromFile(const PathChar* fn, WORD codepage)
{
	SET_SCOPE;
	size_t size = 0;
	char* buf = (char*) ::loadFromFile(fn, size, "BGCPStringBuf", BGCPStringBufID);
	bool result = true;
	if (buf != nullptr) {
		assign(buf, size, codepage);
		traceFree(buf, BGCPStringBufID, size);
	} else {
		clear();
		result = false;
	}
	RETURN result;
}

// Check if the character at index i equals c in whatever codepage the current string uses
// c is expected to be an ASCII character
bool BGCPString::charAtEquals(size_t i, char c) const
{
	if (m_codepage == CP_UTF16_LE) {
		const wchar_t* pc = getPtr<wchar_t>(i);
		if (pc != nullptr) {
			return ((*pc) == c);
		}
	} else {
		const char* pc = getPtr<char>(i);
		if (pc != nullptr) {
			return ((*pc) == c);
		}
	}

	return (c == 0); // character 0 may match beyond end of string
}

// Get number of bytes used by the character at offset i
size_t BGCPString::getCharSize(size_t i) const
{
	if (i < m_byteCount) {
		return ::getCharSize(m_chars + i, m_codepage, m_byteCount-i);
	} else {
		return 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

MessageListener* logListener = nullptr;

void log(const BGCPString& str)
{
	BGCPString msgPlusEOL = str + BGCPString::EOL;
	logNoLF(msgPlusEOL);
}

void logNoLF(const BGCPString& str)
{
	if (logListener == nullptr) {
		str.print();
	} else {
		BGCPString copy = str; // copy needed: str is const, but receiveMessage's 3rd arg isn't
		logListener->receiveMessage(nullptr, MessageType::Log, &copy);
	}
}

void setLogListener(MessageListener* newListener)
{
	logListener = newListener;
}

///////////////////////////////////////////////////////////////////////////////
// Get directory for temporary files

// todo: get temp dir from environment variables

BGCPString getTempDir()
#ifdef _WIN32
{
	return BGCPString("C:\\temp", CP_ASCII); //todo: use GetTempPath
}
#else
{
	return BGCPString("/tmp", CP_ASCII);
}
#endif


// Get current date and time as a string
BGCPString BGCPString::now()
{
	time_t curr_time;
	std::time(&curr_time);
	tm * curr_tm = std::localtime(&curr_time);

	char date_time_string[100];
	strftime(date_time_string, 50, "%Y-%m-%d %H:%M", curr_tm);
	return BGCPString(date_time_string, CP_ASCII);
}

// Get directory part of a file name
BGCPString BGCPString::getDirectory() const
{
	size_t lastSlashPos = 0;
	if (findLast(OSPathSep, 0, m_byteCount, lastSlashPos)) {
		BGCPString result(m_chars, lastSlashPos, m_codepage);
		return result;
	} else {
		return this;
	}
}

// After writing directly to the internal buffer via a pointer obtained e.g. by getPtr(),
// use this function to update the internal data
void BGCPString::updateData(WORD newCP, size_t newBC)
{
	m_codepage = newCP;
	m_byteCount = newBC;
	m_terminatorSize = getTerminatorSize(newCP);
}

////////////////////////////////////////////////////////////////////////////////
// BGCPString declaration
// A class that stores a string and knows its codepage
// Copyright Bjoern Ganster 
// Developed since 2011
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "BGBase.h"
#include "BGStringTemplates.h"

#include <vector>
#include <string.h> // for memcpy 
#include <wchar.h>

#define FILE_NUMBER FILE_NO_BGCPSTRING_H

// Return number of bytes used by one character - code pages 932 to 950 still need testing
// A return value of 0 indicates an invalid encoding
size_t getCharSize(const void* word, WORD cp, size_t maxLen);

// Get size of a terminator for a codepage
int getTerminatorSize(WORD cp);

// Terminate a string, given the codepage
void terminateStr(void* StrEnd, WORD cp);

// Test if word is zero-length, i.e. points to a terminator for the given code page
bool isTerminator(const char* word, WORD cp);

// Return number of bytes in string
size_t getCharCount(const char* word, size_t maxBytes, WORD cp);

// Return number of bytes used for the given number of characters
size_t countBytesForChars(const char* word, size_t charCount, WORD cp, int maxLen);

////////////////////////////////////////////////////////////////////////////////

class BGCPString FINAL: public VirtualClass {
public:
	// Default constructor
	// To construct from QString: include Qtbridge.h and call QString2BGCPString
	BGCPString();

	// Construct from a string with known length
	// cp alone defines the type for str
	BGCPString(const void* str, size_t byteCount, WORD cp);

	// Construct from a string with unknown length
	// cp alone defines the type for str, type of str can be ignored
	BGCPString(const void* str, WORD cp = CP_ASCII);

	// Copy constructor
	BGCPString(const BGCPString& other);

	// Move constructor
	BGCPString(BGCPString&& other);

	// Create string from an integer
	// This cannot be a constructor for BGCPString because gcc gets confused with types then ...
	static BGCPString fromInt(INT64 num, int base = 10);

	// Destructor
	~BGCPString() OVERRIDE;

	// Check if the string is empty
	bool empty() const;

	// Get string length in terms of bytes
	static size_t getByteCount(const char* str, WORD cp, size_t maxLen);

	// Assign from other BGCPString
	void operator=(const BGCPString& str);

	// Assign move from other BGCPString
	void operator=(BGCPString&& other);

	// Compare to other BGCPString
	bool operator==(const BGCPString& other) const;
	bool operator!=(const BGCPString& other) const
	{
		return !(operator==(other));
	}

	// Copy part of a string
	bool assign(const void* str, size_t byteCount, WORD cp);

	// Set length of string and add terminator
	// Does not check whether a new character starts at the given offset!
	void setByteCount(size_t newLen);

	// Clear string
	void clear();

	// Add a character to the current string
	// Returns the number of bytes used by the character, 0 implies an error
	// charP: pointer to the character
	// codepage: codepage currently used by charP
	// maxLen: maximum length of charP in bytes
	template <typename C>
	size_t add(const C* charP, WORD codepage = CP_ASCII, size_t maxLen = sizeof(C))
	{
		SET_SCOPE;
		size_t byteCount = ::getCharSize(charP, codepage, maxLen);

		SET_OP;
		if (m_byteCount == 0) {
			m_codepage = codepage;
			m_terminatorSize = getTerminatorSize(codepage);
		}

		if (byteCount > 0) {
			if ((codepage == m_codepage)
			|| ((codepage == CP_ASCII) && (m_codepage == CP_UTF8)))
			{
				// Append without conversion
				SET_OP;
				resizeBuffer(m_byteCount + byteCount);
				memcpy(m_chars+m_byteCount, charP, byteCount);
				m_byteCount += byteCount;
				terminateStr(m_chars + m_byteCount, m_codepage);
			} else {
				// Append with conversion
				SET_OP;
				BGCPString help;
				help.assign(charP, byteCount, codepage);
				operator+=(help);
			}
		}

		RETURN byteCount;
	}

	// Append part of a string
	bool appendPart(const char* str, size_t byteCount, WORD cp);

	// Make sure buffer is large enough to hold newSize bytes
	void resizeBuffer(size_t newSize);

	// Append another BGCPString
	bool operator+=(const BGCPString& other);

	BGCPString operator+(const BGCPString& other) const
	{
		BGCPString result(*this);
		result += other;
		return result;
	}

	// Convert to codepage
	bool convertToCP(WORD cp);

	// Convert string to a double
	double toDouble() const;

	// Get codepage currently used by the internal representation
	WORD getCP() const;

	// Get current internal representation of the string
	template <typename C>
	const C* get() const
	{
		return (C*) m_chars;
	}

	// Like above function, but the returned pointer is non-const, allowing to change its contents.
	// Use with caution.
	template <typename C>
	C* get()
	{
		return (C*)(m_chars);
	}

	// Get pointer to a certain character
	// Ignores codepage information, C defines the character set
	// offset: byte index of character (not necessarily the number of the character!)
	template <typename C>
	C get(size_t offset) const
	{
		return (C) (m_chars[offset]);
	}

	// Get pointer to a char (non-const, allowing manipulation)
	template <typename C>
	C* getPtr(size_t offset = 0)
	{
		return (C*)(&m_chars[offset]);
	}

	// Get pointer to a character (const)
	template <typename C>
	const C* getPtr(size_t offset = 0) const
	{
		return (C*)(&m_chars[offset]);
	}

	// Get number of bytes used by current internal string representation
	// The returned value does not include the terminating zero
	size_t getByteCount() const;

	// Get number of characters in current string
	size_t getCharCount() const;

	// Cut off string at specified number of characters
	void setCharCount(size_t newCount);

	// Load string from file
	bool loadFromFile(const PathChar* fn, WORD codepage = CP_UTF8);

	// Print this string to the console
	void print() const;

	// Print this string to the console and add a line feed
	void printLn() const;

	std::vector<BGCPString> split(const BGCPString& mark) const
	{
		SET_SCOPE;
		size_t pos = 0;
		size_t lastHit = 0;

		BGCPString temp2 = mark;
		temp2.convertToCP(m_codepage);
		size_t markBytes = temp2.getByteCount();

		std::vector<BGCPString> result;
		while (pos < m_byteCount) {
			BGCPString temp;
			temp.assign(m_chars + pos, markBytes, m_codepage);
			if (mark == temp) {
				if (pos > lastHit) {
					BGCPString temp3;
					temp3.assign(m_chars + lastHit, pos - lastHit, m_codepage);
					result.push_back(temp3);
				}
				pos += markBytes;
				lastHit = pos;
			} else {
				pos += getCharSize(pos);
			}
		}

		if (pos > lastHit) {
			BGCPString temp3;
			temp3.assign(m_chars + lastHit, pos - lastHit, m_codepage);
			result.push_back(temp3);
		}

		RETURN result;
	}

	std::vector<BGCPString> splitByLines() const
	{
		SET_SCOPE;
		size_t pos = 0;
		size_t lastHit = 0;

		BGCPString retChar1 = "\r";
		retChar1.convertToCP(m_codepage);
		size_t retChar1Len = retChar1.m_byteCount;
		BGCPString retChar2 = "\n";
		retChar2.convertToCP(m_codepage);
		//size_t retChar2Len = retChar2.m_byteCount;

		std::vector<BGCPString> result;
		while (pos < m_byteCount) {
			BGCPString temp;
			temp.assign(m_chars + pos, retChar1Len, m_codepage);
			if (temp == retChar1 || temp == retChar2) {
				if (pos > lastHit) {
					BGCPString temp3;
					temp3.assign(m_chars + lastHit, pos - lastHit, m_codepage);
					result.push_back(temp3);
				}
				pos += retChar1Len;
				lastHit = pos;
			}
			else {
				pos += getCharSize(pos);
			}
		}

		SET_OP;
		if (pos > lastHit) {
			BGCPString temp3;
			temp3.assign(m_chars + lastHit, pos - lastHit, m_codepage);
			result.push_back(temp3);
		}

		RETURN result;
	}

	// Only treats preceeding whitespace so far ...
	BGCPString removeWhiteSpace() const
	{
		SET_SCOPE;
		size_t p1 = 0;
		BGCPString spaceStr(" ");
		spaceStr.convertToCP(m_codepage);
		BGCPString tabStr("	");
		tabStr.convertToCP(m_codepage);
		bool keepLooking = true;

		while (keepLooking) {
			keepLooking = false;
			if (bgstrcmp(m_chars + p1, spaceStr.m_chars, spaceStr.m_byteCount) == 0) {
				p1 += spaceStr.m_byteCount;
				keepLooking = true;
			} else {
				if (bgstrcmp(m_chars + p1, tabStr.m_chars, tabStr.m_byteCount) == 0) {
					p1 += tabStr.m_byteCount;
					keepLooking = true;
				}
			}

			if (p1 >= m_byteCount) {
				keepLooking = false;
			}
		}

		BGCPString result;
		result.assign(m_chars + p1, m_byteCount - p1, m_codepage);
		RETURN result;
	}

	// Return the integer represented by the string
	INT64 toInt() const;

	// Append windows error message to string
	#ifdef _WIN32
	static BGCPString getOSError(DWORD winErrCode = GetLastError());
	#else
	static BGCPString getOSError(int code = errno);
	#endif

	// Create size string, e.g. 1024MB for 1024*1024
	static BGCPString sizeStr (int64_t size);

	// Get current date and time as a string
	static BGCPString now();

	// Find character c in current string
	bool strscan(char c, size_t start, size_t stop, size_t& pos) const
	{
		if (m_codepage != CP_UTF16_LE && m_codepage != CP_UTF16_BE) {
			return bgstrscan<char>(m_chars, c, start, stop, pos);
		} else {
			return false;
		}
	}

	// Find character c in current string
	// Assumes that c uses the same endianness as m_chars ...
	bool strscan(char16_t c, size_t start, size_t stop, size_t& pos) const
	{
		if (m_codepage == CP_UTF16_LE || m_codepage == CP_UTF16_BE) {
			return bgstrscan<char16_t>((char16_t*)m_chars, c, start, stop, pos);
		} else {
			return false; 
		}
	}
	
	// Scan string to find the last occurrence of c
	// todo: assumes that C matches m_codepage ...
	// not suitable for composite characters ...
	template <typename C>
	bool findLast(C c, size_t start, size_t stop, size_t& pos) const
	{
		SET_SCOPE;
		bool found = false;
		size_t i = start; 

		while (i < stop) {
			C d = get<C>(i);
			if (c == d) {
				pos = i;
				found = true;
			}
			i += ::getCharSize(m_chars+i, m_codepage, m_byteCount-i);
		}

		RETURN found;
	}

	// Naive upcase implementation (ASCII)
	void upcase()
	{
		SET_SCOPE;
		convertToCP(CP_UTF16_LE);
		size_t i = 0;
		char16_t* c = (char16_t*)m_chars;
		while (c[i] != 0) {
			if (c[i] >= 'a' && c[i] <= 'z') {
				c[i] -= 'a' - 'A';
			}
			++i;
		}
		RETURN;
	}

	void addPathSep()
	{
		add<PathChar>(&OSPathSep, CP_SYSTEM);
	}
	void addPathSep(const BGCPString& str)
	{
		add<PathChar>(&OSPathSep, CP_SYSTEM);
		operator+=(str);
	}

	const PathChar* getAsPath()
	{
		SET_SCOPE;
		convertToCP(CP_SYSTEM);
		RETURN (const PathChar*)(m_chars);
	}

	bool equalsNoCase(const BGCPString& other) const
	{
		SET_SCOPE;
		BGCPString a = *this;
		BGCPString b = other;
		a.upcase();
		b.upcase();
		RETURN a == b;
	}

	// Check if this string ends with another string
	bool endsWith(const BGCPString& other) const;

	// Remove other string from the end of this string
	bool reduceBy(const BGCPString& other);

	bool saveToFile(const PathChar* fn) const;

	// Create a string that represents a double
	static BGCPString fromDouble(double d, int precision = 3);

	static const BGCPString EOL;
	static const BGCPString OSPathSepStr;

	static BGCPString time(int64_t ms);

	BGCPString getPart(size_t start, size_t stop) const
	{
		return BGCPString(m_chars+start, stop-start, m_codepage);
	}

	// Check if the character at index i equals c in whatever codepage the current string uses
	// c is expected to be an ASCII character
	bool charAtEquals(size_t i, char c) const;

	// Get number of bytes used by the character at offset i
	size_t getCharSize(size_t i) const;

	// Get directory part of a file name
	BGCPString getDirectory() const;

	// After writing directly to the internal buffer via a pointer obtained e.g. by getPtr(),
	// use this function to update the internal data
	void updateData(WORD newCP, size_t newBC);

private:
	static const size_t internalBufSize = 32;
	char* m_chars = nullptr;
	char m_internalBuf[internalBufSize];
	size_t m_byteCount = 0, m_reserved = 0, m_terminatorSize = 1;
	WORD m_codepage = CP_ASCII;
};

// Get the part of the path before the last slash
// (if filename defines a file, that will be its directory)
BGCPString getDirectory(const BGCPString& filename);

// Get directory for temporary files
BGCPString getTempDir();

////////////////////////////////////////////////////////////////////////////////
// Logging

void log(const BGCPString& str);
void logNoLF(const BGCPString& str);

template <typename C>
void log(const C* str)
{ log(BGCPString(str)); }

template <typename T1, typename T2>
void log (const T1& v1, const T2& v2)
{ log(BGCPString(v1) + BGCPString(v2)); }

template<typename T1, typename T2, typename... Args>
void log(const T1& v1, const T2& v2, Args... args) {
	log(BGCPString(v1) + BGCPString(v2), args...);
}

void setLogListener(MessageListener* newListener);

#undef FILE_NUMBER

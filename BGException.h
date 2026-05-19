////////////////////////////////////////////////////////////////////////////////
// BGException.h
// std::exception does not have a constructor that takes text (Linux-gcc)...

#pragma once

#include "BGCPString.h"

#define FILE_NUMBER FILE_NO_BGEXCEPTION_H

// unrelated to std::exception because std::exception::what() returns char*, 
// which is unsuitable for error messages that might contain Windows filenames
// Do not use this to report out of memory errors, because this class may 
// attempt to allocate a buffer for its strings ...

class BGException {
public:
	BGException(StackTrace* st, const BGCPString& msg, void* p = nullptr);

	const BGCPString& what() const NOEXCEPT;

	void* getAddress() const;

private:
	BGCPString mMsg;
	void* mExceptionAddress;
};

#define CONDITIONAL_THROW_WITH_BACKTRACE(condition, msg) \
if (condition) { \
	throw BGException(getStackTrace(), msg); \
}

////////////////////////////////////////////////////////////////////////////////
// Cast data from SourceType to TargetType and throw an exception if data is outside C's limits

template <typename SourceType, typename TargetType>
TargetType limitCheckCast(SourceType data)
{
	SET_SCOPE;

	int conversionSuccess = checkConversion<SourceType, TargetType>(data);
	if (conversionSuccess != 0) {
		BGCPString msg = "Conversion error ";
		msg += BGCPString::fromInt(conversionSuccess);
		throw BGException(getStackTrace(), msg);
	}

	RETURN (TargetType)(data);
}

#undef FILE_NUMBER

////////////////////////////////////////////////////////////////////////////////
// BGException.cpp
// We need exceptions that can report Unicode strings in a portable manner!

#include "BGBase.h"
#include "BGException.h"

////////////////////////////////////////////////////////////////////////////////

#ifdef USE_STACK_INFO
BGException::BGException(StackTrace* st, const BGCPString& msg, void* p)
: mMsg(msg),
  mExceptionAddress(p)
{
	if (st != nullptr) {
		mMsg += ". StackTrace: ";
		mMsg += st->toString();
	}
}
#else
BGException::BGException(StackTrace* /*st*/, const BGCPString& msg, void* p)
: mMsg(msg),
  mExceptionAddress(p)
{
}
#endif

const BGCPString& BGException::what() const NOEXCEPT
{
	return mMsg;
}

void* BGException::getAddress() const
{
	return mExceptionAddress;
}

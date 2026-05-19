////////////////////////////////////////////////////////////////////////////////
// BGBase.cpp
// Virtual class/memory tracer declaration and misc functions
// Copyright Bjoern Ganster 
// Developed since in 2005
////////////////////////////////////////////////////////////////////////////////

#include "BGBase.h"
#include "BGStringTemplates.h"
#include "BGCPString.h"

#include <algorithm>
#include <map>

#ifdef _WIN32
const PathChar* OSPathSepStr = L"\\";
#else
#include "sys/time.h"
#include <sys/stat.h>
const PathChar* OSPathSepStr = "/";
#endif

#define FILE_NUMBER FILE_NO_BGBASE_CPP

////////////////////////////////////////////////////////////////////////////////
// Constructor

VirtualClass::VirtualClass(const char* className, int classID)
: mClassName(className),
  mClassID(classID)
{
	SET_SCOPE;
	#ifdef SimpleMemoryCounters
	MemoryTracker* mt = getMemoryTracker();
	if (mt != nullptr) {
		mt->registerObject(this, className, classID, sizeof(this));
	}
	#endif
	RETURN;
}

// Copy Constructor
VirtualClass::VirtualClass (const VirtualClass& other)
: mClassName(other.mClassName),
  mClassID(other.mClassID)
{
	SET_SCOPE;
	#ifdef SimpleMemoryCounters
	MemoryTracker* mt = getMemoryTracker();
	if (mt != nullptr) {
		mt->registerObject(this, mClassName, mClassID, sizeof(this));
	}
	#endif
	RETURN;
}

// Destructor
VirtualClass::~VirtualClass() 
{
	SET_SCOPE;
	#ifdef SimpleMemoryCounters
	MemoryTracker* mt = getMemoryTracker();
	if (mt != nullptr) {
		mt->deregisterObject(this, mClassID, sizeof(this));
	}
	#endif
	RETURN;
}

// Send messages to other objects
bool VirtualClass::sendMessage (MessageListener* receiver, MessageType messageType, VirtualClass* data)
{ 
	SET_SCOPE;
	if (receiver != NULL) {
		receiver->receiveMessage (this, messageType, data);
		RETURN true;
	} else {
		RETURN false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Memory profiling support

// Allocate memory with trace support
#ifdef SimpleMemoryCounters
void* traceAlloc(size_t bytes, const char* className, int classID)
{
	SET_SCOPE;
	void* result = malloc (bytes);
	MemoryTracker* mt = getMemoryTracker();
	if (mt != nullptr) {
		mt->registerObject(result, className, classID, bytes);
	}
	RETURN result;
}
#else
void* traceAlloc (size_t bytes, const char*, int)
{
	SET_SCOPE;
	void* result = malloc (bytes);
	RETURN result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Free memory with trace support

#ifdef SimpleMemoryCounters
void traceFree(void* p, int classID, size_t usedMem)
{
	SET_SCOPE;
	MemoryTracker* mt = getMemoryTracker();
	if (mt != nullptr) {
		mt->deregisterObject(p, classID, usedMem);
	}
	free (p);
	RETURN;
}
#else
void traceFree (void* p, int, size_t)
{
	SET_SCOPE;
	free (p);
	RETURN;
}
#endif

// Dump memory counters for all classes
#ifdef SimpleMemoryCounters
void dumpMemoryUsage()
{
	SET_SCOPE;
	MemoryTracker* mt = getMemoryTracker();
	if (mt != nullptr) {
		mt->dump();
	}
	RETURN;
}
#else
void dumpMemoryUsage()
{
	log("Memory counters are inactive");
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Load data from file

#ifdef _WIN32

// Load data from file (Windows, ANSI file name)
// Free the returned buffer using traceFree(buf, UndefinedClassID, size);
void* loadFromFile(const char* FN, size_t& size)
{
	// Open file
	SET_SCOPE;
	Win32HandleCloser hc = CreateFileA(FN, GENERIC_READ, FILE_SHARE_READ, NULL, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (!hc.valid()) {
		log("Failed to open ", FN, "for reading: ", BGCPString::getOSError());
		RETURN nullptr;
	}
	
	// Determine file size
	SET_OP;
	LARGE_INTEGER fileSize;
	GetFileSizeEx(hc.get(), &fileSize);
	
	// Allocate buffer, if possible
	size = (size_t) fileSize.QuadPart;
	if ((LONGLONG) size < fileSize.QuadPart) {
		RETURN nullptr;
	}
	
	unsigned char* buf = (unsigned char*) traceAlloc(size, "LoadFromFileBuf", LoadFromFileBufID);
	
	// Read file - only ReadWriteBufMaxSize bytes per call due to documented 
	// limitations in ReadFile
	INT64 readTotal = 0;
	DWORD read = 0;
	do {
		SET_OP;
		DWORD toRead = (DWORD)std::min<INT64> (size-readTotal, ReadWriteBufMaxSize);
		if (!ReadFile(hc.get(), buf+readTotal, toRead, &read, NULL)) {
			log("Read failure on", FN, ": ", BGCPString::getOSError());
		}
		readTotal += read;
	} while (readTotal < fileSize.QuadPart && read > 0);

	if (readTotal == size) {
		RETURN buf;
	} else {
		size = 0;
		delete buf;
		RETURN nullptr;
	}
}

// Load data from file (Windows, Unicode file name)
// Free the returned buffer using traceFree(buf, UndefinedClassID, size);
void* loadFromFile(const wchar_t* FN, size_t& size, const char* allocatorName, int allocatorID)
{
	// Open file
	SET_SCOPE;
	Win32HandleCloser hc = CreateFileW(FN, GENERIC_READ, FILE_SHARE_READ, NULL, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (!hc.valid()) {
		log("Failed to open ", BGCPString(FN, CP_SYSTEM), " for reading: ", BGCPString::getOSError());
		RETURN nullptr;
	}

	// Determine file size
	SET_OP;
	LARGE_INTEGER fileSize;
	GetFileSizeEx(hc.get(), &fileSize);

	// Allocate buffer, if possible
	size = (size_t) fileSize.QuadPart;
	if ((LONGLONG) size < fileSize.QuadPart) {
		RETURN NULL;
	}

	unsigned char* buf = (unsigned char*) traceAlloc(size, allocatorName, allocatorID);

	// Read file - only ReadWriteBufMaxSize bytes per call due to documented 
	// limitations in ReadFile
	INT64 readTotal = 0;
	DWORD read = 0;
	do {
		SET_OP;
		DWORD toRead = (DWORD)std::min<INT64> (size-readTotal, ReadWriteBufMaxSize);
		if (!ReadFile(hc.get(), buf+readTotal, toRead, &read, NULL)) {
			log("Read failure on ", FN, ": ", BGCPString::getOSError());
		}
		readTotal += read;
	} while (readTotal < fileSize.QuadPart && read > 0);

	if (readTotal == size) {
		RETURN buf;
	} else {
		size = 0;
		delete buf;
		RETURN NULL;
	}
}

#else

// Load data from file (Linux)
// size returns the actual amount that was read
void* loadFromFile(const char* FileName, size_t& size)
{
	SET_SCOPE;
	FILE* file = fopen(FileName, "rb");

	if (file == NULL) {
		size = 0;
		log("Failed to open ", FileName, ", ", BGCPString::getOSError(errno));
		RETURN NULL;
	}

	// Determine file size
	SET_OP;
	fseek (file, 0, SEEK_END);
	size = ftell (file);
	fseek (file, 0, SEEK_SET);

	// Read file
	unsigned char* buf = (unsigned char*) traceAlloc(size, "LoadFromFileBuf", LoadFromFileBufID);
	if (buf != NULL) {
		// If we can't read everything, size should give the amount read
		SET_OP;
		size = fread(buf, 1, size, file);
	} else {
		log("Failed to allocate buffer of ", BGCPString::fromInt(size), " bytes to read ", FileName);
		size = 0;
	}

	fclose(file);
	RETURN buf;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Save data to file

#ifdef _WIN32

// Save data to file (Windows, Unicode file name)
bool saveToFile(const char* data, size_t size, const wchar_t* FN)
{
	SET_SCOPE;
	Win32HandleCloser hc = CreateFileW(FN, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hc.valid()) {
		log("Failed to open file ", FN, " for writing, ", BGCPString::getOSError());
		RETURN false;
	}
	DWORD written;
	size_t totalWritten = 0;
	size_t remaining = size;

	// WiteFile is documented to limit the max. amount of bytes written per call
	// so a loop is needed to read the data
	do {
		SET_OP;
		DWORD toWrite = (DWORD) std::min((size_t) ReadWriteBufMaxSize, remaining);
		WriteFile (hc.get(), data, toWrite, &written, NULL);
		data += written;
		remaining -= written;
		totalWritten += written;
	} while (written > 0 && remaining > 0);

	RETURN (totalWritten == size);
}

// Save data to file (Windows, ANSI file name)
bool saveToFile (const char* data, size_t size, const char* FN)
{
	SET_SCOPE;
	Win32HandleCloser hc = CreateFileA(FN, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hc.valid()) {
		log("Failed to open file ", FN, " for writing, ", BGCPString::getOSError());
		RETURN false;
	}
	DWORD written;
	size_t totalWritten = 0;
	size_t remaining = size;

	// WiteFile is documented to limit the max. amount of bytes written per call
	// so a loop is needed to read the data
	do {
		SET_OP;
		DWORD toWrite = (DWORD) std::min((size_t) ReadWriteBufMaxSize, remaining);
		WriteFile (hc.get(), data, toWrite, &written, NULL);
		data += written;
		remaining -= written;
		totalWritten += written;
	} while (written > 0 && remaining > 0);

	RETURN (totalWritten == size);
}

#else

// Save data to file (Linux)
bool saveToFile (const char* data, size_t size, const char* FN)
{
	SET_SCOPE;
	FILE* f = fopen(FN, "wb");
	if (f == NULL) {
		RETURN false;
	}
	SET_OP;
	size_t written = fwrite (data, 1, size, f);
	fclose (f);
	RETURN written == size;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// A simple integer storing the currently active operation - used for debugging
// crashes

#ifdef USE_STACK_INFO
// thread_local does not work with OpenMP?
StackTrace stackTrace;

StackInfo::StackInfo(const char* /*aFileName*/, int aFileNo, const char* /*aFunctionName*/, int aLineNo)
: cleanExit(false),
  fileNo (aFileNo),
  lineNo (aLineNo),
  next (NULL),
  previous(NULL)  
{
	stackTrace.addStackFrame(this);
	stackTrace.mLastFileNo = aFileNo;
	stackTrace.mLastLineNo = aLineNo;
}

StackInfo::~StackInfo()
{
	if (!cleanExit) {
		log("Unclean exit at ", BGCPString::fromInt(fileNo), ":", BGCPString::fromInt(lineNo));
		logStackTrace();
	}
	stackTrace.removeStackFrame();
}

void StackInfo::setOp(const char* /*aFileName*/, int aFileNo, const char* /*aFunctionName*/, int aLineNo)
{
	fileNo = aFileNo;
	lineNo = aLineNo;
	stackTrace.mLastFileNo = fileNo;
	stackTrace.mLastLineNo = lineNo;
}

int getLastFileNo()
{ return stackTrace.mLastFileNo; }

int getLastLineNo() 
{ return stackTrace.mLastLineNo; }

const StackTrace* StackTrace::get() 
{
	return &stackTrace;
}

void StackTrace::addStackFrame(StackInfo* info)
{
	if (info == nullptr) {
		return;
	}

	if (firstStackInfo == nullptr) {
		firstStackInfo = info;
		info->previous = nullptr;
	}

	if (lastStackInfo == nullptr) {
		lastStackInfo = info;
	} else {
		lastStackInfo->next = info;
		info->previous = lastStackInfo;
		lastStackInfo = info;
	}

	info->next = nullptr;
}

void StackTrace::removeStackFrame()
{
	if (firstStackInfo != nullptr && lastStackInfo != nullptr) {
		if (firstStackInfo == lastStackInfo) {
			firstStackInfo = nullptr;
			lastStackInfo = nullptr;
			mLastFileNo = 0;
			mLastLineNo = 0;
		} else {
			lastStackInfo = lastStackInfo->previous;
			lastStackInfo->next = nullptr;
			mLastFileNo = lastStackInfo->fileNo;
			mLastLineNo = lastStackInfo->lineNo;
		}
	}
}

BGCPString StackTrace::toString() const
{
	//SET_SCOPE; // doesn't belong here!!!
	BGCPString result;
	StackInfo * stackInfo = firstStackInfo;
	if (stackInfo != nullptr) {
		while (stackInfo != nullptr) {
			result += BGCPString::fromInt(stackInfo->getFileNo());
			result += ":";
			result += BGCPString::fromInt(stackInfo->getLineNo());
			result += ", ";
			stackInfo = stackInfo->next;
		}
	} else {
		result += " No stackinfo recorded.\n";
	}

	return result;
}
void StackTrace::toStringBuf(char* buf, size_t bufSize)
{
	StackInfo * stackInfo = firstStackInfo;
	if (stackInfo != nullptr) {
		buf[0] = 0;
		while (stackInfo != nullptr) {
			appendInt(buf, stackInfo->getFileNo(), 10, bufSize);
			bgstrcat(buf, ":", bufSize);
			appendInt(buf, stackInfo->getLineNo(), 10, bufSize);
			bgstrcat(buf, ", ", bufSize);
			stackInfo = stackInfo->next;
		}
	} else {
		bgstrcpy(buf, " No stackinfo recorded.\n", bufSize);
	}
}

StackTrace* getStackTrace()
{
	return &stackTrace;
}

#else

int gFileNo = 0, gLine = 0;
void setOp(int fileNo, int line)
{
	gFileNo = fileNo;
	gLine = line;
}

StackTrace* getStackTrace()
{
	return nullptr;
}

int getLastFileNo()
{
	return gFileNo;
}

int getLastLineNo() 
{
	return gLine;
}

BGCPString StackTrace::toString() const
{
	return BGCPString("No stack trace available");
}
void StackTrace::toStringBuf(char* buf, size_t bufSize)
{
	bgstrcpy(buf, "No stack trace available", bufSize);
}


#endif

void logStackTrace()
{
	auto* st = getStackTrace();
	if (st != nullptr) {
		log(st->toString());
	}
}

////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
#include "sys/time.h"
#endif

////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
int GetTickCount ()
{
   timeval t;
   gettimeofday (&t, NULL);
   int val = (t.tv_sec * 1000) + (t.tv_usec / 1000);
   return val;
}
#endif

////////////////////////////////////////////////////////////////////////////////

MessageListener* globalMessageListener = nullptr;

void setGlobalMessageListener(MessageListener* newListener)
{
	globalMessageListener = newListener;
}

DWORD gLastProcessMessagesCall = 0;

void bgProcessMessages()
{
	DWORD currTicks = GetTickCount();

	if (dist(currTicks, gLastProcessMessagesCall) > GUI_UPDATE_INTERVAL && globalMessageListener != nullptr) {
		globalMessageListener->receiveMessage(nullptr, MessageType::ProcessGuiMessages);
		gLastProcessMessagesCall = currTicks;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Simple class to make sure that handles are closed 

#ifdef _WIN32

Win32HandleCloser::Win32HandleCloser(HANDLE handle)
: mHandle(handle)
{
}

Win32HandleCloser::~Win32HandleCloser()
{
	close();
}

void Win32HandleCloser::close()
{
	SET_SCOPE;
	if (mHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(mHandle);
		mHandle = INVALID_HANDLE_VALUE;
	}
	RETURN;
}

HANDLE Win32HandleCloser::get() const
{
	return mHandle;
}

bool Win32HandleCloser::valid()
{
	return (mHandle != INVALID_HANDLE_VALUE);
}

void Win32HandleCloser::operator=(HANDLE newVal)
{
	if (valid())
		CloseHandle(mHandle);
	mHandle = newVal;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Run process. Blocks until process is finished and prints errors if necessary.

#ifdef _WIN32
bool runProcess(PathChar* cmdLineBuf, const PathChar* procDir)
{
	STARTUPINFO suInfo;
	memset(&suInfo, 0, sizeof(suInfo));
	suInfo.cb = sizeof(suInfo);
	PROCESS_INFORMATION pi;

	bool processCreated = CreateProcessW(NULL, cmdLineBuf,
		NULL, NULL, false, NORMAL_PRIORITY_CLASS, NULL,
		procDir, &suInfo, &pi);
	if (processCreated) {
		DWORD waitRes = WAIT_FAILED;
		do {
			waitRes = WaitForSingleObject(pi.hProcess, 100);
			bgProcessMessages();
		} while (waitRes == WAIT_TIMEOUT);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	} else {
		log("Failed to create process: ", BGCPString::getOSError());
		return false;
	}
}
#endif

bool deleteFile(const char* filename)
#ifdef _WIN32
{
	SET_SCOPE;
	bool result = DeleteFileA(filename);
	RETURN result;
}
#else
{
	SET_SCOPE;
	int result = remove(filename);
	RETURN (result == 0);
}
#endif

#ifdef _WIN32
bool deleteFile(const wchar_t* filename)
{
	SET_SCOPE;
	bool result = DeleteFileW(filename);
	RETURN result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Simple timer implementation
// Optimization may shift calls around, but only if these calls are known
// to be independent ... hopefully, this class obscures things enough to
// guarantee good time measurements!

class BGSimpleTimer: public BGTimer {
public:
	BGSimpleTimer()
	{
	}

	~BGSimpleTimer() OVERRIDE
	{
	}

	virtual void start() OVERRIDE
	{
		mStart = GetTickCount();
	}

	virtual void stop() OVERRIDE
	{
		mStop = GetTickCount();
	}

	virtual BGCPString getDurationString() OVERRIDE
	{
		DWORD ticks = mStop-mStart;
		return BGCPString::time(ticks);
	}
private:
	DWORD mStart = -1;
	DWORD mStop = -1;
};

BGTimer* getTimer()
{
	return new BGSimpleTimer;
}

////////////////////////////////////////////////////////////////////////////////
// MemoryTracker 

MemoryTracker* gMT = nullptr;

MemoryTracker* getMemoryTracker()
{
	return gMT;
}

void setMemoryTracker(MemoryTracker* mt)
{
	gMT = mt;
}

// Destructor has to be virtual because we intend to derive from this class
MemoryTracker::~MemoryTracker()
{
}

// Test various types of crashes
#ifdef _DEBUG
int crash(int method)
{
	switch(method) {
		case 0: // division by zero
		{
			int i = 1;
			return i / (i-1);
		}
		case 1: // nullptr unreference
		{
			char* p = nullptr;
			return p[0];
		}
		case 2: // infinite recusion
			return crash(method);
		case 3: // C++ exception
			throw std::exception();
			return 0;
		default: // double free
		{
			char* p = new char[10];
			char* q = p;
			delete p;
			delete q;
			return 0;
		}
	}
}
#endif

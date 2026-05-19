////////////////////////////////////////////////////////////////////////////////
// BGBase
// Virtual class/memory tracer declaration and misc functions
// Developed by Bjoern Ganster since 2005
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <limits>

#include <stdlib.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
// At least for Windows' WriteFile, restrictions apply
#define ReadWriteBufMaxSize 64 * 1024
// Fix Windows' misbehaviour regarding min/max ...
#undef max
#undef min
#endif

#define storeClassID
#ifdef _DEBUG
#define SimpleMemoryCounters
#endif

////////////////////////////////////////////////////////////////////////////////
// Codepages

#define CP_ASCII 437
#define CP_UTF16_LE 1200
#define CP_UTF16_BE 1201
#define CP_UTF32 12000

////////////////////////////////////////////////////////////////////////////////
// Use C++11 features if supported

#if __cplusplus >= 201103 || (defined(_MSC_VER) && _MSC_VER >= 1900)
#define NOEXCEPT noexcept
#define OVERRIDE override 
#define FINAL final
#define NO_EXCEPT noexcept
//#define THREAD_LOCAL thread_local // no TLS in delay-lodaded-DLLs
#else
#define NOEXCEPT
#define OVERRIDE
#define FINAL
#define NO_EXCEPT
//#define THREAD_LOCAL // no TLS in delay-lodaded-DLLs
#endif

////////////////////////////////////////////////////////////////////////////////
// Operating system defines

#ifdef __APPLE__
typedef unsigned int uint;
#endif

#ifdef _WIN32
	typedef UINT BGKey;
	typedef wchar_t PathChar;
	const PathChar OSPathSep = L'\\';
	extern const PathChar* OSPathSepStr;
	#define PathCharMod "%S"
	#define CP_SYSTEM CP_UTF16_LE
	#define COUT std::wcout
#else
	typedef char TCHAR;
	typedef uint BGKey;
	typedef char PathChar;
	typedef int64_t INT64;
	typedef u_int16_t WORD;
	typedef u_int32_t DWORD;
	const PathChar OSPathSep = '/';
	#define PathCharMod "%s"
	#define CP_UTF7 65000
	#define CP_UTF8 65001
	#define CP_SYSTEM CP_UTF8
	#define COUT std::cout
#endif

////////////////////////////////////////////////////////////////////////////////

enum class MessageType {
	ProcessGuiMessages,
	Log
};

////////////////////////////////////////////////////////////////////////////////
// File IDs

// It should be easy to look up numbers here, so every identifier should be defined by a number
enum FileID {
	FILE_NO_BGBASE_CPP = 0,
	FILE_NO_BGCPSTRING_H = 1,
	FILE_NO_BGCPSTRING_CPP = 2,
	FILE_NO_BGSTRINGTEMPLATES_H = 3,
	FILE_NO_BGABSTRACTSTORAGE_H = 4,
	FILE_NO_MEMORYBLOCK_H = 5,
	FILE_NO_MEMORYBLOCK_CPP = 6,
	FILE_NO_FILELIST_H = 7,
	FILE_NO_FILELIST_CPP = 8,
	FILE_NO_BACKPROPAGATION_H = 9,
	FILE_NO_BASICTRAINER_H = 10,
	FILE_NO_ANNOTATION_EDITOR_CPP = 11,
	FILE_NO_BG_MAX_NEURON_H = 12,
	FILE_NO_BG_NEURAL_NET_H = 13,
	FILE_NO_BG_NEURAL_NET_CPP = 14,
	FILE_NO_BG_NEURAL_NET_LAYER_H = 15,
	FILE_NO_BG_NEURON_H = 16,
	FILE_NO_BG_OPENCL_NEURAL_NET_H = 17,
	FILE_NO_BG_OPENCL_NEURAL_NET_CPP = 18,
	FILE_NO_BG_REFERENCE_NEURON_H = 19,
	FILE_NO_BG_RELU_NEURON_H = 20,
	FILE_NO_BG_COLOR_EXPERIMENTS_MAIN_WINDOW_H = 22,
	FILE_NO_BG_COLOR_EXPERIMENTS_MAIN_WINDOW_CPP = 23,
	FILE_NO_EVALUATE_BG_NEURONS_CPP = 24,
	FILE_NO_FIND_EYES_CPP = 25,
	FILE_NO_FIND_OBJECTS_H = 26,
	FILE_NO_IMAGE_STRUCTS_H = 27,
	FILE_NO_OPENCL_BUFFER_H = 28,
	FILE_NO_OPENCL_CONTEXT_H = 29,
	FILE_NO_OPENCL_CONTEXT_CPP = 30,
	FILE_NO_PHOTO_ANNOTATOR_CPP = 31,
	FILE_NO_PHOTO_ITEM_CPP = 32,
	FILE_NO_PIC_PROVIDER_H = 33,
	FILE_NO_PIC_PROVIDER_CPP = 34,
	FILE_NO_QT_BRIDGE_CPP = 35,
	FILE_NO_TEXTURE_H = 36,
	FILE_NO_TEXTURE_CPP = 37,
	FILE_NO_TRAIN_CLASSIFIER_DIALOG_H = 38,
	FILE_NO_TRAIN_CLASSIFIER_DIALOG_CPP = 39,
	FILE_NO_XML_TREE_H = 40,
	FILE_NO_IMAGE_STRUCTS_CPP = 41, 
	FILE_NO_PATH_AND_BUTTON_CONNECTOR_CPP = 42,
	FILE_NO_BGTEXTFILE_CPP = 43,
	FILE_NO_PROGRESSDIALOG_H = 44,
	FILE_NO_OBJECTFINDER_H = 45,
	FILE_NO_OBJECTFINDER_CPP = 46,
	FILE_NO_PROCESS_VIDEO_H = 47,
	FILE_NO_PROCESS_VIDEO_CPP = 48,
	FILE_NO_BGEXCEPTION_H = 49,
	FILE_NO_TEST_BGCPSTRING_CPP = 50,
	FILE_NO_RUN_TENSORFLOW_CPP = 51,
	FILE_NO_EVAL_PROCESS_CPP = 52,
	FILE_NO_TEST_TRAINERS_CPP = 53,
	FILE_NO_NN_TRAINING_DATA_CACHE_H = 54,
	FILE_NO_NN_TRAINING_DATA_PROVIDER_H = 55,
	FILE_NO_BG_FILE_READER_H = 56,
	FILE_NO_BGABSTRACTSTORAGE_CPP = 57,
	FILE_NO_BG_ALGORITHM_H = 58,
	FILE_NO_TEST_BG_ALGORITHM_H = 59,
	FILE_NO_AUTO_SCALED_BACKPROPAGATION_H = 60,
	FILE_NO_XML_TREE_CPP = 61,
	FILE_NO_PYTHON_CPP = 62,
	FILE_NO_MAIN_CPP = 2021013
};

enum ClassIDs {
	BGCPStringID = 1,
	BGCPStringBufID = 2,
	BGTextFileID = 3,
	BGTextFileWriterID = 4,
	MemoryBlockID = 5,
	MemoryBlockBufID = 6,
	BGFileReaderID = 7,
	XMLAttributeID = 8,
	XMLTreeNodeID = 9,
	XMLTreeConstIteratorID = 10,
	XMLTreeIteratorID = 11,
	XMLTreeID = 12,
	LoadFromFileBufID = 13,
	FileListID = 14,
	ColorExperimentsMainWindowID = 15,
	BGNeuralNetID = 16,
	AutoScaledBackpropagationID = 17,
	TextureID = 18,
	TextureTexelsID = 19,
	TextureSaveBufferID = 20,
	GlobalProcessMessageListenerID = 21,
	BasicNNTrainerID = 22,
	ParallelBasicNNTrainerID = 23,
	BackpropagationID = 24,
	XMLTreeBufID = 25,
	NNTrainingDataProviderID = 26,
	PicProviderID = 27,
	NNTrainingDataCacheID = 28,
	OOCDataProviderID = 29
};

////////////////////////////////////////////////////////////////////////////////
// Interface definition for memory tracking

class MemoryTracker {
public:
	// Destructor has to be virtual because we intend to derive from this class
	virtual ~MemoryTracker();

	// Register a new object
	virtual void registerObject(void* p, const char* className, int classID, size_t instanceSize) = 0;

	// Deregister an object that is about to be deleted
	virtual void deregisterObject(void* p, int classID, size_t instanceSize) = 0;

	// Print usage information
	virtual void dump() = 0;
};

MemoryTracker* getMemoryTracker();
void setMemoryTracker(MemoryTracker* mt);

////////////////////////////////////////////////////////////////////////////////
// Virtual class: introduces memory profiling and message passing to all
// subclasses

class MessageListener;

class VirtualClass {
public:
	// Default constructor
	// Stores the class name without allocating memory, so only pass static strings here!
	VirtualClass(const char* className, int classID);

	// Copy constructor
	VirtualClass(const VirtualClass& other);

	// Destructors of classes with virtual functions must also be virtual
	virtual ~VirtualClass();

	// Get class metainformation for memory tracking
	const char* getClassName() const
	{ return mClassName; }
	virtual int getClassID() const
	{ return mClassID; }
protected:
	// It might seem tempting to define virtual functions instead of using member functions,
	// but we need access to these values during construction and destruction, where using 
	// virtual functions is unsafe
	const char* mClassName;
	int mClassID;

	// Send messages to other objects
	bool sendMessage(MessageListener* receiver, MessageType mt, VirtualClass* data = nullptr);
};

////////////////////////////////////////////////////////////////////////////////
// Class for message listeners

class MessageListener: public VirtualClass {
public:
	// Constructor
	MessageListener(const char* className, int classID)
	: VirtualClass(className, classID)
	{
	}

	// Destructor
	~MessageListener() OVERRIDE
	{
	}

	// Function to receive messages
	virtual void receiveMessage (VirtualClass* /*sender*/, MessageType /*MessageType*/, VirtualClass* /*data*/ = nullptr)
	{
	}
};

////////////////////////////////////////////////////////////////////////////////
// Memory allocation and deallocation with memory tracing

// Allocate memory with trace support, if enabled
void* traceAlloc (size_t bytes, const char* className, int classID);

// Free memory with trace support, if enabled
void traceFree (void* p, int classID, size_t usedMem);

// Safely delete a pointer
template<typename T>
void safeDelete(T*& p)
{
	if (p != nullptr) {
		delete p;
		p = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Memory profiling support

// Get amount of memory used by typeID
size_t getMemoryUsedByType(int typeID);

// Dump memory counters for all classes
void dumpMemoryUsage();

// Initialize table of memory sizes
void initTypeSizes();

////////////////////////////////////////////////////////////////////////////////
// File help functions

// Load data from file
void* loadFromFile (const char* FileName, size_t& size, const char* allocatorName, int allocatorID);

#ifdef _WIN32
void* loadFromFile (const wchar_t* FN, size_t& size, const char* allocatorName, int allocatorID);
#endif

// Save data to file
bool saveToFile (const char* data, size_t size, const char* FN);

#ifdef _WIN32
bool saveToFile (const char* data, size_t size, const wchar_t* FN);
#endif

////////////////////////////////////////////////////////////////////////////////
// Provide simple timer functionality for non-Windows

#ifndef _WIN32
int GetTickCount ();
#endif

////////////////////////////////////////////////////////////////////////////////
// Random functions

// Initialize randomizer - without this, the program produces the same 
// sequence of pseudorandom numbers every time the program is run
inline void randomize()
{ 
   time_t now;
   time(&now);
   srand ((unsigned int) now);
}

// Produce a floating point random number in given range
template <typename T>
T frand(T a = 0.0, T b = 1.0)
{
	// For Windows, RAND_MAX = 0x7fff. In some cases, more could be required. So we force a higher value here.
	const int frandMax = 1 << 23; // single precision float mantissa is 23 bits
	return a + (b-a) * ((T) irand (0, frandMax)) / ((T) (frandMax)); 
}

// Integer random, returns a value x satisfying a <= x < b
// Assumes that b-a < RAND_MAX, but under Windows: RAND_MAX = 7fff
// Which is not enough in many cases ...
template <typename T>
T irand(T a, T b)
{
	if (a == b) {
		return a;
	}
	assert (a <= b);
	T x = 0;
	T y = 0;
	int randMaxBits = 15; // 15 bits are guaranteed by the standard

	while (y < b-a) {
		T z = rand();
		x = (x << randMaxBits) ^ z;
		y = (y << randMaxBits) ^ RAND_MAX;
	}

	return a + (x % (b-a));
}

////////////////////////////////////////////////////////////////////////////////
// Put b into the range of a, c

template <class Type>
const Type& putInRange (const Type& min, const Type& val,const Type& max)
{
   if (val < min)
      return min;
   else if (val < max)
      return val;
   else
      return max;
}

////////////////////////////////////////////////////////////////////////////////
// Calculate minimum, maximum at the same time

template <class T>
void minmax (const T& a, const T& b, T& min, T& max)
{
   if (a < b) {
      T help = b; // Keep a copy in case of swap 
      min = a;
      max = help;
   } else {
      T help = a; // Keep a copy in case of swap 
      min = b;
      max = help;
   }
}

////////////////////////////////////////////////////////////////////////////////
// Message IDs - used by receiveMessage and related functions

const int ButtonClickedEvent = 0;
const int ToolbarButtonClickedEvent = 1;
const int UpdateWidgetsEvent = 2;
const int ValueChangedEvent = 3; // Sent by GLLineEdit, GLSlider
const int ContextMenuClickEvent = 4; // Sent by glutContextMenuEntry
const int TableItemSelected = 5; // Sent by GLTableView
const int ContextMenuRequested = 6;
const int CheckBoxChecked = 7;
const int CheckBoxUnchecked = 8;

////////////////////////////////////////////////////////////////////////////////
// Translation support

bool loadLangFile (const PathChar* FileName);
const TCHAR* tr (int strNum);

////////////////////////////////////////////////////////////////////////////////
// A simple integer storing the currently active operation - used for debugging
// crashes

class BGCPString;

class StackInfo FINAL {
public:
	bool cleanExit;

	StackInfo(const char* fileName, int fileNo, const char* functionName, int lineNo);
	~StackInfo();
	void setOp(const char* fileName, int fileNo, const char* functionName, int lineNo);

	int getFileNo() const
	{ return fileNo; }

	int getLineNo() const
	{ return lineNo; }

	StackInfo* getNext() const
	{ return next; }

private:
	int fileNo;
	int lineNo;
	StackInfo* next;
	StackInfo* previous;

	friend class StackTrace;
};

class StackTrace FINAL {
public:
	int mLastFileNo;
	int mLastLineNo;

	// Constructor
	StackTrace()
	: mLastFileNo(0),
	  mLastLineNo(0),
	  firstStackInfo(nullptr),
	  lastStackInfo(nullptr)
	{
	}

	// Destructor
	~StackTrace()
	{
	}

	void addStackFrame(StackInfo* info);

	void removeStackFrame();

	BGCPString toString() const;
	void toStringBuf(char* buf, size_t bufSize);

	static const StackTrace* get();

	const StackInfo* getFirstinfo() const
	{ return firstStackInfo; }

private:
	StackInfo* firstStackInfo;
	StackInfo* lastStackInfo;
};

StackTrace* getStackTrace();
void logStackTrace();

////////////////////////////////////////////////////////////////////////////////

//#define USE_STACK_INFO

#ifdef USE_STACK_INFO

#define SET_SCOPE StackInfo stackInfo (__FILE__, FILE_NUMBER, __FUNCTION__, __LINE__);

#define SET_OP stackInfo.setOp(__FILE__, FILE_NUMBER, __FUNCTION__, __LINE__);

#define RETURN stackInfo.cleanExit = true; return 

#else
// Replacement definitions that do less ...

void setOp(int fileNo, int line);

#define SET_SCOPE setOp(FILE_NUMBER, __LINE__)

#define SET_OP  setOp(FILE_NUMBER, __LINE__)

#define RETURN return

#endif

int getLastFileNo();
int getLastLineNo();

////////////////////////////////////////////////////////////////////////////////
// Simple class to make sure that handles are closed 

#ifdef _WIN32

class Win32HandleCloser FINAL {
public:
	Win32HandleCloser(HANDLE handle = INVALID_HANDLE_VALUE);

	~Win32HandleCloser();

	void close();

	HANDLE get() const;

	bool valid();

	void operator=(HANDLE newVal);
private:
	HANDLE mHandle;
};

#endif

////////////////////////////////////////////////////////////////////////////////

void setGlobalMessageListener(MessageListener* newListener);

#define GUI_UPDATE_INTERVAL 25
void bgProcessMessages();

// Check if datum can be converted safely from SourceType to TargetType
// The additional checks ensure that alerts are raised if something goes wrong,
// but this function should not be used inside performance-critical loops ...
// Then again, the compiler heavily optimizes this function by inlining it and
// removing all tests whose outcome is known at compile time
// When called with constants, the optimization may completely remove this function
// from executed code ...
template <typename SourceType, typename TargetType>
int checkConversion(SourceType datum) NOEXCEPT
{
	if (std::is_same<SourceType, TargetType>::value) {
		return 0;
	}

	if (std::numeric_limits<TargetType>::is_signed) {
		// TargetType is signed
		auto max = std::numeric_limits<TargetType>::max();
		if ((uint64_t)datum > (uint64_t) max && datum > 0) {
			return 2; // Value larger than expected
		}

		auto min = std::numeric_limits<TargetType>::min();
		if ((int64_t)datum < (int64_t) min && std::numeric_limits<SourceType>::is_signed)
		{
			return 1; // Value smaller than expected
		}
	} else {
		// Target is unsigned, so we can assume std::numeric_limits<TargetType>::min() == 0
		if (datum < 0) {
			return 3; // Unexpected negative value
		}

		auto max = std::numeric_limits<TargetType>::max();
		if ((uint64_t) datum > (uint64_t) max)
		{
			return 5; // Value larger than expected
		}
	}

	return 0;
}

template <typename SourceType, typename TargetType>
bool limitCheckAssign(const SourceType c, TargetType& d) NOEXCEPT
{
	if (checkConversion<SourceType, TargetType>(c) == 0) {
		d = (TargetType)c;
		return true;
	} else {
		return false;
	}
}

// Run process. Blocks until process is finished and prints errors if necessary.
bool runProcess(PathChar* cmdLineBuf, const PathChar* procDir);

// Compute distance between a and b
// Not using abs here because simply computing a-b may cause integer overflows, especially for unsigned types
template <typename T>
T dist(T& a, T& b)
{
	if (a < b) 
		return b-a;
	else
		return a-b;
}

bool deleteFile(const char* filename);
#ifdef _WIN32
bool deleteFile(const wchar_t* filename);
#endif

////////////////////////////////////////////////////////////////////////////////
// Simple timer implementation
// Optimization may shift calls around, but only if these calls are known
// to be independent ... hopefully, this class obscures things enough to
// guarantee good time measurements!

class BGTimer {
public:
	virtual ~BGTimer()
	{}
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual BGCPString getDurationString() = 0;
};

BGTimer* getTimer();

////////////////////////////////////////////////////////////////////////////////
// https://stackoverflow.com/questions/875103/how-do-i-erase-an-element-from-stdvector-by-index

template< typename TContainer >
static bool eraseFromUnorderedByIndex(TContainer& inContainer, size_t inIndex )
{
	if (inIndex < inContainer.size()) {
		if(inIndex != inContainer.size() - 1) {
			inContainer[inIndex] = inContainer.back();
		}
		inContainer.pop_back();
		return true;
	} else {
		return false;
	}
}

// Test various types of crashes
#ifdef _DEBUG
int crash(int method);
#endif
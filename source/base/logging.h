//
// PROJECT:         Aspia
// FILE:            base/logging.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__LOGGING_H
#define _ASPIA_BASE__LOGGING_H

#include <experimental/filesystem>
#include <sstream>
#include <type_traits>
#include <utility>

#include "base/macros.h"


// Instructions
// ------------
//
// Make a bunch of macros for logging.  The way to log things is to stream
// things to LOG(<a particular severity level>).  E.g.,
//
//   LOG(INFO) << "Found " << num_cookies << " cookies";
//
// You can also do conditional logging:
//
//   LOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// The CHECK(condition) macro is active in both debug and release builds and
// effectively performs a LOG(FATAL) which terminates the process and
// generates a crashdump unless a debugger is attached.
//
// There are also "debug mode" logging macros like the ones above:
//
//   DLOG(INFO) << "Found cookies";
//
//   DLOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// All "debug mode" logging is compiled away to nothing for non-debug mode
// compiles.  LOG_IF and development flags also work well together
// because the code can be compiled away sometimes.
//
// We also have
//
//   LOG_ASSERT(assertion);
//   DLOG_ASSERT(assertion);
//
// which is syntactic sugar for {,D}LOG_IF(FATAL, assert fails) << assertion;
//
// We also override the standard 'assert' to use 'DLOG_ASSERT'.
//
// Lastly, there is:
//
//   PLOG(ERROR) << "Couldn't do foo";
//   DPLOG(ERROR) << "Couldn't do foo";
//   PLOG_IF(ERROR, cond) << "Couldn't do foo";
//   DPLOG_IF(ERROR, cond) << "Couldn't do foo";
//   PCHECK(condition) << "Couldn't do foo";
//   DPCHECK(condition) << "Couldn't do foo";
//
// which append the last system error to the message in string form (taken from
// GetLastError() on Windows and errno on POSIX).
//
// The supported severity levels for macros that allow you to specify one
// are (in increasing order of severity) INFO, WARNING, ERROR, and FATAL.
//
// Very important: logging a message at the FATAL severity level causes
// the program to terminate (after the message is logged).
//
// There is the special severity of DFATAL, which logs FATAL in debug mode,
// ERROR in normal mode.

namespace aspia {

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() 0
#else
#define DCHECK_IS_ON() 1
#endif

// Where to record logging output? A flat file and/or system debug log via OutputDebugString.
enum LoggingDestination
{
    LOG_NONE                = 0,
    LOG_TO_FILE             = 1 << 0,
    LOG_TO_SYSTEM_DEBUG_LOG = 1 << 1,

    LOG_TO_ALL = LOG_TO_FILE | LOG_TO_SYSTEM_DEBUG_LOG,

    // On Windows, use a file next to the exe.
    LOG_DEFAULT = LOG_TO_FILE,
};

// Indicates that the log file should be locked when being written to.
// Unless there is only one single-threaded process that is logging to
// the log file, the file should be locked during writes to make each
// log output atomic. Other writers will block.
//
// All processes writing to the log file must have their locking set for it to
// work properly. Defaults to LOCK_LOG_FILE.
enum LogLockingState { LOCK_LOG_FILE, DONT_LOCK_LOG_FILE };

enum LoggingSeverity : int
{
    LS_INFO    = 0,
    LS_WARNING = 1,
    LS_ERROR   = 2,
    LS_FATAL   = 3,
    LS_NUMBER  = 4,
    LS_DFATAL  = LS_FATAL,
    LS_DCHECK  = LS_FATAL
};

struct LoggingSettings
{
    // The defaults values are:
    //
    //  logging_dest: LOG_DEFAULT
    //  log_file:     NULL
    //  lock_log:     LOCK_LOG_FILE
    //  delete_old:   APPEND_TO_OLD_LOG_FILE
    LoggingSettings();

    LoggingDestination logging_dest;

    // The three settings below have an effect only when LOG_TO_FILE is
    // set in |logging_dest|.
    LogLockingState lock_log;
};

// Define different names for the BaseInitLoggingImpl() function depending on
// whether NDEBUG is defined or not so that we'll fail to link if someone tries
// to compile logging.cc with NDEBUG but includes logging.h without defining it,
// or vice versa.
#if defined(NDEBUG)
#define BaseInitLoggingImpl BaseInitLoggingImpl_built_with_NDEBUG
#else
#define BaseInitLoggingImpl BaseInitLoggingImpl_built_without_NDEBUG
#endif

// Implementation of the InitLogging() method declared below.  We use a
// more-specific name so we can #define it above without affecting other code
// that has named stuff "InitLogging".
bool BaseInitLoggingImpl(const LoggingSettings& settings);

// Sets the log file name and other global logging state. Calling this function
// is recommended, and is normally done at the beginning of application init.
// If you don't call it, all the flags will be initialized to their default
// values, and there is a race condition that may leak a critical section
// object if two threads try to do the first log at the same time.
// See the definition of the enums above for descriptions and default values.
//
// The default log file is initialized to "debug.log" in the application
// directory. You probably don't want this, especially since the program
// directory may not be writable on an enduser's system.
//
// This function may be called a second time to re-direct logging (e.g after
// loging in to a user partition), however it should never be called more than
// twice.
inline bool InitLogging(const LoggingSettings& settings)
{
    return BaseInitLoggingImpl(settings);
}

// Sets the log level. Anything at or above this level will be written to the
// log file/displayed to the user (if applicable). Anything below this level
// will be silently ignored. The log level defaults to 0 (everything is logged
// up to level INFO) if this function is not called.
// Note that log messages for VLOG(x) are logged at level -x, so setting
// the min log level to negative values enables verbose logging.
void SetMinLogLevel(LoggingSeverity level);

// Gets the current log level.
LoggingSeverity GetMinLogLevel();

// Used by LOG_IS_ON to lazy-evaluate stream arguments.
bool ShouldCreateLogMessage(LoggingSeverity severity);

// Sets the Log Message Handler that gets passed every log message before
// it's sent to other log destinations (if any).
// Returns true to signal that it handled the message and the message
// should not be sent to other log destinations.
typedef bool (*LogMessageHandlerFunction)
    (int severity, const char* file, int line, size_t message_start, const std::string& str);

void SetLogMessageHandler(LogMessageHandlerFunction handler);
LogMessageHandlerFunction GetLogMessageHandler();

// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define COMPACT_LOG_EX_LS_INFO(ClassName, ...) \
    aspia::ClassName(__FILE__, __LINE__, aspia::LS_INFO, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_WARNING(ClassName, ...) \
    aspia::ClassName(__FILE__, __LINE__, aspia::LS_WARNING, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_ERROR(ClassName, ...) \
    aspia::ClassName(__FILE__, __LINE__, aspia::LS_ERROR, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_FATAL(ClassName, ...) \
    aspia::ClassName(__FILE__, __LINE__, aspia::LS_FATAL, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_DFATAL(ClassName, ...) \
    aspia::ClassName(__FILE__, __LINE__, aspia::LS_DFATAL, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_DCHECK(ClassName, ...) \
    aspia::ClassName(__FILE__, __LINE__, aspia::LS_DCHECK, ##__VA_ARGS__)

#define COMPACT_LOG_LS_INFO    COMPACT_LOG_EX_LS_INFO(LogMessage)
#define COMPACT_LOG_LS_WARNING COMPACT_LOG_EX_LS_WARNING(LogMessage)
#define COMPACT_LOG_LS_ERROR   COMPACT_LOG_EX_LS_ERROR(LogMessage)
#define COMPACT_LOG_LS_FATAL   COMPACT_LOG_EX_LS_FATAL(LogMessage)
#define COMPACT_LOG_LS_DFATAL  COMPACT_LOG_EX_LS_DFATAL(LogMessage)
#define COMPACT_LOG_LS_DCHECK  COMPACT_LOG_EX_LS_DCHECK(LogMessage)

// As special cases, we can assume that LOG_IS_ON(FATAL) always holds. Also,
// LOG_IS_ON(DFATAL) always holds in debug mode. In particular, CHECK()s will
// always fire if they fail.
#define LOG_IS_ON(severity) \
  (aspia::ShouldCreateLogMessage(aspia::##severity))

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold. Condition is evaluated once and only once.
#define LAZY_STREAM(stream, condition) \
  !(condition) ? (void) 0 : aspia::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token COMPACT_LOG_LS_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG_STREAM(severity) COMPACT_LOG_ ## severity.stream()

#define LOG(severity) LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition) \
  LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

#define LOG_ASSERT(condition) \
  LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition ". "

#define PLOG_STREAM(severity) \
  COMPACT_LOG_EX_ ## severity(Win32ErrorLogMessage, \
      aspia::GetLastSystemErrorCode()).stream()

#define PLOG(severity) \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity))

#define PLOG_IF(severity, condition) \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

extern std::ostream* g_swallow_stream;

// Note that g_swallow_stream is used instead of an arbitrary LOG() stream to
// avoid the creation of an object with a non-trivial destructor (LogMessage).
// On MSVC x86 (checked on 2015 Update 3), this causes a few additional
// pointless instructions to be emitted even at full optimization level, even
// though the : arm of the ternary operator is clearly never executed. Using a
// simpler object to be &'d with Voidify() avoids these extra instructions.
// Using a simpler POD object with a templated operator<< also works to avoid
// these instructions. However, this causes warnings on statically defined
// implementations of operator<<(std::ostream, ...) in some .cc files, because
// they become defined-but-unreferenced functions. A reinterpret_cast of 0 to an
// ostream* also is not suitable, because some compilers warn of undefined
// behavior.
#define EAT_STREAM_PARAMETERS \
  true ? (void)0 : aspia::LogMessageVoidify() & (*aspia::g_swallow_stream)

// Captures the result of a CHECK_EQ (for example) and facilitates testing as a
// boolean.
class CheckOpResult
{
public:
    // |message| must be non-null if and only if the check failed.
    CheckOpResult(std::string* message) : message_(message)
    {
        // Nothing
    }

    // Returns true if the check succeeded.
    operator bool() const { return !message_; }
    // Returns the message.
    std::string* message() { return message_; }

private:
    std::string* message_;
};

#define IMMEDIATE_CRASH() __debugbreak()

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.
//
// We make sure CHECK et al. always evaluates their arguments, as
// doing CHECK(FunctionWithSideEffect()) is a common idiom.

// Do as much work as possible out of line to reduce inline code size.
#define CHECK(condition)                                                    \
    LAZY_STREAM(aspia::LogMessage(__FILE__, __LINE__, #condition).stream(), !(condition))

#define PCHECK(condition)                                                   \
    LAZY_STREAM(PLOG_STREAM(FATAL), !condition) << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   CHECK_EQ(2, a);
#define CHECK_OP(name, op, val1, val2)                                         \
  switch (0) case 0: default:                                                  \
  if (aspia::CheckOpResult true_if_passed =                                    \
      aspia::Check##name##Impl((val1), (val2), #val1 " " #op " " #val2));      \
  else                                                                         \
      aspia::LogMessage(__FILE__, __LINE__, true_if_passed.message()).stream()

template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};

template <typename T>
struct SupportsOstreamOperator<T, decltype(
    void(std::declval<std::ostream&>() << std::declval<T>()))> : std::true_type {};

// This formats a value for a failing CHECK_XX statement.  Ordinarily,
// it uses the definition for operator<<, with a few special cases below.
template <typename T>
inline typename std::enable_if<
    SupportsOstreamOperator<const T&>::value &&
        !std::is_function<typename std::remove_pointer<T>::type>::value, void>::type
MakeCheckOpValueString(std::ostream* os, const T& v)
{
    (*os) << v;
}

// Provide an overload for functions and function pointers. Function pointers
// don't implicitly convert to void* but do implicitly convert to bool, so
// without this function pointers are always printed as 1 or 0. (MSVC isn't
// standards-conforming here and converts function pointers to regular
// pointers, so this is a no-op for MSVC.)
template <typename T>
inline typename std::enable_if<
    std::is_function<typename std::remove_pointer<T>::type>::value, void>::type
MakeCheckOpValueString(std::ostream* os, const T& v)
{
    (*os) << reinterpret_cast<const void*>(v);
}

// We need overloads for enums that don't support operator<<.
// (i.e. scoped enums where no operator<< overload was declared).
template <typename T>
inline typename std::enable_if<
    !SupportsOstreamOperator<const T&>::value && std::is_enum<T>::value, void>::type
MakeCheckOpValueString(std::ostream* os, const T& v)
{
    (*os) << static_cast<typename std::underlying_type<T>::type>(v);
}

// We need an explicit overload for std::nullptr_t.
void MakeCheckOpValueString(std::ostream* os, std::nullptr_t p);

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline.  Caller
// takes ownership of the returned string.
template<class t1, class t2>
std::string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names)
{
    std::ostringstream ss;
    ss << names << " (";
    MakeCheckOpValueString(&ss, v1);
    ss << " vs. ";
    MakeCheckOpValueString(&ss, v2);
    ss << ")";
    std::string* msg = new std::string(ss.str());
    return msg;
}

// Commonly used instantiations of MakeCheckOpString<>. Explicitly instantiated in logging.cc.
std::string* MakeCheckOpString(const int& v1, const int& v2, const char* names);
std::string* MakeCheckOpString(const unsigned long& v1, const unsigned long& v2, const char* names);
std::string* MakeCheckOpString(const unsigned int& v1, const unsigned int& v2, const char* names);
std::string* MakeCheckOpString(const unsigned long long& v1, const unsigned long long& v2, const char* names);
std::string* MakeCheckOpString(const unsigned long& v1, const unsigned int& v2, const char* names);
std::string* MakeCheckOpString(const unsigned int& v1, const unsigned long& v2, const char* names);
std::string* MakeCheckOpString(const std::string& v1, const std::string& v2, const char* names);

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define DEFINE_CHECK_OP_IMPL(name, op)                                                   \
    template <class t1, class t2>                                                        \
    inline std::string* Check##name##Impl(const t1& v1, const t2& v2, const char* names) \
    {                                                                                    \
        if ((v1 op v2))                                                                  \
            return nullptr;                                                              \
        else                                                                             \
            return aspia::MakeCheckOpString(v1, v2, names);                              \
    }                                                                                    \
    inline std::string* Check##name##Impl(int v1, int v2, const char* names)             \
    {                                                                                    \
        if ((v1 op v2))                                                                  \
            return nullptr;                                                              \
        else                                                                             \
            return aspia::MakeCheckOpString(v1, v2, names);                              \
    }

DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL

#define CHECK_EQ(val1, val2) CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(GT, > , val1, val2)

// Definitions for DLOG et al.

#if DCHECK_IS_ON()

#define DLOG_IS_ON(severity) LOG_IS_ON(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)
#define DPLOG_IF(severity, condition) PLOG_IF(severity, condition)

#else  // DCHECK_IS_ON()

// If !DCHECK_IS_ON(), we want to avoid emitting any references to |condition|
// (which may reference a variable defined only if DCHECK_IS_ON()).
// Contrast this with DCHECK et al., which has different behavior.

#define DLOG_IS_ON(severity) false
#define DLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DLOG_ASSERT(condition) EAT_STREAM_PARAMETERS
#define DPLOG_IF(severity, condition) EAT_STREAM_PARAMETERS

#endif  // DCHECK_IS_ON()

#define DLOG(severity) LAZY_STREAM(LOG_STREAM(severity), DLOG_IS_ON(severity))
#define DPLOG(severity) LAZY_STREAM(PLOG_STREAM(severity), DLOG_IS_ON(severity))

// Definitions for DCHECK et al.

// DCHECK et al. make sure to reference |condition| regardless of
// whether DCHECKs are enabled; this is so that we don't get unused
// variable warnings if the only use of a variable is in a DCHECK.
// This behavior is different from DLOG_IF et al.
//
// Note that the definition of the DCHECK macros depends on whether or not
// DCHECK_IS_ON() is true. When DCHECK_IS_ON() is false, the macros use
// EAT_STREAM_PARAMETERS to avoid expressions that would create temporaries.

#if DCHECK_IS_ON()

#define DCHECK(condition) \
  LAZY_STREAM(LOG_STREAM(LS_DCHECK), !(condition)) << "Check failed: " #condition ". "
#define DPCHECK(condition) \
  LAZY_STREAM(PLOG_STREAM(LS_DCHECK), !(condition)) << "Check failed: " #condition ". "

#else  // DCHECK_IS_ON()

#define DCHECK(condition) EAT_STREAM_PARAMETERS << !(condition)
#define DPCHECK(condition) EAT_STREAM_PARAMETERS << !(condition)

#endif  // DCHECK_IS_ON()

// Helper macro for binary operators.
// Don't use this macro directly in your code, use DCHECK_EQ et al below.
// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   DCHECK_EQ(2, a);
#if DCHECK_IS_ON()

#define DCHECK_OP(name, op, val1, val2)                                \
  switch (0) case 0: default:                                          \
  if (aspia::CheckOpResult true_if_passed =                            \
      DCHECK_IS_ON() ?                                                 \
      aspia::Check##name##Impl((val1), (val2),                         \
                                   #val1 " " #op " " #val2) : nullptr) \
   ;                                                                   \
  else                                                                 \
    aspia::LogMessage(__FILE__, __LINE__, aspia::LS_DCHECK,            \
                          true_if_passed.message()).stream()

#else  // DCHECK_IS_ON()

// When DCHECKs aren't enabled, DCHECK_OP still needs to reference operator<<
// overloads for |val1| and |val2| to avoid potential compiler warnings about
// unused functions. For the same reason, it also compares |val1| and |val2|
// using |op|.
//
// Note that the contract of DCHECK_EQ, etc is that arguments are only evaluated
// once. Even though |val1| and |val2| appear twice in this version of the macro
// expansion, this is OK, since the expression is never actually evaluated.
#define DCHECK_OP(name, op, val1, val2)                             \
  EAT_STREAM_PARAMETERS << (aspia::MakeCheckOpValueString(          \
                                aspia::g_swallow_stream, val1),     \
                            aspia::MakeCheckOpValueString(          \
                                aspia::g_swallow_stream, val2),     \
                            (val1)op(val2))

#endif  // DCHECK_IS_ON()

// Equality/Inequality checks - compare two values, and log a
// LOG_DCHECK message including the two values when the result is not
// as expected.  The values must have operator<<(ostream, ...)
// defined.
//
// You may append to the error message like so:
//   DCHECK_NE(1, 2) << "The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   DCHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These don't compile correctly if one of the arguments is a pointer
// and the other is NULL.  In new code, prefer nullptr instead.  To
// work around this for C++98, simply static_cast NULL to the type of the
// desired pointer.

#define DCHECK_EQ(val1, val2) DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) DCHECK_OP(GT, > , val1, val2)

#define NOTREACHED() DCHECK(false)

// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
class LogMessage
{
public:
    // Used for LOG(severity).
    LogMessage(const char* file, int line, LoggingSeverity severity);

    // Used for CHECK().  Implied severity = LOG_FATAL.
    LogMessage(const char* file, int line, const char* condition);

    // Used for CHECK_EQ(), etc. Takes ownership of the given string.
    // Implied severity = LOG_FATAL.
    LogMessage(const char* file, int line, std::string* result);

    // Used for DCHECK_EQ(), etc. Takes ownership of the given string.
    LogMessage(const char* file, int line, LoggingSeverity severity, std::string* result);

    ~LogMessage();

    std::ostream& stream() { return stream_; }

    LoggingSeverity severity() { return severity_; }
    std::string str() { return stream_.str(); }

private:
    void Init(const char* file, int line);

    LoggingSeverity severity_;
    std::ostringstream stream_;
    size_t message_start_;  // Offset of the start of the message (past prefix // info).
    // The file and line information passed in to the constructor.
    const char* file_;
    const int line_;

    // Stores the current value of GetLastError in the constructor and restores
    // it in the destructor by calling SetLastError.
    // This is useful since the LogMessage class uses a lot of Win32 calls
    // that will lose the value of GLE and the code that called the log function
    // will have lost the thread error value when the log call returns.
    class SaveLastError
    {
    public:
        SaveLastError();
        ~SaveLastError();

        unsigned long get_error() const { return last_error_; }

    protected:
        unsigned long last_error_;
    };

    SaveLastError last_error_;

    DISALLOW_COPY_AND_ASSIGN(LogMessage);
};

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify
{
public:
    LogMessageVoidify() = default;
    // This has to be an operator with a precedence lower than << but
    // higher than ?:
    void operator&(std::ostream&) { }
};

using SystemErrorCode = DWORD;

// Alias for ::GetLastError() on Windows and errno on POSIX. Avoids having to
// pull in windows.h just for GetLastError() and DWORD.
SystemErrorCode GetLastSystemErrorCode();
std::string SystemErrorCodeToString(SystemErrorCode error_code);

// Appends a formatted system message of the GetLastError() type.
class Win32ErrorLogMessage
{
public:
    Win32ErrorLogMessage(const char* file,
                         int line,
                         LoggingSeverity severity,
                         SystemErrorCode err);

    // Appends the error message before destructing the encapsulated class.
    ~Win32ErrorLogMessage();

    std::ostream& stream() { return log_message_.stream(); }

private:
    SystemErrorCode err_;
    LogMessage log_message_;

    DISALLOW_COPY_AND_ASSIGN(Win32ErrorLogMessage);
};

// Closes the log file explicitly if open.
// NOTE: Since the log file is opened as necessary by the action of logging
//       statements, there's no guarantee that it will stay closed
//       after this call.
void CloseLogFile();

// Async signal safe logging mechanism.
void RawLog(int level, const char* message);

#define RAW_LOG(level, message) \
    aspia::RawLog(aspia::LOG_##level, message)

#define RAW_CHECK(condition)                                                   \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
            aspia::RawLog(aspia::LOG_FATAL, "Check failed: " #condition "\n"); \
    } while (0)

// Returns true if logging to file is enabled.
bool IsLoggingToFileEnabled();

// Returns the default log file path.
std::experimental::filesystem::path GetLogFileFullPath();

} // namespace aspia

// Note that "The behavior of a C++ program is undefined if it adds declarations
// or definitions to namespace std or to a namespace within namespace std unless
// otherwise specified." --C++11[namespace.std]
//
// We've checked that this particular definition has the intended behavior on
// our implementations, but it's prone to breaking in the future, and please
// don't imitate this in your own definitions without checking with some
// standard library experts.
namespace std {

// These functions are provided as a convenience for logging, which is where we
// use streams (it is against Google style to use streams in other places). It
// is designed to allow you to emit non-ASCII Unicode strings to the log file,
// which is normally ASCII. It is relatively slow, so try not to use it for
// common cases. Non-ASCII characters will be converted to UTF-8 by these
// operators.
std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
inline std::ostream& operator<<(std::ostream& out, const std::wstring& wstr)
{
    return out << wstr.c_str();
}

}  // namespace std

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE can be used.
#define NOTIMPLEMENTED_MSG "NOT IMPLEMENTED"

#define NOTIMPLEMENTED() LOG(ERROR) << NOTIMPLEMENTED_MSG
#define NOTIMPLEMENTED_LOG_ONCE()                          \
    do                                                     \
    {                                                      \
        static bool logged_once = false;                   \
        LOG_IF(ERROR, !logged_once) << NOTIMPLEMENTED_MSG; \
        logged_once = true;                                \
    } while (0);                                           \
    EAT_STREAM_PARAMETERS

#endif // _ASPIA_BASE__LOGGING_H

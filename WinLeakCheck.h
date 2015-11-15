#if !defined(RESIP_WINLEAKCHECK_HXX)
#define RESIP_WINLEAKCHECK_HXX 

// Used for tracking down memory leaks in Visual Studio
#if defined(WIN32) && defined(_DEBUG) && defined(LEAK_CHECK) 
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new   new( _NORMAL_BLOCK, __FILE__, __LINE__)


/**
 *  Creating a single instance of this class at start up will cause
 *  memory leak information to be dumped to the debug window when
 *  the program terminates.  
 */
class FindMemoryLeaks
{
    _CrtMemState m_checkpoint;
public:
    FindMemoryLeaks()
    {
        _CrtMemCheckpoint(&m_checkpoint);
    };
    ~FindMemoryLeaks()
    {
        _CrtMemState checkpoint;
        _CrtMemCheckpoint(&checkpoint);
        _CrtMemState diff;
        _CrtMemDifference(&diff, &m_checkpoint, &checkpoint);
        _CrtMemDumpStatistics(&diff);
        _CrtMemDumpAllObjectsSince(&m_checkpoint);
    };
};


#endif

#endif


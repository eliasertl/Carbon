#include "Carbon/Backends/OpenGL/OpenGLFunctionsInternal.h"

namespace Carbon::Internal
{
    bool OpenGLFunctions::Load(OpenGLProcLoader getProcAddress, bool isES, std::string_view& missing)
    {
        // A function pointer of one type converts to another with reinterpret_cast; the host's loader returns the
        // address as a generic function pointer.
#define CB_OPENGL_LOAD(name, result, parameters)                                             \
    name = reinterpret_cast<result(CB_OPENGL_CALL*) parameters>(getProcAddress("gl" #name)); \
    if (name == nullptr)                                                                     \
    {                                                                                        \
        missing = "gl" #name;                                                                \
        return false;                                                                        \
    }
        CB_OPENGL_FUNCTIONS(CB_OPENGL_LOAD)
        if (!isES)
        {
            CB_OPENGL_DESKTOP_FUNCTIONS(CB_OPENGL_LOAD)
        }
#undef CB_OPENGL_LOAD
        return true;
    }
} // namespace Carbon::Internal

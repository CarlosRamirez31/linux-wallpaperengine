#include "GLFWOpenGLDriver.h"
#include "VideoFactories.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#ifdef ENABLE_X11
#include "WallpaperEngine/Render/Drivers/Output/X11Output.h"
#endif

#define GLFW_EXPOSE_NATIVE_X11
#include "WallpaperEngine/Debugging/CallStack.h"

#include <GLFW/glfw3native.h>

#include <unistd.h>
#include <cstdlib>
#ifdef ENABLE_X11
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#endif

using namespace WallpaperEngine::Render::Drivers;

void CustomGLFWErrorHandler (int errorCode, const char* reason) { sLog.error ("GLFW error ", errorCode, ": ", reason); }

GLFWOpenGLDriver::GLFWOpenGLDriver (const char* windowTitle, ApplicationContext& context, WallpaperApplication& app) :
    VideoDriver (app, m_mouseInput), m_context (context), m_mouseInput (*this) {
    glfwSetErrorCallback (CustomGLFWErrorHandler);

    // initialize glfw
    if (glfwInit () == GLFW_FALSE) {
	sLog.exception ("Failed to initialize glfw");
    }

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);
    // set X11-specific hints
    glfwWindowHintString (GLFW_X11_CLASS_NAME, "linux-wallpaperengine");
    glfwWindowHintString (GLFW_X11_INSTANCE_NAME, "linux-wallpaperengine");

    // for forced window mode, we can set some hints that'll help position the window
    if (context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW) {
	glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
	glfwWindowHint (GLFW_FLOATING, GLFW_TRUE);
    }

#if !NDEBUG
    glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif /* DEBUG */

    // Local desktop integration is opt-in; retain upstream preview behavior.
    const bool desktop = std::getenv ("BOLLOW_DESKTOP") != nullptr;
    if (desktop) {
        glfwWindowHint (GLFW_FLOATING, GLFW_FALSE);
        glfwWindowHint (GLFW_FOCUSED, GLFW_FALSE);
        glfwWindowHint (GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    }

    // create window, size doesn't matter as long as we don't show it
    this->m_window = glfwCreateWindow (640, 480, windowTitle, nullptr, nullptr);

    if (this->m_window == nullptr) {
	sLog.exception ("Cannot create window");
    }

#ifdef ENABLE_X11
    if (desktop) {
        Display* display = glfwGetX11Display ();
        const Window window = glfwGetX11Window (this->m_window);
        XSetWindowAttributes attributes {};
        attributes.override_redirect = True;
        XChangeWindowAttributes (display, window, CWOverrideRedirect, &attributes);
        Atom type = XInternAtom (display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        XChangeProperty (display, window, XInternAtom (display, "_NET_WM_WINDOW_TYPE", False),
                         XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*> (&type), 1);
        // Mouse clicks pass through to Awesome's desktop bindings.
        XShapeCombineRectangles (display, window, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
    }
#endif

    if (std::getenv ("BOLLOW_PREVIEW")) {
        if (const char* pipe = std::getenv ("BOLLOW_PREVIEW_VIDEO"))
            m_previewOutput.open (pipe, std::ios::binary);
    }

    // make context current, required for glew initialization
    glfwMakeContextCurrent (this->m_window);

    // initialize glew for rendering
    if (const GLenum result = glewInit (); result != GLEW_OK) {
	sLog.error ("Failed to initialize GLEW: ", glewGetErrorString (result));
    }

    // setup output
    if (context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW
	|| context.settings.render.mode == ApplicationContext::NORMAL_WINDOW) {
	m_output = new WallpaperEngine::Render::Drivers::Output::GLFWWindowOutput (context, *this);
    }
#ifdef ENABLE_X11
    else {
	m_output = new WallpaperEngine::Render::Drivers::Output::X11Output (context, *this);
    }
#else
    else {
	sLog.exception ("Trying to start GLFW in background mode without X11 support installed. Bailing out");
    }
#endif
}

GLFWOpenGLDriver::~GLFWOpenGLDriver () { glfwTerminate (); }

Output::Output& GLFWOpenGLDriver::getOutput () { return *this->m_output; }

float GLFWOpenGLDriver::getRenderTime () const { return static_cast<float> (glfwGetTime ()); }

bool GLFWOpenGLDriver::closeRequested () { return glfwWindowShouldClose (this->m_window); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec2 size) { glfwSetWindowSize (this->m_window, size.x, size.y); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    glfwSetWindowPos (this->m_window, sizeandpos.x, sizeandpos.y);
    glfwSetWindowSize (this->m_window, sizeandpos.z, sizeandpos.w);
}

void GLFWOpenGLDriver::showWindow () {
    if (std::getenv ("BOLLOW_PREVIEW")) return;
    glfwShowWindow (this->m_window);
#ifdef ENABLE_X11
    if (std::getenv ("BOLLOW_DESKTOP"))
        XLowerWindow (glfwGetX11Display (), glfwGetX11Window (this->m_window));
#endif
}

void GLFWOpenGLDriver::hideWindow () { glfwHideWindow (this->m_window); }

glm::ivec2 GLFWOpenGLDriver::getFramebufferSize () const {
    glm::ivec2 size;

    glfwGetFramebufferSize (this->m_window, &size.x, &size.y);

    return size;
}

uint32_t GLFWOpenGLDriver::getFrameCounter () const { return this->m_frameCounter; }

void GLFWOpenGLDriver::dispatchEventQueue () {
    static float startTime, endTime, minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;
    // get the start time of the frame
    startTime = this->getRenderTime ();
    // clear the screen
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);
    }

    // read the full texture into the image
    if (this->m_output->haveImageBuffer ()) {
	// 4.5 supports glReadnPixels, anything older doesn't...
	if (GLEW_VERSION_4_5) {
	    glReadnPixels (
		0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
		this->m_output->getImageBufferSize (), this->m_output->getImageBuffer ()
	    );
	} else {
	    // fallback to old version
	    glReadPixels (
		0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
		this->m_output->getImageBuffer ()
	    );
	}

	GLenum error = glGetError ();

	if (error != GL_NO_ERROR) {
	    sLog.exception ("OpenGL error when reading texture ", error);
	}
    }

    // TODO: FRAMETIME CONTROL SHOULD GO BACK TO THE CWALLPAPAERAPPLICATION ONCE ACTUAL PARTICLES ARE IMPLEMENTED
    // TODO: AS THOSE, MORE THAN LIKELY, WILL REQUIRE OF A DIFFERENT PROCESSING RATE
    // update the output with the given image
    this->m_output->updateRender ();
    // Export only the short cached hover preview, never during desktop playback.
    if (m_previewOutput.is_open () && m_frameCounter >= 90) {
        const auto size = getFramebufferSize ();
        m_previewPixels.resize (size.x * size.y * 3);
        glPixelStorei (GL_PACK_ALIGNMENT, 1);
        glReadPixels (0, 0, size.x, size.y, GL_RGB, GL_UNSIGNED_BYTE, m_previewPixels.data ());
        m_previewOutput.write (reinterpret_cast<const char*> (m_previewPixels.data ()), m_previewPixels.size ());
        if (m_frameCounter >= 209) {
            m_previewOutput.close ();
            m_context.state.general.keepRunning = false;
        }
    }
    // do buffer swapping first
    glfwSwapBuffers (this->m_window);
    // Publish readiness only after a complete desktop frame has been presented.
    if (m_frameCounter == 0) {
        if (const char* ready = std::getenv ("BOLLOW_READY_FILE")) {
            std::ofstream (ready) << "ready\n";
        }
    }
    // poll for events
    glfwPollEvents ();
    // increase frame counter
    this->m_frameCounter++;
    // get the end time of the frame
    endTime = this->getRenderTime ();

    // ensure the frame time is correct to not overrun FPS
    if ((endTime - startTime) < minimumTime) {
	usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);
    }
}

void* GLFWOpenGLDriver::getProcAddress (const char* name) const {
    return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

GLFWwindow* GLFWOpenGLDriver::getWindow () const { return this->m_window; }

__attribute__ ((constructor)) void registerGLFWOpenGLDriver () {
    sVideoFactories.registerDriver (
	ApplicationContext::DESKTOP_BACKGROUND, "x11",
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
    sVideoFactories.registerDriver (
	ApplicationContext::EXPLICIT_WINDOW, DEFAULT_WINDOW_NAME,
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
    sVideoFactories.registerDriver (
	ApplicationContext::NORMAL_WINDOW, DEFAULT_WINDOW_NAME,
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
}

#include "GLFWMouseInput.h"
#include <glm/common.hpp>
#include <cstdlib>


#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#ifdef ENABLE_X11
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

using namespace WallpaperEngine::Input::Drivers;

GLFWMouseInput::GLFWMouseInput (const Render::Drivers::GLFWOpenGLDriver& driver) : m_driver (driver) { }

void GLFWMouseInput::update () {
    if (!this->m_driver.getApp ().getContext ().settings.mouse.enabled) {
	this->m_reportedPosition = { 0, 0 };
	return;
    }

    const int leftClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_LEFT);
    const int rightClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_RIGHT);

    this->m_leftClick = leftClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;
    this->m_rightClick = rightClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;

    // update current mouse position
    glfwGetCursorPos (this->m_driver.getWindow (), &this->m_mousePosition.x, &this->m_mousePosition.y);

#ifdef ENABLE_X11
    if (std::getenv ("BOLLOW_DESKTOP")) {
        // The desktop is click-through: GLFW receives no button events here.
        Window root, child;
        int rootX, rootY, x, y;
        unsigned int mask;
        Display* display = glfwGetX11Display ();
        const Window window = glfwGetX11Window (this->m_driver.getWindow ());
        if (XQueryPointer (display, window, &root, &child, &rootX, &rootY, &x, &y, &mask)) {
            this->m_mousePosition = { x, y };
            Window rootChild;
            XQueryPointer (display, root, &root, &rootChild, &rootX, &rootY, &x, &y, &mask);
            const bool desktop = rootChild == None || rootChild == window;
            this->m_leftClick = desktop && (mask & Button1Mask) ? MouseClickStatus::Clicked : MouseClickStatus::Released;
            this->m_rightClick = desktop && (mask & Button3Mask) ? MouseClickStatus::Clicked : MouseClickStatus::Released;
        }
    }
#endif
    if (std::getenv ("BOLLOW_PREVIEW")) {
        const auto size = this->m_driver.getFramebufferSize ();
        // Stable representative position; moving the real mouse cannot disturb capture.
        this->m_mousePosition = { size.x / 2.0, size.y / 2.0 };
    }

    // Convert from GLFW coordinate system (Y=0 at top) to OpenGL coordinate system (Y=0 at bottom)
    const glm::ivec2 framebufferSize = this->m_driver.getFramebufferSize ();
    this->m_mousePosition.y = static_cast<double> (framebufferSize.y) - this->m_mousePosition.y;

    this->m_mousePosition.x = glm::clamp (this->m_mousePosition.x, 0.0, static_cast<double> (framebufferSize.x));
    this->m_mousePosition.y = glm::clamp (this->m_mousePosition.y, 0.0, static_cast<double> (framebufferSize.y));

    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 GLFWMouseInput::position () const { return this->m_reportedPosition; }

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::leftClick () const { return m_leftClick; }

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::rightClick () const { return m_rightClick; }
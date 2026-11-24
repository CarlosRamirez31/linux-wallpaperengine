#pragma once

#include "include/cef_client.h"
#include "include/cef_load_handler.h"

namespace WallpaperEngine::WebBrowser::CEF {
// *************************************************************************
//! \brief Provide access to browser-instance-specific callbacks. A single
//! CefClient instance can be shared among any number of browsers.
// *************************************************************************
class BrowserClient : public CefClient, public CefLoadHandler {
public:
    explicit BrowserClient (CefRefPtr<CefRenderHandler> ptr, std::string initialization);
    CefRefPtr<CefLoadHandler> GetLoadHandler () override { return this; }
    void OnLoadEnd (CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int status) override;
    std::string m_initialization;

    [[nodiscard]] CefRefPtr<CefRenderHandler> GetRenderHandler () override;

    CefRefPtr<CefRenderHandler> m_renderHandler = nullptr;

    IMPLEMENT_REFCOUNTING (BrowserClient);
};
} // namespace WallpaperEngine::WebBrowser::CEF
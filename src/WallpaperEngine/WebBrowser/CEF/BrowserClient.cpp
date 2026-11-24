#include "BrowserClient.h"

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserClient::BrowserClient (CefRefPtr<CefRenderHandler> ptr, std::string initialization) :
    m_initialization (std::move (initialization)), m_renderHandler (std::move (ptr)) { }

void BrowserClient::OnLoadEnd (CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int status) {
    if (frame->IsMain ()) frame->ExecuteJavaScript (m_initialization, frame->GetURL (), 0);
}

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler () { return m_renderHandler; }
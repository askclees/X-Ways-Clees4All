#ifndef VISUALSTYLES_H
#define VISUALSTYLES_H

#include <windows.h>
#include <commctrl.h>

// RAII helper that activates a ComCtl32 v6 activation context for the
// lifetime of the enclosing scope.  Uses the manifest embedded in
// shell32.dll (resource 124) -- the standard technique for DLLs that
// cannot carry their own manifest.
class VisualStylesScope
{
public:
    VisualStylesScope() : m_cookie(0), m_hCtx(INVALID_HANDLE_VALUE)
    {
        wchar_t sysDir[MAX_PATH] = {0};
        GetSystemDirectoryW(sysDir, MAX_PATH);

        ACTCTXW actCtx = {0};
        actCtx.cbSize = sizeof(actCtx);
        actCtx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID
                       | ACTCTX_FLAG_SET_PROCESS_DEFAULT
                       | ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
        actCtx.lpSource        = L"shell32.dll";
        actCtx.lpAssemblyDirectory = sysDir;
        actCtx.lpResourceName  = MAKEINTRESOURCEW(124);

        m_hCtx = CreateActCtxW(&actCtx);
        if (m_hCtx != INVALID_HANDLE_VALUE)
        {
            ActivateActCtx(m_hCtx, &m_cookie);
        }

        INITCOMMONCONTROLSEX icc = {0};
        icc.dwSize = sizeof(icc);
        icc.dwICC  = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);
    }

    ~VisualStylesScope()
    {
        if (m_cookie)
        {
            DeactivateActCtx(0, m_cookie);
        }
        if (m_hCtx != INVALID_HANDLE_VALUE)
        {
            ReleaseActCtx(m_hCtx);
        }
    }

    // Non-copyable
    VisualStylesScope(const VisualStylesScope&) = delete;
    VisualStylesScope& operator=(const VisualStylesScope&) = delete;

private:
    ULONG_PTR m_cookie;
    HANDLE    m_hCtx;
};

#endif // VISUALSTYLES_H

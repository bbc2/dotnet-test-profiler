#include "ClassFactory.h"

const IID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID IID_IClassFactory = {0x00000001, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

BOOL STDMETHODCALLTYPE DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return TRUE;
}

extern "C" HRESULT STDMETHODCALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    // {cf0d821e-299b-5307-a3d8-9ccb4916d2e5}
    const GUID CLSID_CorProfiler = {
        0xcf0d821e, 0x299b, 0x5307, {0xa3, 0xd8, 0x9c, 0xcb, 0x49, 0x16, 0xd2, 0xe5}
    };

    if (ppv == nullptr || rclsid != CLSID_CorProfiler) {
        return E_FAIL;
    }

    auto factory = new ClassFactory;
    if (factory == nullptr) {
        return E_FAIL;
    }

    return factory->QueryInterface(riid, ppv);
}

extern "C" HRESULT STDMETHODCALLTYPE DllCanUnloadNow() {
    return S_OK;
}

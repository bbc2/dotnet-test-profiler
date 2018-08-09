#include "CorProfiler.h"
#include "corhlpr.h"
#include "CComPtr.h"
#include "profiler_pal.h"
#include <codecvt>
#include <iostream>
#include <locale>
#include <string>

static CorProfiler* profiler = nullptr;

std::string ToBytes(std::wstring wide) {
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    return converter.to_bytes(wide);
}

std::wstring ToWideString(WCHAR array[], size_t size) {
    wchar_t* stringWide = new wchar_t[size];
    for (int i = 0; i < size; i++) {
        stringWide[i] = (wchar_t) array[i];
    }
    return std::wstring(stringWide);
}

std::wstring GetModulePath(ICorProfilerInfo2& info, ModuleID moduleId, AssemblyID& assemblyId) {
    ULONG size;
    info.GetModuleInfo(moduleId, NULL, 0, &size, nullptr, &assemblyId);

    WCHAR* path = new WCHAR[size];
    HRESULT result = info.GetModuleInfo(moduleId, NULL, size, &size, path, &assemblyId);
    if (FAILED(result)) {
        printf("Error: GetModuleInfo\n");
        return std::wstring();
    }

    return ToWideString(path, size);
}

bool GetFunctionInfo(
        [in] ICorProfilerInfo2& info,
        [in] FunctionID funcId,
        [out] mdToken& functionToken,
        [out] ModuleID& moduleId,
        [out] std::wstring& modulePath,
        [out] AssemblyID& assemblyId
) {
    HRESULT result = info.GetFunctionInfo2(funcId, 0, NULL, &moduleId, &functionToken, 0, NULL, NULL);
    if (FAILED(result)) {
        printf("Error: GetFunctionInfo2 %x\n", result);
        return false;
    }
    modulePath = GetModulePath(info, moduleId, assemblyId);
    return true;
}

std::wstring GetAssemblyName(ICorProfilerInfo2& info, AssemblyID assemblyId) {
    ULONG size;
    info.GetAssemblyInfo(assemblyId, 0, &size, nullptr, NULL, NULL);

    WCHAR *name = new WCHAR[size];
    HRESULT result = info.GetAssemblyInfo(assemblyId, size, &size, name, NULL, NULL);
    if (FAILED(result)) {
        printf("Error: GetAssemblyInfo %x\n", result);
        return std::wstring();
    }

    return ToWideString(name, size);
}

std::wstring GetFunctionName(
        CComPtr<IMetaDataImport2>& metaDataImport2,
        mdToken functionToken,
        mdTypeDef& classId
) {
    ULONG size;
    metaDataImport2->GetMethodProps(
            functionToken,
            &classId,
            nullptr,
            512,
            &size,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
    );

    WCHAR *name = new WCHAR[size];
    HRESULT result = metaDataImport2->GetMethodProps(
            functionToken,
            &classId,
            name,
            size,
            &size,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
    );
    if (FAILED(result)) {
        printf("Error: GetMethodProps %x\n", result);
        return std::wstring();
    }

    return ToWideString(name, size);
}

std::wstring GetFunctionType(
        CComPtr<IMetaDataImport2>& metaDataImport2,
        mdTypeDef classId
) {
    ULONG size;
    metaDataImport2->GetTypeDefProps(classId, nullptr, 0, &size, nullptr, nullptr);

    WCHAR *type = new WCHAR[size];
    HRESULT result = metaDataImport2->GetTypeDefProps(classId, type, size, &size, nullptr, nullptr);
    if (FAILED(result)) {
        printf("Error: GetTypeDefProps %x\n", result);
        return std::wstring();
    }

    return ToWideString(type, size);
}

std::wstring GetTypeAndMethodName(ICorProfilerInfo2& info, FunctionID functionId) {
    CComPtr<IMetaDataImport2> metaDataImport2;
    mdMethodDef functionToken;
    info.GetTokenAndMetaDataFromFunction(
            functionId,
            IID_IMetaDataImport,
            (IUnknown **) &metaDataImport2,
            &functionToken
    );

    mdTypeDef classId;
    std::wstring name = GetFunctionName(metaDataImport2, functionToken, classId);
    std::wstring type = GetFunctionType(metaDataImport2, classId);

    return type + L"::" + name;
}

PROFILER_STUB EnterStub(
        FunctionID functionId,
        COR_PRF_ELT_INFO eltInfo
) {
    ICorProfilerInfo3 *info = profiler->corProfilerInfo;
    printf("EnterStub %lu\n", functionId);

    COR_PRF_FRAME_INFO frameInfo;
    ULONG argumentInfoSize = 0;

    info->GetFunctionEnter3Info(
            functionId,
            eltInfo,
            &frameInfo,
            &argumentInfoSize,
            nullptr
    );

    COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo =
        new COR_PRF_FUNCTION_ARGUMENT_INFO[argumentInfoSize];

    HRESULT result = info->GetFunctionEnter3Info(
            functionId,
            eltInfo,
            &frameInfo,
            &argumentInfoSize,
            argumentInfo
    );
    if (FAILED(result)) {
        printf("Error: GetFunctionEnter3Info %x", result);
    }

    printf(
            "  argumentInfo:\n    numRanges: %u\n    totalArgumentSize: %u\n    ranges:\n",
            argumentInfo->numRanges,
            argumentInfo->totalArgumentSize
    );

    for (uint64_t i = 0; i < argumentInfo->numRanges; i++) {
        COR_PRF_FUNCTION_ARGUMENT_RANGE range = argumentInfo->ranges[i];
        printf(
                "      startAddress: %p\n      length: %u\n",
                (void *) range.startAddress,
                range.length
        );

        printf("      data:");
        for (UINT_PTR index = 0; index < range.length; index++) {
            uint8_t byte = *(((uint8_t *) range.startAddress) + index);
            printf(" %02x", byte);
        }
        printf("\n");
    }
}

PROFILER_STUB LeaveStub(
        FunctionID functionId,
        COR_PRF_ELT_INFO eltInfo
) {
}

PROFILER_STUB TailcallStub(
        FunctionID functionId,
        COR_PRF_ELT_INFO eltInfo
) {
}

EXTERN_C void EnterNaked(
        FunctionIDOrClientID functionIDOrClientID,
        COR_PRF_ELT_INFO eltInfo
);
EXTERN_C void LeaveNaked(
        FunctionIDOrClientID functionIDOrClientID,
        COR_PRF_ELT_INFO eltInfo
);
EXTERN_C void TailcallNaked(
        FunctionIDOrClientID functionIDOrClientID,
        COR_PRF_ELT_INFO eltInfo
);

UINT_PTR __stdcall _FunctionIDMapper2(
        [in] FunctionID functionId,
        [in] void *clientData,
        [out] BOOL *pbHookFunction
) {
    ICorProfilerInfo2& info = *static_cast<ICorProfilerInfo2 *>(clientData);

    std::wstring modulePath;
    mdToken functionToken;
    ModuleID moduleId;
    AssemblyID assemblyId;
    GetFunctionInfo(info, functionId, functionToken, moduleId, modulePath, assemblyId);
    std::wstring assemblyName = GetAssemblyName(info, assemblyId);
    std::wstring signature = GetTypeAndMethodName(info, functionId);

    if (
            assemblyName == L"foo"
            && signature == L"foo.Program::foo"
    ) {
        printf(
                "Mapping:\n  Module: %s\n  Assembly: %s\n  Signature: %s\n",
                ToBytes(modulePath).c_str(),
                ToBytes(assemblyName).c_str(),
                ToBytes(signature).c_str()
        );
        *pbHookFunction = true;
    } else {
        *pbHookFunction = false;
    };
    return functionId;
};

CorProfiler::CorProfiler() : refCount(0), corProfilerInfo(nullptr)
{
}

CorProfiler::~CorProfiler()
{
    if (this->corProfilerInfo != nullptr)
    {
        this->corProfilerInfo->Release();
        this->corProfilerInfo = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE CorProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
{
    HRESULT queryInterfaceResult = pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo8), reinterpret_cast<void **>(&this->corProfilerInfo));

    if (FAILED(queryInterfaceResult))
    {
        return E_FAIL;
    }

    DWORD eventMask = (
        COR_PRF_MONITOR_ENTERLEAVE
        | COR_PRF_ENABLE_FRAME_INFO
        | COR_PRF_ENABLE_FUNCTION_ARGS
        | COR_PRF_ENABLE_FUNCTION_RETVAL
    );
    HRESULT result = this->corProfilerInfo->SetEventMask2(eventMask, COR_PRF_HIGH_MONITOR_NONE);

    HRESULT monitorResult = this->corProfilerInfo->SetEnterLeaveFunctionHooks3WithInfo(
            EnterNaked,
            LeaveNaked,
            TailcallNaked
    );
    if (FAILED(monitorResult)) {
        return E_FAIL;
    };

    result = this->corProfilerInfo->SetFunctionIDMapper2(
            _FunctionIDMapper2,
            this->corProfilerInfo
    );
    if (FAILED(result)) {
        return E_FAIL;
    };

    if (profiler != nullptr) {
        printf("Profiler already initialized\n");
        return E_FAIL;
    }
    profiler = this;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::Shutdown()
{
    if (this->corProfilerInfo != nullptr)
    {
        this->corProfilerInfo->Release();
        this->corProfilerInfo = nullptr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainCreationStarted(AppDomainID appDomainId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainShutdownStarted(AppDomainID appDomainId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyLoadStarted(AssemblyID assemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyUnloadStarted(AssemblyID assemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadStarted(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleUnloadStarted(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID AssemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassLoadStarted(ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassLoadFinished(ClassID classId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassUnloadStarted(ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassUnloadFinished(ClassID classId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::FunctionUnloadStarted(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCachedFunctionSearchStarted(FunctionID functionId, BOOL *pbUseCachedFunction)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITFunctionPitched(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITInlining(FunctionID callerId, FunctionID calleeId, BOOL *pfShouldInline)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadCreated(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadDestroyed(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientInvocationStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientSendingMessage(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientReceivingReply(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientInvocationFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerReceivingMessage(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerInvocationStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerInvocationReturned()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerSendingReply(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendAborted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeResumeStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeResumeFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeThreadSuspended(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeThreadResumed(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], ULONG cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ObjectAllocated(ObjectID objectId, ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[], ULONG cObjects[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RootReferences(ULONG cRootRefs, ObjectID rootRefIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionThrown(ObjectID thrownObjectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFunctionEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFunctionLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFilterEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFilterLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchCatcherFound(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionOSHandlerEnter(UINT_PTR __unused)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionOSHandlerLeave(UINT_PTR __unused)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFunctionEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFunctionLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFinallyEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFinallyLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCatcherLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable, ULONG cSlots)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCLRCatcherFound()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCLRCatcherExecute()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GarbageCollectionStarted(int cGenerations, BOOL generationCollected[], COR_PRF_GC_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], ULONG cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GarbageCollectionFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[], COR_PRF_GC_ROOT_KIND rootKinds[], COR_PRF_GC_ROOT_FLAGS rootFlags[], UINT_PTR rootIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::HandleCreated(GCHandleID handleId, ObjectID initialObjectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::HandleDestroyed(GCHandleID handleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::InitializeForAttach(IUnknown *pCorProfilerInfoUnk, void *pvClientData, UINT cbClientData)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ProfilerAttachComplete()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ProfilerDetachSucceeded()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ReJITCompilationStarted(FunctionID functionId, ReJITID rejitId, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GetReJITParameters(ModuleID moduleId, mdMethodDef methodId, ICorProfilerFunctionControl *pFunctionControl)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ReJITCompilationFinished(FunctionID functionId, ReJITID rejitId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ReJITError(ModuleID moduleId, mdMethodDef methodId, FunctionID functionId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::MovedReferences2(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::SurvivingReferences2(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ConditionalWeakTableElementReferences(ULONG cRootRefs, ObjectID keyRefIds[], ObjectID valueRefIds[], GCHandleID rootIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GetAssemblyReferences(const WCHAR *wszAssemblyPath, ICorProfilerAssemblyReferenceProvider *pAsmRefProvider)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleInMemorySymbolsUpdated(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock, LPCBYTE ilHeader, ULONG cbILHeader)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    return S_OK;
}

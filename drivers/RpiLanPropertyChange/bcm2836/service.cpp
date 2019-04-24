//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Module Name:
//
//     service.cpp
//
// Abstract:
//
//     This file defines the service interfaces. 
//
#include "stdafx.h"
#include "Service.h"
#include <cstdint>
#include <thread>


using namespace RpiLanPropertyChange;

namespace
{
    // 'The service'
    Service g_service;

    //
    // Thunk function for RegisterServiceCtrlHandlerEx
    //
    DWORD WINAPI ServiceHandlerThunk(DWORD opcode, DWORD eventType, _In_ void* pEventData, _In_ void* pContext)
    {
        Service* pThis = reinterpret_cast<Service*>(pContext);
        return pThis->ServiceHandler(opcode, eventType, pEventData);
    }
}

//
// Service::ctor
//
Service::Service() :
    m_hServiceStatus(nullptr),
    m_LanDevice(nullptr)
{
    ZeroMemory(&m_serviceStatus, sizeof(m_serviceStatus));
    m_serviceStatus.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
}

//
// Entry point of the service (invoked by sdvhost framework). It works as a Initialize method.
//
void Service::ServiceMain(_In_ DWORD /*argc*/, _In_ PTSTR argv[])
{
    HRESULT hr = S_OK;

    m_hServiceStatus = ::RegisterServiceCtrlHandlerEx(argv[0], ServiceHandlerThunk, this);
    if (m_hServiceStatus == nullptr)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError()); // Failed to register service control handler
    }

    if (SUCCEEDED(hr))
    {
        hr = UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR);
    }

    if (SUCCEEDED(hr))
    {
        // Do full initialization of the host on a different Thread
        // std::thread takes a ref count on the module before starting the thread and releases the
        // reference on thread exit. Hence we shouldn't worry about module ref counting here and in fact
        // is unsafe since without the std::thread guarantee, there is no guarantee that the module
        // is loaded when this method is running. On top of that, calling FreeLibraryAndExitThread()
        // here will exit the thread and leak the ref count taken by std::thread. Hence keep it simple
        // and don't bother with module ref count here.
        std::thread([=]()
        {
            Start();
        }).detach();
    }

    if (FAILED(hr))
    {
        Stop();
    }
}

//
// Performs startup of the service, normally invoked by svchost via ServiceMain()
//
void Service::Start()
{
    HRESULT hr = S_OK;
    bool bUpdateDone = false;

    hr = UpdateServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    if (!m_LanDevice)
    {
        m_LanDevice = new LanDevice;
    }

    if (m_LanDevice)
    {
        hr = m_LanDevice->CheckAndUpdateProperty(bUpdateDone);
    }

    if (FAILED(hr) || bUpdateDone)
    {
        // Signal SCM to stop the NT service
        Stop();
    }
}

//
// Performs shutdown of the service, normally invoked by svchost via ServiceStopCallback()
//
void Service::Stop()
{
    if (m_LanDevice)
    {
        delete m_LanDevice;
    }

    (void)UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);

    // Do not read or write shared variables after the service enters stopped state since
    // that can race with a new service start and the service DLL may not be unloaded in
    // all cases (fast service restart, "ServiceDllUnloadOnStop" reg value set to 0, etc.)
}


//
// Handles system service control requests
//
DWORD Service::ServiceHandler(DWORD opcode, DWORD /*eventType*/, _In_ void* /*pEventData*/)
{
    DWORD returnStatus = NO_ERROR;

    switch (opcode)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
    {
        if (SUCCEEDED(UpdateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR)))
        {
            Stop();
        }
        else
        {
            returnStatus = ::GetLastError();
        }

    }
    break;

    case SERVICE_CONTROL_SESSIONCHANGE:
    case SERVICE_CONTROL_DEVICEEVENT:
    case SERVICE_CONTROL_POWEREVENT:
    {
        // We currently do not need to process these sevice control code.
    }
    break;

    default:
        break;
    }

    return returnStatus;
}

//
// Updates the state of the service with the system
//
HRESULT Service::UpdateServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint)
{
    HRESULT hr = E_UNEXPECTED;

    if (m_hServiceStatus != nullptr)
    {
        hr = S_OK;

        // Do not accept any controls until we are in SERVICE_RUNNING state (i.e., while starting or stopping).
        // It is a headache to get controls while we are not ready yet and can cause hangs since the control handler
        // can block on the lock held while we are starting up (for instance) and SCM doesn't expect the control handler
        // to block (see HandlerEx callback function documentation in MSDN).
        m_serviceStatus.dwControlsAccepted = (currentState != SERVICE_RUNNING) ? 0 : SERVICE_ACCEPT_STOP;
        m_serviceStatus.dwControlsAccepted |= SERVICE_ACCEPT_SESSIONCHANGE;
        m_serviceStatus.dwControlsAccepted |= SERVICE_ACCEPT_POWEREVENT;

        m_serviceStatus.dwCurrentState = currentState;
        m_serviceStatus.dwWin32ExitCode = win32ExitCode;
        m_serviceStatus.dwWaitHint = waitHint;

        if ((currentState == SERVICE_RUNNING) ||
            (currentState == SERVICE_STOPPED))
        {
            m_serviceStatus.dwCheckPoint = 0;
        }
        else
        {
            m_serviceStatus.dwCheckPoint++;
        }

        // Failed to set the service status, something went wrong.
        if (!::SetServiceStatus(m_hServiceStatus, &m_serviceStatus))
        {
            hr = HRESULT_FROM_WIN32(::GetLastError());
        }
    }

    return hr;
}

// exported functions, called by svchost
void STDAPICALLTYPE ServiceMain(DWORD argc, _In_ PTSTR argv[])
{
    g_service.ServiceMain(argc, argv);
}


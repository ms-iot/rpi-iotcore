//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Module Name:
//
//     service.h
//
// Abstract:
//
//     This file is header file defines the service interfaces. 
//

#pragma once

namespace RpiLanPropertyChange
{

    class Service final
    {
    public:
        Service();

        void ServiceMain(DWORD argc, _In_ PTSTR argv[]);

        void Start();
        void Stop();
        DWORD ServiceHandler(DWORD opcode, DWORD eventType, _In_ void* pEventData);

    private:
        HRESULT UpdateServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint = 10000);
        SERVICE_STATUS m_serviceStatus;
        SERVICE_STATUS_HANDLE m_hServiceStatus;

        class LanDevice * m_LanDevice;
    };

    class LanDevice final
    {
    public:
        LanDevice();
        ~LanDevice();

        HRESULT CheckAndUpdateProperty(bool & bUpdateDone);

    private:
        enum LanPropertyChangeStatus
        {
            LanPropertyNoChange = 0,
            LanPropertyNeedUpdate,
            LanPropertyUpdated
        };

        DEVINST m_Devinst;
        CONFIGRET FindDeviceInstance();
        CONFIGRET ApllyLanPropertyChange();
        LanPropertyChangeStatus LanPropertyChange();
        CONFIGRET LanPropertyChangeDone();
    };
}

extern "C"
{
    // public callable by svchost
    void STDAPICALLTYPE ServiceMain(DWORD argc, _In_ PTSTR argv[]);
} // extern "C"


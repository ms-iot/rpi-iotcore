//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     RpiLanPropertyChange.cpp
//
// Abstract:
//
//     This file defines the device handle interfaces. 
//

#include "stdafx.h"
#include "service.h"
#include <tchar.h>
#include <strsafe.h>
#include <stdio.h>

#include <initguid.h>
#include <devpkey.h>
#include <devpropdef.h>

using namespace RpiLanPropertyChange;

//
// LanDevice creator
//
LanDevice::LanDevice() :
    m_Devinst(0)
{
    this->FindDeviceInstance();
}

LanDevice::~LanDevice()
{
    m_Devinst = 0;
}

//
// Check and update registry status, restart Lan device
//
HRESULT LanDevice::CheckAndUpdateProperty(bool & bUpdateDone)
{
    HRESULT hr = S_OK;

    bUpdateDone = true;

    if (!m_Devinst)
    {
        hr = HRESULT_FROM_WIN32(CR_INVALID_DEVINST);
        return hr;
    }

    LanPropertyChangeStatus LanStatus;

    LanStatus = this->LanPropertyChange();

    switch (LanStatus)
    {
    case LanPropertyNoChange:
        // Start timer to check change if service started before NDIS interface ready
        // Currently we cannot get this branch
        // Set update done here
        bUpdateDone = true;
        break;
    case LanPropertyNeedUpdate:
        this->ApllyLanPropertyChange();
        this->LanPropertyChangeDone();

        // Fall through
        // break;        
    case LanPropertyUpdated:
        bUpdateDone = true;
        break;
    }

    return hr;
}

//
// Read Registry value
//
LanDevice::LanPropertyChangeStatus LanDevice::LanPropertyChange()
{
    LanPropertyChangeStatus LanStatus = LanPropertyUpdated;
    CONFIGRET cr = CR_SUCCESS;
    HKEY hKey = 0;
    WCHAR pSubKey[] = L"PropertyChangeStatus";
    DWORD type, data, datasize, ret;

    if (!m_Devinst)
    {
        LanStatus = LanPropertyNoChange;
        goto Exit;
    }


    // Open "Software" registry key
    cr = CM_Open_DevNode_Key(m_Devinst,
        KEY_QUERY_VALUE,
        CM_REGISTRY_SOFTWARE,
        RegDisposition_OpenExisting,
        &hKey,
        CM_REGISTRY_SOFTWARE);
    if (cr != CR_SUCCESS)
    {
        LanStatus = LanPropertyNoChange;
        goto Exit;
    }

    //  Query value 
    ret = RegQueryValueEx(hKey, pSubKey, NULL, &type, (BYTE *)&data, &datasize);
    if (ret == ERROR_FILE_NOT_FOUND)
    {
        LanStatus = LanPropertyNoChange;
        goto Exit;
    }

    LanStatus = (LanPropertyChangeStatus)data;

Exit:
    if(hKey)
    { 
        RegCloseKey(hKey);
    }

    return LanStatus;
}

//
// Update registry value
//
CONFIGRET LanDevice::LanPropertyChangeDone()
{
    CONFIGRET cr = CR_SUCCESS;
    HKEY hKey = 0;
    WCHAR pSubKey[] = L"PropertyChangeStatus";
    DWORD data, ret;

    if (!m_Devinst)
    {
        cr = CR_NO_SUCH_DEVINST;
        goto Exit;
    }

    // Open "Software" registry key
    cr = CM_Open_DevNode_Key(m_Devinst,
        KEY_SET_VALUE,
        CM_REGISTRY_SOFTWARE,
        RegDisposition_OpenExisting,
        &hKey,
        CM_REGISTRY_SOFTWARE);
    if (cr != CR_SUCCESS)
    {
        cr = CR_REGISTRY_ERROR;
        goto Exit;
    }

    //  Set value 
    data = (DWORD)LanPropertyUpdated;
    ret = RegSetValueEx(hKey, pSubKey, NULL, REG_DWORD, (BYTE *)&data, sizeof(DWORD));
    if (ret != ERROR_SUCCESS)
    {
        cr = CR_REGISTRY_ERROR;
        goto Exit;
    }


Exit:
    if (hKey)
    {
        RegCloseKey(hKey);
    }

    return cr;
}

//
// Find Lan device
//
CONFIGRET LanDevice::FindDeviceInstance()
{
    CONFIGRET cr = CR_SUCCESS;
    PWSTR DeviceList = NULL;
    ULONG DeviceListLength = 0;
    PWSTR CurrentDevice;
    WCHAR DeviceDesc[2048];
    DEVPROPTYPE PropertyType;
    DEVINST Devinst;
    ULONG PropertySize;
    DWORD Index = 0;
    WCHAR DescLAN7800[] = L"LAN7800 USB 3.0 to Ethernet 10/100/1000 Adapter";
    WCHAR DescLAN951x[] = L"LAN9512/LAN9514 USB 2.0 to Ethernet 10/100 Adapter";

    cr = CM_Get_Device_ID_List_Size(&DeviceListLength,
        NULL,
        CM_GETIDLIST_FILTER_PRESENT);

    if (cr != CR_SUCCESS)
    {
        goto Exit;
    }

    DeviceList = (PWSTR)HeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        DeviceListLength * sizeof(WCHAR));

    if (DeviceList == NULL) {
        goto Exit;
    }

    cr = CM_Get_Device_ID_List(NULL,
        DeviceList,
        DeviceListLength,
        CM_GETIDLIST_FILTER_PRESENT);

    if (cr != CR_SUCCESS)
    {
        goto Exit;
    }

    // Iterates all devices looking for NDIS devices we want
    for (CurrentDevice = DeviceList;
        *CurrentDevice;
        CurrentDevice += wcslen(CurrentDevice) + 1)
    {
        cr = CM_Locate_DevNode(&Devinst,
            CurrentDevice,
            CM_LOCATE_DEVNODE_NORMAL);

        if (cr != CR_SUCCESS)
        {
            goto Exit;
        }

        // Query a property on the device. the device description.
        PropertySize = sizeof(DeviceDesc);

        cr = CM_Get_DevNode_Property(Devinst,
            &DEVPKEY_Device_DeviceDesc,
            &PropertyType,
            (PBYTE)DeviceDesc,
            &PropertySize,
            0);
        if (cr != CR_SUCCESS)
        {
            Index++;
            continue;
        }

        if (PropertyType != DEVPROP_TYPE_STRING)
        {
            Index++;
            continue;
        }

        // Match device descriptor one by one
        // As there is only 1 NDIS ethernet with the name in one device, there will be no conflict or problem 
        // to exit iteration when finding a match
        // There are 2 devices need to be checked, if more devices to be checked, better to change to a list
        if (_wcsicmp(DescLAN7800, DeviceDesc) == 0)
        {
            m_Devinst = Devinst;
            break;
        }

        if (_wcsicmp(DescLAN951x, DeviceDesc) == 0)
        {
            m_Devinst = Devinst;
            break;
        }

        Index++;
    }


Exit:
    if (DeviceList != NULL)
    {
        HeapFree(GetProcessHeap(),
            0,
            DeviceList);
    }

    if (!m_Devinst)
    {
        cr = CR_NO_SUCH_DEVINST;
    }

    return cr;
}

//
// Restart Lan device
//
CONFIGRET LanDevice::ApllyLanPropertyChange()
{
    CONFIGRET cr = CR_SUCCESS;

    if (!m_Devinst)
    {
        cr = CR_NO_SUCH_DEVINST;
        goto Exit;
    }

    // Restart device
    cr = CM_Query_And_Remove_SubTree(m_Devinst,
        NULL,
        NULL,
        0,
        CM_REMOVE_NO_RESTART);
    if (cr != CR_SUCCESS)
    {
        goto Exit;
    }

    cr = CM_Setup_DevNode(m_Devinst,
        CM_SETUP_DEVNODE_READY);
    if (cr != CR_SUCCESS) {
        goto Exit;
    }


Exit:
    return cr;
}



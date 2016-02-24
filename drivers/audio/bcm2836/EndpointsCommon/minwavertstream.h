/*++

Copyright (c) 1997-2010  Microsoft Corporation All Rights Reserved

Abstract:
    Definition of wavert stream class.

--*/

#pragma once

//
// These have to be constants to get the performance of the conversion good enough.
// Using static const members will make the audio quality unusable.
//

#define PWMRANGE  2268
#define PCMRANGE 0x10000
#define PCMTOPWMDIV ((PCMRANGE / PWMRANGE) + 1)
#define PWMSILENCE  (PWMRANGE / 2)
#define PCMBYTESPERSAMPLE  2
#define PCMFREQ 44100
#define PWMFREQ 100000000

//=============================================================================
// Referenced Forward
//=============================================================================
class CMiniportWaveRT;
typedef CMiniportWaveRT *PCMiniportWaveRT;

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveRTStream 
// 
class CMiniportWaveRTStream :
        public IMiniportWaveRTStreamNotification,
        public IMiniportWaveRTOutputStream,
        public CUnknown
{
protected:
    PPORTWAVERTSTREAM           m_pPortStream;

public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTStream);
    ~CMiniportWaveRTStream();

    IMP_IMiniportWaveRTStream;
    IMP_IMiniportWaveRTStreamNotification;
    IMP_IMiniportWaveRTOutputStream;
    IMP_IMiniportWaveRT;

    NTSTATUS                    Init
        (
        _In_  PCMiniportWaveRT    Miniport,
        _In_  PPORTWAVERTSTREAM   Stream,
        _In_  ULONG               Channel,
        _In_  BOOLEAN             Capture,
        _In_  PKSDATAFORMAT       DataFormat,
        _In_  GUID                SignalProcessingMode
        );

    // Friends
    friend class                CMiniportWaveRT;

protected:
    CMiniportWaveRT*            m_pMiniport;
    ULONG                       m_ulPin;
    BOOLEAN                     m_bUnregisterStream;
    ULONG                       m_ulDmaBufferSize;
    ULONG                       m_ulBytesPerPacket;
    BYTE*                       m_DataBuffer;
    ULONG                       m_ulPwmDmaBufferSize;
    KSSTATE                     m_PwmState;
    BOOLEAN                     m_PwmInitialized;
    LIST_ENTRY                  m_NotificationList;
    ULONG                       m_RestartPacketNumber;
    BOOLEAN                     m_RestartInProgress;
    BCM_PWM_AUDIO_CONFIG        m_PwmAudioConfig;
    ULONG                       m_ulNotificationsPerBuffer;
    LARGE_INTEGER               m_LastSetWritePacket;
    LARGE_INTEGER               m_PerformanceCounterFrequency;

    ULONG                       m_ulSamplesPerPacket;
    ULONG                       m_ulPacketsTransferred;

    KSSTATE                     m_KsState;
    PRKDPC                      m_pDpc;
    ULONGLONG                   m_ullPlayPosition;
    LARGE_INTEGER               m_PlayQpcTime;
    PWAVEFORMATEXTENSIBLE       m_pWfExt;

    GUID                        m_SignalProcessingMode;

    public:

    GUID GetSignalProcessingMode()
    {
        return m_SignalProcessingMode;
    }

private:

    //
    // List for notification events
    //

    typedef struct _NOTIFICATION_LIST_ENTRY
    {
        LIST_ENTRY  ListEntry;
        PKEVENT     NotificationEvent;
    } NOTIFICATION_LIST_ENTRY, *PNOTIFICATION_LIST_ENTRY;

    VOID UpdatePosition
    (
        VOID
    );

    NTSTATUS PwmIoctlCall
    (
        _In_                                ULONG   IoctlCode,
        _In_reads_opt_(InputBufferSize)     PVOID   InputBuffer,
        _In_                                ULONG   InputBufferSize,
        _Out_writes_opt_(OutputBufferSize)  PVOID   OutputBuffer,
        _In_                                ULONG   OutputBufferSize
    );

    VOID ConvertPCMToPWM
    (
        _In_reads_(SampleCount)         PINT16  InBuffer,
        _Out_writes_all_(SampleCount)   PUINT32 OutBuffer,
        _In_                            DWORD   SampleCount
    );

    VOID SilenceToPWM
    (
        _Out_writes_all_(SampleCount)   PUINT32 OutBuffer,
        _In_                            DWORD   SampleCount
    );

    VOID AddPacketToDma
    (
        _In_ ULONG PacketNumber
    );

    VOID RequestNextPacket
    (
        VOID
    );

};

typedef CMiniportWaveRTStream *PCMiniportWaveRTStream;


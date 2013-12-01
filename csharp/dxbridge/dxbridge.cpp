#include "stdafx.h"
#include "dxbridge.h"

#pragma comment(lib, "dsound.lib")

// The buffer size must be a power of 2 and must be less than INT_MAX.
// 1 megabyte gives nearly 6 seconds, which is plenty.
#define DXBRIDGE_BUFFER_SIZE (1024 * 1024) 

static LPDIRECTSOUND8 ds8; 
static LPDIRECTSOUNDBUFFER8 dsb8;
static unsigned cleared;

// Initialize audio playback
DXBRIDGE_API int dxbridgeInit(HWND hwnd) {
  HRESULT hr;
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee416365(v=vs.85).aspx
  hr = DirectSoundCreate8(NULL, &ds8, NULL);
  if(!SUCCEEDED(hr)) return hr;
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee416970(v=vs.85).aspx
  hr = ds8->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
  if(!SUCCEEDED(hr)) return hr;
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee416361(v=vs.85).aspx
  WAVEFORMATEX wfx; // http://msdn.microsoft.com/en-us/library/aa908934.aspx
  memset(&wfx, 0, sizeof wfx);
  wfx.wFormatTag = WAVE_FORMAT_PCM; 
  wfx.nChannels = 2; 
  wfx.nSamplesPerSec = 44100; 
  wfx.wBitsPerSample = 16; 
  wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / CHAR_BIT;
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; 
  DSBUFFERDESC dsbdesc; // http://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.dsbufferdesc(v=vs.85).aspx
  memset(&dsbdesc, 0, sizeof dsbdesc); 
  dsbdesc.dwSize = sizeof dsbdesc; 
  dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS; //DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY
  dsbdesc.dwBufferBytes = DXBRIDGE_BUFFER_SIZE; 
  dsbdesc.lpwfxFormat = &wfx; 
  LPDIRECTSOUNDBUFFER dsb; 
  hr = ds8->CreateSoundBuffer(&dsbdesc, &dsb, NULL);
  if(!SUCCEEDED(hr)) return hr;
  hr = dsb->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*)&dsb8); // http://msdn.microsoft.com/en-us/library/windows/desktop/ms682521(v=vs.85).aspx
  dsb->Release();
  return hr;
}

// Add some audio to the buffer
//
// offset is a 32-bit playback offset determining where in the buffer the
// sample data will go.
//
// data/bytes is the chunk of sample data to play at that offset.
//
// Data need not be buffered in strict order.  The buffer is constrained to a
// power of 2 size so that samples will wrap properly, but samples from "too far"
// in the future will be played at the wrong time.
DXBRIDGE_API int dxbridgeBuffer(unsigned offset, const char *data, size_t bytes) {
  HRESULT hr;
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee419014(v=vs.85).aspx
  LPVOID a1, a2;
  DWORD n1, n2;
  offset %= DXBRIDGE_BUFFER_SIZE;
  // If the offset is too far ahead, block
  DWORD play, write;
  for(;;) {
    hr = dsb8->GetCurrentPosition(&play, &write);
    if(!SUCCEEDED(hr)) return -1;
    unsigned delta = (offset - play) % DXBRIDGE_BUFFER_SIZE;
    if(delta < DXBRIDGE_BUFFER_SIZE / 2)
      break;
    Sleep(1/*ms*/);
  }
  if(cleared != play) {
    // Clear the buffer region most recently played
    hr = dsb8->Lock(cleared, (play - cleared) % DXBRIDGE_BUFFER_SIZE, &a1, &n1, &a2, &n2, 0); 
    if(hr == DSERR_BUFFERLOST) {
      dsb8->Restore();
      hr = dsb8->Lock(cleared, (play - cleared) % DXBRIDGE_BUFFER_SIZE, &a1, &n1, &a2, &n2, 0);
    }
    if(!SUCCEEDED(hr)) return hr;
    memset(a1, 0, n1);
    memset(a2, 0, n2);
    hr = dsb8->Unlock(a1, n1, a2, n2);
    if(!SUCCEEDED(hr)) return hr;
    cleared = play;
  }
  // Install fresh data
  // http://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.idirectsoundbuffer8.idirectsoundbuffer8.lock(v=vs.85).aspx
  hr = dsb8->Lock(offset, bytes, &a1, &n1, &a2, &n2, 0); 
  if(hr == DSERR_BUFFERLOST) {
    dsb8->Restore();
    hr = dsb8->Lock(offset, bytes, &a1, &n1, &a2, &n2, 0);
  }
  if(!SUCCEEDED(hr)) return hr;
  memcpy(a1, data, n1);
  data += n1;
  bytes -= n1;
  memcpy(a2, data, n2);
  data += n2;
  hr = dsb8->Unlock(a1, n1, a2, n2);
  return hr;
}

// Start playing
DXBRIDGE_API int dxbridgePlay() {
  HRESULT hr;
  hr = dsb8->Play(0, 0, DSBPLAY_LOOPING);
  cleared = 0;
  return hr;
}

// Stop playing
DXBRIDGE_API int dxbridgeStop() {
  HRESULT hr;
  hr = dsb8->Stop();
  return hr;
}

// Calculate how far ahead a given offset is
//
// offset is compared to the current play cursor (the most recent estimate of the sound
// being played right now).  Working on the assumption that it is ahead of the play cursor
// but not multiple buffers ahead of it, the distance from the play cursor to the offset
// is returned.
//
// Returns -1 on error.
DXBRIDGE_API int dxbridgeAhead(unsigned offset) {
  HRESULT hr;
  DWORD play, write;
  hr = dsb8->GetCurrentPosition(&play, &write);
  if(!SUCCEEDED(hr)) return -1;
  offset %= DXBRIDGE_BUFFER_SIZE;
  return offset > play ? offset - play : offset + DXBRIDGE_BUFFER_SIZE - play;
}
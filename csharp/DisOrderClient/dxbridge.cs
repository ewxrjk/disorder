using System;
using System.Runtime.InteropServices;

namespace DisOrderClient
{
  static class dxbridge
  {
    [DllImport("dxbridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public unsafe static extern int dxbridgeInit(int hwnd);
    [DllImport("dxbridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public unsafe static extern int dxbridgeBuffer(uint offset, IntPtr data, IntPtr bytes);
    [DllImport("dxbridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public unsafe static extern int dxbridgePlay();
    [DllImport("dxbridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public unsafe static extern int dxbridgeStop();
    [DllImport("dxbridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public unsafe static extern int dxbridgeAhead(uint offset);
  }
}

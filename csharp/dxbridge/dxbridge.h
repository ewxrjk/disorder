#ifdef DXBRIDGE_EXPORTS
#define DXBRIDGE_API __declspec(dllexport)
#else
#define DXBRIDGE_API __declspec(dllimport)
#endif

extern "C" {

DXBRIDGE_API int dxbridgeInit(HWND hwnd);
DXBRIDGE_API int dxbridgeBuffer(unsigned offset, const char *data, size_t bytes);
DXBRIDGE_API int dxbridgePlay();
DXBRIDGE_API int dxbridgeStop();
DXBRIDGE_API int dxbridgeAhead(unsigned offset);

}

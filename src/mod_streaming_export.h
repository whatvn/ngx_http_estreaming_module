#ifndef MOD_STREAMING_EXPORT_H_AKW
#define MOD_STREAMING_EXPORT_H_AKW

#if __GNUC__ >= 4
#define MOD_STREAMING_DLL_IMPORT __attribute__ ((visibility("default")))
#define MOD_STREAMING_DLL_EXPORT __attribute__ ((visibility("default")))
#define MOD_STREAMING_DLL_LOCAL  __attribute__ ((visibility("hidden")))
#else
#define MOD_STREAMING_DLL_IMPORT
#define MOD_STREAMING_DLL_EXPORT
#define MOD_STREAMING_DLL_LOCAL
#endif

#define X_MOD_HLS_KEY "mod_hls"
#define X_MOD_HLS_VERSION "version=0.9"

#endif // MOD_STREAMING_EXPORT_H_AKW

// End Of File
#ifdef SPEECHRECOGNIZERXUNFEI_EXPORTS
#define SR_API  __declspec(dllexport) 
#else
#define SR_API  __declspec(dllimport) 
#endif

#ifdef __cplusplus
extern "C" {
#endif
	SR_API int InitSR();
	SR_API int GetText(LPSTR text,int maxLen);
	SR_API int DisposeSR();

#ifdef __cplusplus
} /* extern "C" */
#endif /* C++ */
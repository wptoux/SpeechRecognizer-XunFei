/*
* 语音听写(iFly Auto Transform)技术能够实时地将语音转换成对应的文字。
*/

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <conio.h>
#include <errno.h>
#include <process.h>

#include "../XunFeiApis/include/msp_cmn.h"
#include "../XunFeiApis/include/msp_errors.h"

#include "iat_record.h"
#include "speech_recognizer.h"


#define FRAME_LEN	640 
#define	BUFFER_SIZE	4096

enum {
	EVT_START = 0,
	EVT_STOP,
	EVT_QUIT,
	EVT_TOTAL
};
static HANDLE events[EVT_TOTAL] = { NULL,NULL,NULL };

static COORD begin_pos = { 0, 0 };
static COORD last_pos = { 0, 0 };

/* 上传用户词表 */
static int upload_userwords()
{
	char*			userwords = NULL;
	size_t			len = 0;
	size_t			read_len = 0;
	FILE*			fp = NULL;
	int				ret = -1;

	fp = fopen("userwords.json", "rb");
	if (NULL == fp)
	{
		fprintf(stderr,"\nError:open [userwords.json] failed! \n");
		goto upload_exit;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp); //获取文件大小
	fseek(fp, 0, SEEK_SET);

	userwords = (char*)malloc(len + 1);
	if (NULL == userwords)
	{
		fprintf(stderr,"\nError:out of memory! \n");
		goto upload_exit;
	}

	read_len = fread((void*)userwords, 1, len, fp); //读取用户词表内容
	if (read_len != len)
	{
		fprintf(stderr,"\nError:read [userwords.json] failed!\n");
		goto upload_exit;
	}
	userwords[len] = '\0';

	MSPUploadData("userwords", userwords, len, "sub = uup, dtt = userword", &ret); //上传用户词表
	if (MSP_SUCCESS != ret)
	{
		fprintf(stderr,"\nError:MSPUploadData failed ! errorCode: %d \n", ret);
		goto upload_exit;
	}

upload_exit:
	if (NULL != fp)
	{
		fclose(fp);
		fp = NULL;
	}
	if (NULL != userwords)
	{
		free(userwords);
		userwords = NULL;
	}

	return ret;
}

static char *g_result = NULL;
static BOOL ready = FALSE;
static BOOL hasResult = FALSE;
static unsigned int g_buffersize = BUFFER_SIZE;

void on_result(const char *result, char is_last)
{
	hasResult = TRUE;
	if (result) {
		size_t left = g_buffersize - 1 - strlen(g_result);
		size_t size = strlen(result);
		if (left < size) {
			g_result = (char*)realloc(g_result, g_buffersize + BUFFER_SIZE);
			if (g_result)
				g_buffersize += BUFFER_SIZE;
			else {
				fprintf(stderr,"Error:mem alloc failed\n");
				return;
			}
		}
		strncat(g_result, result, size);
		if (is_last) {
			ready = TRUE;
		}
	}
}
void on_speech_begin()
{
	if (g_result)
	{
		free(g_result);
	}
	g_result = (char*)malloc(BUFFER_SIZE);
	g_buffersize = BUFFER_SIZE;
	memset(g_result, 0, g_buffersize);

	fprintf(stderr,"Info:Start Listening...\n");
	ready = FALSE;
	hasResult = FALSE;
}
void on_speech_end(int reason)
{
	if (reason == END_REASON_VAD_DETECT)
		fprintf(stderr,"Info:Speaking done\n");
	else
		fprintf(stderr,"Error:Recognizer error %d\n", reason);
}

struct speech_rec iat;
SR_API int InitSR()
{
	char* appdata = getenv("LOCALAPPDATA");
	char path[512];
	ZeroMemory(path, 512 * sizeof(char));
	strcat(path, appdata);
	strcat(path, "\\MetroRobot\\Robot\\SpeechRecognition.log");
	freopen(path, "a", stderr);
	setbuf(stderr, NULL);

	int			ret = MSP_SUCCESS;
	int			upload_on = 1; //是否上传用户词表
	const char* login_params = "appid = 57b2f865, work_dir = ."; // 登录参数，appid与msc库绑定,请勿随意改动
	int aud_src = 0;

	/*
	* sub:				请求业务类型
	* domain:			领域
	* language:			语言
	* accent:			方言
	* sample_rate:		音频采样率
	* result_type:		识别结果格式
	* result_encoding:	结果编码格式
	*
	* 详细参数说明请参阅《iFlytek MSC Reference Manual》
	*/
	const char* session_begin_params = "sub = iat, domain = iat, language = zh_cn, accent = mandarin, sample_rate = 44000, result_type = plain, result_encoding = gb2312";

	/* 用户登录 */
	ret = MSPLogin(NULL, NULL, login_params); //第一个参数是用户名，第二个参数是密码，均传NULL即可，第三个参数是登录参数	
	if (MSP_SUCCESS != ret) {
		fprintf(stderr,"Error:MSPLogin failed , Error code %d.\n", ret);
		return -1; //登录失败，退出登录
	}

	if (upload_on)
	{
		fprintf(stderr,"Info:上传用户词表 ...\n");
		ret = upload_userwords();
		if (MSP_SUCCESS != ret)
		{
			MSPLogout();
			return -1;
		}
		fprintf(stderr,"Info:上传用户词表成功\n");
	}

	int errcode;
	int i = 0;
	HANDLE helper_thread = NULL;

	DWORD waitres;
	char isquit = 0;

	struct speech_rec_notifier recnotifier = {
		on_result,
		on_speech_begin,
		on_speech_end
	};

	errcode = sr_init(&iat, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier);
	if (errcode) {
		fprintf(stderr,"Error:speech recognizer init failed\n");
		return -1;
	}

	return 0;
}

SR_API int GetText(LPSTR text,int maxLen)
{
	int errcode;
	errcode = sr_start_listening(&iat);
	
	if (errcode > 0) {
		fprintf(stderr, "Error:start listening failed %d\n", errcode);
		return -1;
	}
	if (errcode < 0) {
		fprintf(stderr, "Warn:start listening exception %d, restaring\n", errcode);
		sr_stop_listening(&iat);
		errcode = sr_start_listening(&iat);

		if (errcode != 0) {
			fprintf(stderr, "Error:start listening failed %d\n", errcode);
			return -1;
		}
	}

	int timeout = 150; // 150 times 100ms = 15s
	while (!ready && timeout-- != 0) {
		Sleep(100);
	}

	if (timeout == 0) {
		sr_stop_listening(&iat);
	}

	timeout = 10;
	while (!ready && timeout-- != 0) {
		Sleep(100);
	}

	if (hasResult) {
		int n = strlen(g_result);

		if (n > maxLen) {
			n = maxLen - 1;
		}

		strncpy(text,g_result,n);
		text[n] = '\0';
		fprintf(stderr,"Info:Result is %s\n", text);
		return n;
	}
	else {
		return 0;
	}
}

SR_API int DisposeSR()
{
	sr_stop_listening(&iat);
	sr_uninit(&iat);
	MSPLogout();
}

#include "ffmpeg_encoder.h"

#include <jni.h>
#include <string>
#include <chrono>
extern "C" {
#include <libavcodec/jni.h>
}
#include "my_log.h"

#define FF_LOG_TAG     "FFmpeg"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  FF_LOG_TAG, __VA_ARGS__)

static void log_callback_test(void *ptr, int level, const char *fmt, va_list vl) {
    va_list vl2;
    char *line = (char *)malloc(128 * sizeof(char));
    static int print_prefix = 1;
    va_copy(vl2, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, 128, &print_prefix);
    va_end(vl2);
    line[127] = '\0';
    LOGI("%s", line);
    free(line);
}

int my_main(const char* prefix_path) {
    const std::string output_file = prefix_path + std::string("/output.mp4");
    std::vector<std::string> input_images = {
        "images/00001-capture.jpg",
        "images/00002-capture.jpg",
        "images/00003-capture.jpg",
        "images/00004-capture.jpg",
        "images/00005-capture.jpg",
        "images/00006-capture.jpg",
        "images/00007-capture.jpg",
        "images/00008-capture.jpg",
        "images/00009-capture.jpg",
        "images/00010-capture.jpg",
        "images/00011-capture.jpg",
        "images/00012-capture.jpg",
        "images/00013-capture.jpg",
        "images/00014-capture.jpg",
        "images/00015-capture.jpg",
        "images/00016-capture.jpg",
        "images/00017-capture.jpg",
        "images/00018-capture.jpg",
        "images/00019-capture.jpg",
        "images/00020-capture.jpg",
        "images/00021-capture.jpg",
        "images/00022-capture.jpg",
        "images/00023-capture.jpg",
        "images/00024-capture.jpg",
        "images/00025-capture.jpg",
        "images/00026-capture.jpg",
        "images/00027-capture.jpg",
        "images/00028-capture.jpg",
        "images/00029-capture.jpg",
        "images/00030-capture.jpg",
        "images/00031-capture.jpg",
        "images/00032-capture.jpg",
        "images/00033-capture.jpg",
        "images/00034-capture.jpg",
        "images/00035-capture.jpg",
        "images/00036-capture.jpg",
        "images/00037-capture.jpg",
        "images/00038-capture.jpg",
        "images/00039-capture.jpg",
        "images/00040-capture.jpg",
        "images/00041-capture.jpg",
        "images/00042-capture.jpg",
        "images/00043-capture.jpg",
        "images/00044-capture.jpg",
        "images/00045-capture.jpg",
        "images/00046-capture.jpg",
        "images/00047-capture.jpg",
        "images/00048-capture.jpg",
        "images/00049-capture.jpg",
        "images/00050-capture.jpg",
        "images/00051-capture.jpg",
        "images/00052-capture.jpg",
        "images/00053-capture.jpg",
        "images/00054-capture.jpg",
        "images/00055-capture.jpg",
        "images/00056-capture.jpg",
        "images/00057-capture.jpg",
        "images/00058-capture.jpg",
        "images/00059-capture.jpg",
        "images/00060-capture.jpg",
        "images/00061-capture.jpg",
        "images/00062-capture.jpg",
        "images/00063-capture.jpg",
        "images/00064-capture.jpg",
        "images/00065-capture.jpg",
        "images/00066-capture.jpg",
        "images/00067-capture.jpg",
        "images/00068-capture.jpg",
        "images/00069-capture.jpg",
        "images/00070-capture.jpg",
        "images/00071-capture.jpg",
        "images/00072-capture.jpg",
        "images/00073-capture.jpg",
        "images/00074-capture.jpg",
        "images/00075-capture.jpg",
        "images/00076-capture.jpg",
        "images/00077-capture.jpg",
        "images/00078-capture.jpg",
        "images/00079-capture.jpg",
        "images/00080-capture.jpg",
        "images/00081-capture.jpg",
        "images/00082-capture.jpg",
        "images/00083-capture.jpg",
        "images/00084-capture.jpg",
        "images/00085-capture.jpg",
        "images/00086-capture.jpg",
        "images/00087-capture.jpg",
        "images/00088-capture.jpg",
        "images/00089-capture.jpg",
        "images/00090-capture.jpg",
        "images/00091-capture.jpg",
        "images/00092-capture.jpg",
        "images/00093-capture.jpg",
        "images/00094-capture.jpg",
        "images/00095-capture.jpg",
        "images/00096-capture.jpg",
        "images/00097-capture.jpg",
        "images/00098-capture.jpg",
        "images/00099-capture.jpg",
        "images/00100-capture.jpg",
        "images/00101-capture.jpg",
        "images/00102-capture.jpg",
        "images/00103-capture.jpg",
        "images/00104-capture.jpg",
        "images/00105-capture.jpg",
        "images/00106-capture.jpg",
        "images/00107-capture.jpg",
        "images/00108-capture.jpg",
        "images/00109-capture.jpg",
        "images/00110-capture.jpg",
        "images/00111-capture.jpg",
        "images/00112-capture.jpg",
        "images/00113-capture.jpg",
        "images/00114-capture.jpg",
        "images/00115-capture.jpg",
        "images/00116-capture.jpg",
        "images/00117-capture.jpg",
        "images/00118-capture.jpg",
        "images/00119-capture.jpg",
        "images/00120-capture.jpg",
        "images/00121-capture.jpg",
        "images/00122-capture.jpg",
        "images/00123-capture.jpg",
        "images/00124-capture.jpg",
        "images/00125-capture.jpg",
        "images/00126-capture.jpg",
        "images/00127-capture.jpg",
        "images/00128-capture.jpg",
        "images/00129-capture.jpg",
        "images/00130-capture.jpg",
        "images/00131-capture.jpg",
        "images/00132-capture.jpg",
        "images/00133-capture.jpg",
        "images/00134-capture.jpg",
        "images/00135-capture.jpg",
        "images/00136-capture.jpg",
        "images/00137-capture.jpg",
        "images/00138-capture.jpg",
        "images/00139-capture.jpg",
        "images/00140-capture.jpg",
        "images/00141-capture.jpg",
        "images/00142-capture.jpg",
        "images/00143-capture.jpg",
        "images/00144-capture.jpg",
        "images/00145-capture.jpg",
        "images/00146-capture.jpg",
        "images/00147-capture.jpg",
        "images/00148-capture.jpg",
        "images/00149-capture.jpg",
        "images/00150-capture.jpg",
        "images/00151-capture.jpg",
        "images/00152-capture.jpg",
        "images/00153-capture.jpg",
        "images/00154-capture.jpg",
        "images/00155-capture.jpg",
        "images/00156-capture.jpg",
        "images/00157-capture.jpg",
        "images/00158-capture.jpg",
        "images/00159-capture.jpg",
        "images/00160-capture.jpg",
        "images/00161-capture.jpg",
        "images/00162-capture.jpg",
        "images/00163-capture.jpg",
        "images/00164-capture.jpg",
        "images/00165-capture.jpg",
        "images/00166-capture.jpg",
        "images/00167-capture.jpg",
        "images/00168-capture.jpg",
        "images/00169-capture.jpg",
        "images/00170-capture.jpg",
        "images/00171-capture.jpg",
        "images/00172-capture.jpg",
        "images/00173-capture.jpg",
        "images/00174-capture.jpg",
        "images/00175-capture.jpg",
        "images/00176-capture.jpg",
        "images/00177-capture.jpg",
        "images/00178-capture.jpg",
        "images/00179-capture.jpg",
        "images/00180-capture.jpg",
        "images/00181-capture.jpg",
        "images/00182-capture.jpg",
        "images/00183-capture.jpg",
        "images/00184-capture.jpg",
        "images/00185-capture.jpg",
        "images/00186-capture.jpg",
        "images/00187-capture.jpg",
        "images/00188-capture.jpg",
        "images/00189-capture.jpg",
        "images/00190-capture.jpg",
        "images/00191-capture.jpg",
        "images/00192-capture.jpg",
        "images/00193-capture.jpg",
        "images/00194-capture.jpg",
        "images/00195-capture.jpg",
        "images/00196-capture.jpg",
        "images/00197-capture.jpg",
        "images/00198-capture.jpg",
        "images/00199-capture.jpg",
        "images/00200-capture.jpg",
        "images/00201-capture.jpg",
        "images/00202-capture.jpg",
        "images/00203-capture.jpg",
        "images/00204-capture.jpg",
        "images/00205-capture.jpg",
        "images/00206-capture.jpg",
        "images/00207-capture.jpg",
        "images/00208-capture.jpg",
        "images/00209-capture.jpg",
        "images/00210-capture.jpg",
        "images/00211-capture.jpg",
        "images/00212-capture.jpg",
        "images/00213-capture.jpg",
        "images/00214-capture.jpg",
        "images/00215-capture.jpg",
        "images/00216-capture.jpg",
        "images/00217-capture.jpg",
        "images/00218-capture.jpg",
        "images/00219-capture.jpg",
        "images/00220-capture.jpg",
        "images/00221-capture.jpg",
        "images/00222-capture.jpg",
        "images/00223-capture.jpg",
        "images/00224-capture.jpg",
        "images/00225-capture.jpg",
        "images/00226-capture.jpg",
        "images/00227-capture.jpg",
        "images/00228-capture.jpg",
        "images/00229-capture.jpg",
        "images/00230-capture.jpg",
        "images/00231-capture.jpg",
        "images/00232-capture.jpg",
        "images/00233-capture.jpg",
        "images/00234-capture.jpg",
        "images/00235-capture.jpg",
        "images/00236-capture.jpg",
        "images/00237-capture.jpg",
        "images/00238-capture.jpg",
        "images/00239-capture.jpg",
        "images/00240-capture.jpg",
        "images/00241-capture.jpg",
        "images/00242-capture.jpg",
        "images/00243-capture.jpg",
        "images/00244-capture.jpg",
        "images/00245-capture.jpg",
        "images/00246-capture.jpg",
        "images/00247-capture.jpg",
        "images/00248-capture.jpg",
        "images/00249-capture.jpg",
        "images/00250-capture.jpg",
        "images/00251-capture.jpg",
        "images/00252-capture.jpg",
        "images/00253-capture.jpg",
        "images/00254-capture.jpg",
        "images/00255-capture.jpg",
        "images/00256-capture.jpg",
        "images/00257-capture.jpg",
        "images/00258-capture.jpg",
        "images/00259-capture.jpg",
        "images/00260-capture.jpg",
        "images/00261-capture.jpg",
        "images/00262-capture.jpg",
        "images/00263-capture.jpg",
        "images/00264-capture.jpg",
        "images/00265-capture.jpg",
        "images/00266-capture.jpg",
        "images/00267-capture.jpg",
        "images/00268-capture.jpg",
        "images/00269-capture.jpg",
        "images/00270-capture.jpg",
        "images/00271-capture.jpg",
        "images/00272-capture.jpg",
        "images/00273-capture.jpg",
        "images/00274-capture.jpg",
        "images/00275-capture.jpg",
        "images/00276-capture.jpg",
        "images/00277-capture.jpg",
        "images/00278-capture.jpg",
        "images/00279-capture.jpg",
        "images/00280-capture.jpg",
        "images/00281-capture.jpg",
        "images/00282-capture.jpg",
        "images/00283-capture.jpg",
        "images/00284-capture.jpg",
        "images/00285-capture.jpg",
        "images/00286-capture.jpg",
        "images/00287-capture.jpg",
        "images/00288-capture.jpg",
        "images/00289-capture.jpg",
        "images/00290-capture.jpg",
        "images/00291-capture.jpg",
        "images/00292-capture.jpg",
        "images/00293-capture.jpg",
        "images/00294-capture.jpg",
        "images/00295-capture.jpg",
        "images/00296-capture.jpg",
        "images/00297-capture.jpg",
        "images/00298-capture.jpg",
        "images/00299-capture.jpg",
        "images/00300-capture.jpg",
        "images/00301-capture.jpg",
        "images/00302-capture.jpg",
    };

    // Start time
    auto start = std::chrono::high_resolution_clock::now();

#if ANDROID
    FFmpegEncoder encoder(FFmpegEncoder::EncoderType::MEDIACODEC, 640, 480, false);
//    FFmpegEncoder encoder(FFmpegEncoder::EncoderType::LIBX264, 640, 480, false);
#else
    FFmpegEncoder encoder(FFmpegEncoder::EncoderType::NVENC, 640, 480, false);
#endif

    av_log_set_callback(log_callback_test);

    if (!encoder.Initialize(output_file)) {
        ILOGE("Initialization failed %s", output_file.c_str());

        return -1;
    }

    for (const auto& img : input_images) {
        std::string img_path = std::string(prefix_path) + "/" + img;
        if (!encoder.EncodeFrame(img_path)) {
            ILOGE("Failed to encode frame: %s", img_path.c_str());
        }
    }

    // End time
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    ILOGD("Encoding duration %lld(ms)", duration);

    return 0;
}

JNIEXPORT jint
JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    av_jni_set_java_vm(vm, reserved);
    ILOGD("JNI_OnLoad", "--------", "");
    return JNI_VERSION_1_4;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pct_ffmpeg_1hw_1encoder_MainActivity_ConvertImagesToMp4(
        JNIEnv* env,
        jobject object,
        jstring prefix_path/* this */) {
    const char *nativeString = env->GetStringUTFChars(prefix_path, 0);
    my_main(nativeString);
    env->ReleaseStringUTFChars(prefix_path, nativeString);
}
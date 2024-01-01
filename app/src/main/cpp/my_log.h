//
// Created by tripham on 12/28/23.
//

#ifndef MY_LOG_H
#define MY_LOG_H

#define LOG_TAG "ffmpeg_encoder"
#ifdef ANDROID
#include <android/log.h>
    #define ILOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
    #define ILOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
    #define ILOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
    #define ILOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#else
#define ILOGD(fmt, ...) printf(" %s " fmt "\n",getTimeFormatForDebug(), ##__VA_ARGS__)
#define ILOGI(fmt, ...) printf(" %s " fmt "\n",getTimeFormatForDebug(), ##__VA_ARGS__)
#define ILOGW(fmt, ...) printf(" %s " fmt "\n",getTimeFormatForDebug(), ##__VA_ARGS__)
#define ILOGE(fmt, ...) printf(" %s " fmt "\n",getTimeFormatForDebug(), ##__VA_ARGS__)
#endif

#endif //MY_LOG_H

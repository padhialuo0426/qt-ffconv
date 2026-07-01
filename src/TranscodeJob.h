#pragma once
#include <QString>
#include "EncodeSettings.h"

enum class JobStatus { Pending, Running, Done, Failed, Cancelled };

struct TranscodeJob {
    QString       inputPath;
    QString       outputPath;
    EncodeSettings settings;
    JobStatus     status = JobStatus::Pending;
    int           progress = 0;   // 0-100
    int           id = 0;         // 稳定 ID（不随行增删变化），日志按此归属任务
    qint64        startedMs = 0;  // 本次开始时刻，用于统计耗时
};

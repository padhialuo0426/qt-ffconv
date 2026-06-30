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
};

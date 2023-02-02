/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iostream>
#include <memory>
#include <QFileDialog>
#include "Clip.h"
#include "Frame.h"
#include "FFmpegReader.h"
#include "Timeline.h"
#include "Profiles.h"

using namespace openshot;


int main(int argc, char* argv[]) {
    QString filename = "/home/jonathan/test-crash.osp";
    //QString filename = "/home/jonathan/Downloads/drive-download-20221123T185423Z-001/project-3363/project-3363.osp";
    //QString filename = "/home/jonathan/Downloads/drive-download-20221123T185423Z-001/project-3372/project-3372.osp";
    //QString filename = "/home/jonathan/Downloads/drive-download-20221123T185423Z-001/project-3512/project-3512.osp";
    QString project_json = "";
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cout << "File error!" << std::endl;
        exit(1);
    } else {
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            project_json += line;
        }
    }

    // Open timeline reader
    std::cout << "Project JSON length: " << project_json.length() << std::endl;
    Timeline r(1280, 720, openshot::Fraction(30, 1), 44100, 2, openshot::LAYOUT_STEREO);
    r.SetJson(project_json.toStdString());
    r.DisplayInfo();
    r.Open();

    // Get max frame
    int64_t max_frame = r.GetMaxFrame();
    std::cout << "max_frame: " << max_frame << ", r.info.video_length: " << r.info.video_length << std::endl;

    for (long int frame = 1; frame <= max_frame; frame++)
    {
        float percent = (float(frame) / max_frame) * 100.0;
        std::cout << "Requesting Frame #: " << frame << " (" << percent << "%)" << std::endl;
        std::shared_ptr<Frame> f = r.GetFrame(frame);

        // Preview frame image
        if (frame % 1 == 0) {
            f->Save("preview.jpg", 1.0, "jpg", 100);
        }
    }
    r.Close();

    exit(0);
}

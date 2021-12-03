/**
 * @file
 * @brief This is a message class for thread safe comunication between ClipProcessingJobs and OpenCV classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PROCESSINGCONTROLLER_H
#define OPENSHOT_PROCESSINGCONTROLLER_H

#include <mutex>
#include <string>

class ProcessingController{
    private:
        uint processingProgress;
        bool processingFinished;
        bool stopProcessing;
        bool error = true;
        std::string error_message;

        std::mutex mtxProgress;
        std::mutex mtxFinished;
        std::mutex mtxStop;
        std::mutex mtxerror;

	public:

    ProcessingController(){
        processingProgress = 0;
        stopProcessing = false;
        processingFinished = false;
    }

    int GetFinished(){
        std::lock_guard<std::mutex> lck (mtxFinished);
        bool f = processingFinished;
        return f;
    }

    void SetFinished(bool f){
        std::lock_guard<std::mutex> lck (mtxFinished);
        processingFinished = f;
    }

    void SetProgress(uint p){
        std::lock_guard<std::mutex> lck (mtxProgress);
        processingProgress = p;
    }

    int GetProgress(){
        std::lock_guard<std::mutex> lck (mtxProgress);
        uint p = processingProgress;
        return p;
    }

    void CancelProcessing(){
        std::lock_guard<std::mutex> lck (mtxStop);
        stopProcessing = true;
    }

    bool ShouldStop(){
        std::lock_guard<std::mutex> lck (mtxStop);
        bool s = stopProcessing;
        return s;
    }

    void SetError(bool err, std::string message){
        std::lock_guard<std::mutex> lck (mtxerror);
        error = err;
        error_message = message;
    }

    bool GetError(){
        std::lock_guard<std::mutex> lck (mtxerror);
        bool e = error;
        return e;
    }

    std::string GetErrorMessage(){
        std::lock_guard<std::mutex> lck (mtxerror);
        std::string message = error_message;
        return message;
    }

};

#endif

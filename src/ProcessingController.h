/**
 * @file
 * @brief This is a message class for thread safe comunication between ClipProcessingJobs and OpenCV classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_PROCESSINGCONTROLLER_H
#define OPENSHOT_PROCESSINGCONTROLLER_H

#include <iostream>
#include <thread>
#include <mutex> 


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
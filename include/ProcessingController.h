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

        std::mutex mtxProgress;
        std::mutex mtxFinished;
        std::mutex mtxStop;

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

};

#endif
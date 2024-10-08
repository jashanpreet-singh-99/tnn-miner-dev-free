#include "miners.hpp"
#include "tnn-hugepages.h"
#include <astrobwtv3/astrobwtv3.h>
#include <astrobwtv3/lookupcompute.h>
#include <spectrex/spectrex.h>
#include <stratum/stratum.h>

void mineSpectre(int tid)
{
  int64_t localJobCounter;
  int64_t localOurHeight = 0;

  byte powHash[32];
  byte work[SpectreX::INPUT_SIZE] = {0};

  std::string diffHex;

  byte diffBytes[32];

  workerData *astroWorker = (workerData *)malloc_huge_pages(sizeof(workerData));
  SpectreX::worker *worker = (SpectreX::worker *)malloc_huge_pages(sizeof(SpectreX::worker));
  initWorker(*astroWorker);
  lookupGen(*astroWorker, nullptr, nullptr);
  worker->astroWorker = astroWorker;

waitForJob:

  while (!isConnected)
  {
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }

  while (true)
  {
    try
    {
      bool assigned = false;
      boost::json::value myJob;
      {
        std::scoped_lock<boost::mutex> lockGuard(mutex);
        myJob = job;
        localJobCounter = jobCounter;
      }
      
      if (!myJob.at("template").is_string()) {
        continue;
      }
      if (ourHeight == 0)
        continue;

      if (ourHeight == 0 || localOurHeight != ourHeight)
      {
        byte *b2 = new byte[SpectreX::INPUT_SIZE];
        switch (protocol)
        {
        case SPECTRE_SOLO:
          hexstrToBytes(std::string(myJob.at("template").as_string()), b2);
          break;
        case SPECTRE_STRATUM:
          hexstrToBytes(std::string(myJob.at("template").as_string()), b2);
          break;
        }
        memcpy(work, b2, SpectreX::INPUT_SIZE);
        SpectreX::newMatrix(work, worker->matBuffer, *worker);
        delete[] b2;
        localOurHeight = ourHeight;
      }

      double which;
      bool submit = false;
      double DIFF = 1;
      diffHex.clear();

      diffHex = cpp_int_toHex(bigDiff);

      cpp_int_to_byte_array(bigDiff, diffBytes);

      while (localJobCounter == jobCounter)
      {
        which = (double)(rand() % 10000);
        DIFF = doubleDiff;
        if (DIFF == 0)
          continue;

        byte* cmpDiff = diffBytes;

        uint64_t *nonce = &nonce0;
        (*nonce)++;

        // printf("nonce = %llu\n", *nonce);

        byte *WORK = &work[0];
        byte *nonceBytes = &WORK[72];
        uint64_t n;
        
        int enLen = 0;
        
        boost::json::value &J = myJob;
        if (!J.as_object().if_contains("extraNonce") || J.at("extraNonce").as_string().size() == 0)
          n = ((tid - 1) % (256 * 256)) | ((rand() % 256) << 16) | ((*nonce) << 24);
        else {
          uint64_t eN = std::stoull(std::string(J.at("extraNonce").as_string().c_str()), NULL, 16);
          enLen = std::string(J.at("extraNonce").as_string()).size()/2;
          int offset = (64 - enLen*8);
          n = ((tid - 1) % (256 * 256)) | (((*nonce) << 16) & ((1ULL << offset)-1)) | (eN << offset);
        }
        memcpy(nonceBytes, (byte *)&n, 8);

        // printf("after nonce: %s\n", hexStr(WORK, SpectreX::INPUT_SIZE).c_str());

        if (localJobCounter != jobCounter) {
          // printf("thread %d updating job before hash\n", tid);
          break;
        }

        SpectreX::worker &usedWorker = *worker;
        SpectreX::hash(usedWorker, WORK, SpectreX::INPUT_SIZE, powHash);

        counter.fetch_add(1);
        submit = !submitting;

        if (localJobCounter != jobCounter || localOurHeight != ourHeight) {
          // printf("thread %d updating job after hash\n", tid);
          break;
        }


        if (SpectreX::checkNonce(((uint64_t*)usedWorker.scratchData),((uint64_t*)cmpDiff)))
        {
          // printf("thread %d entered submission process\n", tid);
          if (!submit) {
            for(;;) {
              submit = !submitting;
              int64_t &rH = ourHeight;
              int64_t &oH = localOurHeight;
              if (submit || localJobCounter != jobCounter || rH != oH)
                break;
              boost::this_thread::yield();
            }
          }

          int64_t &rH = ourHeight;
          int64_t &oH = localOurHeight;
          if (localJobCounter != jobCounter || rH != oH) {
            // printf("thread %d updating job after check\n", tid);
            break;
          }
          
          submitting = true;

          setcolor(BRIGHT_YELLOW);
          std::cout << "\nThread " << tid << " found a nonce!\n" << std::flush;
          setcolor(BRIGHT_WHITE);
          switch (protocol)
          {
          case SPECTRE_SOLO:
            share = {{"block_template", hexStr(&WORK[0], SpectreX::INPUT_SIZE).c_str()}};
            break;
          case SPECTRE_STRATUM:
            std::vector<char> nonceStr;
            // Num(std::to_string((n << enLen*8) >> enLen*8).c_str(),10).print(nonceStr, 16);
            Num(std::to_string(n).c_str(),10).print(nonceStr, 16);
            share = {{{"id", SpectreStratum::submitID},
                      {"method", SpectreStratum::submit.method.c_str()},
                      {"params", {workerName,                                   // WORKER
                                  myJob.at("jobId").as_string().c_str(), // JOB ID
                                  std::string(nonceStr.data()).c_str()}}}};

            break;
          }
          data_ready = true;
          // printf("thread %d finished submission process\n", tid);
          cv.notify_all();
        }

        if (!isConnected) {
          data_ready = true;
          cv.notify_all();
          break;
        }
      }
      if (!isConnected) {
        data_ready = true;
        cv.notify_all();
        break;
      }
    }
    catch (std::exception& e)
    {
      setcolor(RED);
      std::cerr << "Error in POW Function" << std::endl;
      std::cerr << e.what() << std::endl;
      setcolor(BRIGHT_WHITE);

      localJobCounter = -1;
      localOurHeight = -1;
    }
    if (!isConnected) {
      data_ready = true;
      cv.notify_all();
      break;
    }
  }
  goto waitForJob;
}

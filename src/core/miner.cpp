//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: WebSocket SSL client, coroutine
//
//------------------------------------------------------------------------------

#include "tnn-common.hpp"
#include "tnn-hugepages.h"

#include "rootcert.h"
#include <DNSResolver.hpp>
#include "net.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <numeric>

#include "miner.h"

#include <random>

#include <hex.h>
#include "algos.h"
#include <thread>

#include <chrono>

#include <future>
#include <limits>
#include <libcubwt.cuh>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <base64.hpp>

#include <bit>
#include <broadcastServer.hpp>
#include <stratum/stratum.h>

#include <exception>

#include "reporter.hpp"
#include "coins/miners.hpp"

// INITIALIZE COMMON STUFF
int miningAlgo = SPECTRE_X;
int reportCounter = 0;
int reportInterval = 3;

uint256_t bigDiff(0);

uint64_t nonce0 = 0;

std::atomic<int64_t> counter = 0;
std::atomic<int64_t> benchCounter = 0;
boost::asio::io_context my_context;
boost::asio::steady_timer update_timer = boost::asio::steady_timer(my_context);
std::chrono::time_point<std::chrono::steady_clock> g_start_time = std::chrono::steady_clock::now();
bool printHugepagesError = true;

Num oneLsh256 = Num(1) << 256;
Num maxU256 = Num(2).pow(256) - 1;

const auto processor_count = std::thread::hardware_concurrency();

/* Start definitions from tnn-common.hpp */
int protocol = XELIS_SOLO;

std::string daemonType = "";
std::string host = "NULL";
std::string wallet = "NULL";

int jobCounter;

int blockCounter;
int miniBlockCounter;
int rejected;
int accepted;


//static int firstRejected;

//uint64_t hashrate;
int64_t ourHeight;

int64_t difficulty;

double doubleDiff;

bool useLookupMine = false;

std::vector<int64_t> rate5min;
std::vector<int64_t> rate1min;
std::vector<int64_t> rate30sec;

std::string workerName = "default";
std::string workerNameFromWallet = "";

bool isConnected = false;

bool beQuiet = false;
/* End definitions from tnn-common.hpp */

/* Start definitions from astrobwtv3.h */
#if defined(TNN_ASTROBWTV3)
AstroFunc allAstroFuncs[] = {
  {"branch", branchComputeCPU},
  {"lookup", lookupCompute},
  {"wolf", wolfCompute},
#if defined(__AVX2__)
  {"avx2z", branchComputeCPU_avx2_zOptimized}
#elif defined(__aarch64__)
  {"aarch64", branchComputeCPU_aarch64}
#endif
};
size_t numAstroFuncs;
#endif
/* End definitions from astrobwtv3.h */

// #include <cuda_runtime.h>

namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace po = boost::program_options;  // from <boost/program_options.hpp>

boost::mutex mutex;
boost::mutex userMutex;
boost::mutex reportMutex;

uint16_t *lookup2D_global; // Storage for computed values of 2-byte chunks
byte *lookup3D_global;     // Storage for deterministically computed values of 1-byte chunks

using byte = unsigned char;
int bench_duration = -1;
bool startBenchmark = false;
bool stopBenchmark = false;
//------------------------------------------------------------------------------

void openssl_log_callback(const SSL *ssl, int where, int ret)
{
  if (ret <= 0)
  {
    int error = SSL_get_error(ssl, ret);
    char errbuf[256];
    ERR_error_string_n(error, errbuf, sizeof(errbuf));
    std::cerr << "OpenSSL Error: " << errbuf << std::endl;
  }
}

//------------------------------------------------------------------------------

#if defined(TNN_ASTROBWTV3)
void initializeExterns() {
  numAstroFuncs = std::size(allAstroFuncs); //sizeof(allAstroFuncs)/sizeof(allAstroFuncs[0]);
}
#endif

void onExit() {
  setcolor(BRIGHT_WHITE);
  printf("\n\nExiting Miner...\n");
  fflush(stdout);
  boost::this_thread::sleep_for(boost::chrono::seconds(1));
  fflush(stdout);

#if defined(_WIN32)
  SetConsoleMode(hInput, ENABLE_EXTENDED_FLAGS | (prev_mode));
#endif
}

void sigterm(int signum) {
  std::cout << "\n\nTerminate signal (" << signum << ") received.\n" << std::flush;
  exit(0);
}

void sigint(int signum) {
  std::cout << "\n\nInterrupt signal (" << signum << ") received.\n" << std::flush;
  exit(0);
}

int main(int argc, char **argv)
{
  std::atexit(onExit);
  signal(SIGTERM, sigterm);
  signal(SIGINT, sigint);
  alignas(64) char buf[65536];
  setvbuf(stdout, buf, _IOFBF, 65536);
  srand(time(NULL)); // Placing higher here to ensure the effect cascades through the entire program

  #if defined(TNN_ASTROBWTV3)
  initWolfLUT();
  initializeExterns();
  #endif

  // Check command line arguments.
  lookup2D_global = (uint16_t *)malloc_huge_pages(regOps_size * (256 * 256) * sizeof(uint16_t));
  lookup3D_global = (byte *)malloc_huge_pages(branchedOps_size * (256 * 256) * sizeof(byte));


  // default values
  bool lockThreads = true;

  po::variables_map vm;
  po::options_description opts = get_prog_opts();
  try
  {
    int style = get_prog_style();
    po::store(po::command_line_parser(argc, argv).options(opts).style(style).run(), vm);
    po::notify(vm);
  }
  catch (std::exception &e)
  {
    printf("%s v%s\n", consoleLine, versionString);
    std::cerr << "Error: " << e.what() << "\n";
    std::cerr << "Remember: Long options now use a double-dash -- instead of a single-dash -\n";
    return -1;
  }
  catch (...)
  {
    std::cerr << "Unknown error!" << "\n";
    return -1;
  }

#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);
  hInput = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hInput, &prev_mode); 
  SetConsoleMode(hInput, ENABLE_EXTENDED_FLAGS | (prev_mode & ~ENABLE_QUICK_EDIT_MODE));
#endif
  setcolor(BRIGHT_WHITE);
  printf("%s v%s\n", consoleLine, versionString);
  if(vm.count("quiet")) {
    beQuiet = true;
  } else {
    printf("%s", TNN);
  }
#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);
  HANDLE hSelfToken = NULL;

  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, &hSelfToken);
  if (SetPrivilege(hSelfToken, SE_LOCK_MEMORY_NAME, true))
    std::cout << "Permission Granted for Huge Pages!" << std::endl;
  else
    std::cout << "Huge Pages: Permission Failed..." << std::endl;

  // SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif

  if (vm.count("help"))
  {
    std::cout << opts << std::endl;
    boost::this_thread::sleep_for(boost::chrono::seconds(1));
    return 0;
  }
  
  if (vm.count("spectre"))
  {
    #if defined(TNN_ASTROBWTV3)
    symbol = "SPR";
    protocol = SPECTRE_STRATUM;
    #else
    setcolor(RED);
    printf(unsupported_astro);
    fflush(stdout);
    setcolor(BRIGHT_WHITE);
    return 1;
    #endif
  }

  protocol = vm.count("xatum") ? XELIS_XATUM : protocol;

  useStratum |= vm.count("stratum");
  devSelection = vm.count("testnet") ? testDevWallet : devSelection;

  if (vm.count("test-spectre"))
  {
    #if defined(TNN_ASTROBWTV3)
    return SpectreX::test();
    #else
    setcolor(RED);
    printf(unsupported_astro);
    fflush(stdout);
    setcolor(BRIGHT_WHITE);
    return 1;
    #endif
  }

  if (vm.count("sabench"))
  {
    #if defined(TNN_ASTROBWTV3)
    runDivsufsortBenchmark();
    return 0;
    #else
    setcolor(RED);
    printf(unsupported_astro);
    fflush(stdout);
    setcolor(BRIGHT_WHITE);
    #endif
  }

  if (vm.count("daemon-address"))
  {
    host = vm["daemon-address"].as<std::string>();
    boost::char_separator<char> sep(":");
    boost::tokenizer<boost::char_separator<char>> tok(host, sep);
    std::vector<std::string> tokens;
    std::copy(tok.begin(), tok.end(), std::back_inserter<std::vector<std::string> >(tokens));
    if(tokens.size() == 2) {
      host = tokens[0];
      try
      {
        // given host:port
        const int i{std::stoi(tokens[1])};
        port = tokens[1];
      }
      catch (...)
      {
        // protocol:host
        daemonType = tokens[0];
        host = tokens[1];
      }
    } else if(tokens.size() == 3) {
      daemonType = tokens[0];  // wss, stratum+tcp, stratum+ssl, et al
      host = tokens[1];
      port = tokens[2];
    }
    boost::replace_all(host, "/", "");
    if (daemonType.size() > 0) {
      if (daemonType.find("stratum") != std::string::npos) useStratum = true;
      if (daemonType.find("xatum") != std::string::npos) protocol = XELIS_XATUM;
    }
  }

  if (vm.count("port"))
  {
    port = std::to_string(vm["port"].as<int>());
    try {
      const int i{std::stoi(port)};
    } catch (...) {
      printf("ERROR: provided port is invalid: %s\n", port.c_str());
      return 1;
    }
  }
  if (vm.count("wallet"))
  {
    wallet = vm["wallet"].as<std::string>();
    if(wallet.find("spectre", 0) != std::string::npos) {
      symbol = "SPR";
      protocol = SPECTRE_STRATUM;
    }

    boost::char_separator<char> sep(".");
    boost::tokenizer<boost::char_separator<char>> tok(wallet, sep);
    std::vector<std::string> tokens;
    std::copy(tok.begin(), tok.end(), std::back_inserter<std::vector<std::string> >(tokens));
    if(tokens.size() == 2) {
      wallet = tokens[0];
      workerNameFromWallet = tokens[1];
    }
  }
  if (vm.count("ignore-wallet"))
  {
    checkWallet = false;
  }
  if (vm.count("worker-name"))
  {
    workerName = vm["worker-name"].as<std::string>();
  }
  else
  {
    if(workerNameFromWallet != "") {
      workerName = workerNameFromWallet;
    } else {
      workerName = boost::asio::ip::host_name();
    }
  }
  if (vm.count("threads"))
  {
    threads = vm["threads"].as<int>();
  }
  if (vm.count("report-interval"))
  {
    reportInterval = vm["report-interval"].as<int>();
  }
  if (vm.count("dev-fee"))
  {
    setcolor(GREEN);
    printf("CONGRATS:\n           [-_-] This miner is dev-fee free. So ignoring your request. [-_-]\n\n");
    fflush(stdout);
    setcolor(BRIGHT_WHITE);
    boost::this_thread::sleep_for(boost::chrono::seconds(1));
  }
  if (vm.count("no-lock"))
  {
    setcolor(CYAN);
    printf("CPU affinity has been disabled\n");
    fflush(stdout);
    setcolor(BRIGHT_WHITE);
    lockThreads = false;
  }

  if (vm.count("broadcast"))
  {
    broadcastStats = true;
  }

  // Test-specific
  if (vm.count("op"))
  {
    testOp = vm["op"].as<int>();
  }
  if (vm.count("len"))
  {
    testLen = vm["len"].as<int>();
  }
  if (vm.count("lookup"))
  {
    printf("Use Lookup\n");
    useLookupMine = true;
  }

  // We can do this because we've set default in terminal.h
  tuneWarmupSec = vm["tune-warmup"].as<int>();
  tuneDurationSec = vm["tune-duration"].as<int>();

fillBlanks:
{
  if (symbol == nullArg)
  {
    setcolor(CYAN);
    printf("%s\n", coinPrompt);
    fflush(stdout);
    setcolor(BRIGHT_WHITE);

    std::string cmdLine;
    std::getline(std::cin, cmdLine);
    if (cmdLine != "" && cmdLine.find_first_not_of(' ') != std::string::npos)
    {
      symbol = cmdLine;
    }
    else
    {
      symbol = "SPR";
      setcolor(BRIGHT_YELLOW);
      printf("Default value will be used: %s\n\n", "SPR");
      fflush(stdout);
      setcolor(BRIGHT_WHITE);
    }
  }

  auto it = coinSelector.find(symbol);
  if (it != coinSelector.end())
  {
    miningAlgo = it->second;
  }
  else
  {
    setcolor(RED);
    std::cout << "ERROR: Invalid coin symbol: " << symbol << std::endl << std::flush;
    setcolor(BRIGHT_YELLOW);
    it = coinSelector.begin();
    printf("Supported symbols are:\n");
    while (it != coinSelector.end())
    {
      printf("%s\n", it->first.c_str());
      it++;
    }
    printf("\n");
    fflush(stdout);
    setcolor(BRIGHT_WHITE);
    symbol = nullArg;
    goto fillBlanks;
  }

  // necessary as long as the bridge is a thing
  if (miningAlgo == SPECTRE_X) useStratum = true;

  int i = 0;
  std::vector<std::string *> stringParams = {&host, &port, &wallet};
  std::vector<const char *> stringDefaults = {defaultHost.c_str(), devPort.c_str(), devSelection.c_str()};
  std::vector<const char *> stringPrompts = {daemonPrompt, portPrompt, walletPrompt};
  for (std::string *param : stringParams)
  {
    if (*param == nullArg)
    {
      setcolor(CYAN);
      printf("%s\n", stringPrompts[i]);
      fflush(stdout);
      setcolor(BRIGHT_WHITE);

      std::string cmdLine;
      std::getline(std::cin, cmdLine);
      if (cmdLine != "" && cmdLine.find_first_not_of(' ') != std::string::npos)
      {
        *param = cmdLine;
      }
      else
      {
        *param = stringDefaults[i];
        setcolor(BRIGHT_YELLOW);
        printf("Default value will be used: %s\n\n", (*param).c_str());
        fflush(stdout);
        setcolor(BRIGHT_WHITE);
      }

      if (param == &host) {
        boost::char_separator<char> sep(":");
        boost::tokenizer<boost::char_separator<char>> tok(host, sep);
        std::vector<std::string> tokens;
        std::copy(tok.begin(), tok.end(), std::back_inserter<std::vector<std::string> >(tokens));
        if(tokens.size() == 2) {
          host = tokens[0];
          try
          {
            // given host:port
            const int i{std::stoi(tokens[1])};
            port = tokens[1];
          }
          catch (...)
          {
            // protocol:host
            daemonType = tokens[0];
            host = tokens[1];
          }
        } else if(tokens.size() == 3) {
          daemonType = tokens[0];  // wss, stratum+tcp, stratum+ssl, et al
          host = tokens[1];
          port = tokens[2];
        }
        boost::replace_all(host, "/", "");
        if (daemonType.size() > 0) {
          if (daemonType.find("stratum") != std::string::npos) useStratum = true;
        }
      }
    }
    i++;
  }

  if (useStratum)
  {
    switch (miningAlgo)
    {
      case SPECTRE_X:
        protocol = SPECTRE_STRATUM;
        break;
    }
  }

  if (threads == 0)
  {
    threads = processor_count;
  }

  #if defined(_WIN32)
    if (threads > 32) lockThreads = false;
  #endif

  setcolor(BRIGHT_YELLOW);

  #ifdef TNN_ASTROBWTV3
  if (miningAlgo == SPECTRE_X) {
    if (vm.count("no-tune")) {
      std::string noTune = vm["no-tune"].as<std::string>();
      if(!setAstroAlgo(noTune)) {
        throw po::validation_error(po::validation_error::invalid_option_value, "no-tune");
      }
    } else {
      astroTune(threads, tuneWarmupSec, tuneDurationSec);
    }
  }
  fflush(stdout);
  setcolor(BRIGHT_WHITE);
  #endif

  printf("\n");
}

Mining:
{
  if (miningAlgo == SPECTRE_X && (wallet.find("spectre", 0) == std::string::npos)) {
    std::cout << "Provided wallet address is not valid for Spectre" << std::endl;
    return EXIT_FAILURE;
  }
  boost::thread GETWORK(getWork, miningAlgo);
  // setPriority(GETWORK.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);

  unsigned int n = std::thread::hardware_concurrency();


  std::cout << "Starting threads: ";
  for (int i = 0; i < threads; i++)
  {

    boost::thread t(POW[miningAlgo], i + 1);

    if (lockThreads)
    {
      setAffinity(t.native_handle(), i);
    }
    // if (threads == 1 || (n > 2 && i <= n - 2))
    // setPriority(t.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);

    std::cout << i + 1;
    if(i+1 != threads)
      std::cout << ", ";
  }
  std::cout << std::endl;

  g_start_time = std::chrono::steady_clock::now();
  if (broadcastStats)
  {
    boost::thread BROADCAST(BroadcastServer::serverThread, &rate30sec, &accepted, &rejected, versionString, reportInterval);
  }

  while (!isConnected)
  {
    boost::this_thread::yield();
  }

  // boost::thread reportThread([&]() {
  // Set an expiry time relative to now.
  update_timer.expires_after(std::chrono::seconds(1));

  // Start an asynchronous wait.
  update_timer.async_wait(update_handler);
  my_context.run();
  // });
  // setPriority(reportThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

  for(;;) {
    std::this_thread::yield();
  }

  return EXIT_SUCCESS;
}
}

void logSeconds(std::chrono::steady_clock::time_point start_time, int duration, bool *stop)
{
  int i = 0;
  while (!(*stop))
  {
    auto now = std::chrono::steady_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    if (milliseconds >= 1000)
    {
      start_time = now;
     //  mutex.lock();
      // std::cout << "\n" << std::flush;
      printf("\rBENCHMARKING: %d/%d seconds elapsed...", i, duration);
      std::cout << std::flush;
     //  mutex.unlock();
      i++;
    }
    boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
  }
}

#if defined(_WIN32)
DWORD_PTR SetThreadAffinityWithGroups(HANDLE threadHandle, DWORD_PTR coreIndex)
{
  DWORD numGroups = GetActiveProcessorGroupCount();

  // Calculate group and processor within the group
  DWORD group = static_cast<DWORD>(coreIndex / 64);
  DWORD numProcessorsInGroup = GetMaximumProcessorCount(group);
  DWORD processorInGroup = static_cast<DWORD>(coreIndex % numProcessorsInGroup);

  if (group < numGroups)
  {
    GROUP_AFFINITY groupAffinity = {};
    groupAffinity.Group = static_cast<WORD>(group);
    groupAffinity.Mask = static_cast<KAFFINITY>(1ULL << processorInGroup);

    GROUP_AFFINITY previousGroupAffinity;
    if (!SetThreadGroupAffinity(threadHandle, &groupAffinity, &previousGroupAffinity))
    {
      return 0; // Fail case, return 0 like SetThreadAffinityMask
    }

    // Return the previous affinity mask for compatibility with your code
    return previousGroupAffinity.Mask;
  }

  return 0; // If out of bounds
}
#endif

void setAffinity(boost::thread::native_handle_type t, uint64_t core)
{
#if defined(_WIN32)
  HANDLE threadHandle = t;
  DWORD_PTR affinityMask = core;
  DWORD_PTR previousAffinityMask = SetThreadAffinityWithGroups(threadHandle, affinityMask);
  if (previousAffinityMask == 0)
  {
    DWORD error = GetLastError();
    std::cerr << "Failed to set CPU affinity for thread. Error code: " << error << std::endl;
  }

#elif !defined(__APPLE__)
  // Get the native handle of the thread
  pthread_t threadHandle = t;

  // Create a CPU set with a single core
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset); // Set core 0

  // Set the CPU affinity of the thread
  if (pthread_setaffinity_np(threadHandle, sizeof(cpu_set_t), &cpuset) != 0)
  {
    std::cerr << "Failed to set CPU affinity for thread" << std::endl;
  }

#endif
}

void setPriority(boost::thread::native_handle_type t, int priority)
{
#if defined(_WIN32)

  HANDLE threadHandle = t;

  // Set the thread priority
  int threadPriority = priority;
  BOOL success = SetThreadPriority(threadHandle, threadPriority);
  if (!success)
  {
    DWORD error = GetLastError();
    std::cerr << "Failed to set thread priority. Error code: " << error << std::endl;
  }

#else
  // Get the native handle of the thread
  pthread_t threadHandle = t;

  // Set the thread priority
  int threadPriority = priority;
  // do nothing

#endif
}

void getWork(int algo)
{
  net::io_context ioc;
  ssl::context ctx = ssl::context{ssl::context::tlsv12_client};
  load_root_certificates(ctx);

  bool caughtDisconnect = false;

connectionAttempt:
  bool *B = &isConnected;
  *B = false;
  //  mutex.lock();
  setcolor(BRIGHT_YELLOW);
  std::cout << "Connecting...\n" << std::flush;
  setcolor(BRIGHT_WHITE);
  //  mutex.unlock();
  try
  {
    // Launch the asynchronous operation
    bool err = false;
    boost::asio::spawn(ioc, std::bind(&do_session, daemonType, protocol, host, port, wallet, workerName, algo, std::ref(ioc), std::ref(ctx), std::placeholders::_1),
                        // on completion, spawn will call this function
                        [&](std::exception_ptr ex)
                        {
                          if (ex)
                          {
                            std::rethrow_exception(ex);
                            err = true;
                          }
                        });
    ioc.run();
    if (err)
    {
      //  mutex.lock();
      setcolor(RED);
      std::cerr << "\nError establishing connections" << std::endl
                << "Will try again in 10 seconds...\n\n" << std::flush;
      setcolor(BRIGHT_WHITE);
      //  mutex.unlock();
      boost::this_thread::sleep_for(boost::chrono::milliseconds(10000));
      ioc.reset();
      goto connectionAttempt;
    }
    else
    {
      caughtDisconnect = false;
    }
  }
  catch (...)
  {
    //  mutex.lock();
    setcolor(RED);
    std::cerr << "\nError establishing connections" << std::endl
              << "Will try again in 10 seconds...\n\n" << std::flush;
    setcolor(BRIGHT_WHITE);
    //  mutex.unlock();
    boost::this_thread::sleep_for(boost::chrono::milliseconds(10000));
    ioc.reset();
    goto connectionAttempt;
  }
  while (*B)
  {
    caughtDisconnect = false;
    boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
  }
  //  mutex.lock();
  setcolor(RED);
  if (!caughtDisconnect)
    std::cerr << "\nERROR: lost connection" << std::endl
              << "Will try to reconnect in 10 seconds...\n\n";
  else
    std::cerr << "\nError establishing connection" << std::endl
              << "Will try again in 10 seconds...\n\n";

  fflush(stdout);
  setcolor(BRIGHT_WHITE);

  rate30sec.clear();
  //  mutex.unlock();
  caughtDisconnect = true;
  boost::this_thread::sleep_for(boost::chrono::milliseconds(10000));
  ioc.reset();
  goto connectionAttempt;
}

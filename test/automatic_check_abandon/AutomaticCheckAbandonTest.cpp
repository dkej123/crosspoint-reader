// Requirements for the background automatic progress check (pull):
//  - a completed check reports the remote position and releases the radio;
//  - a missing server record or a network failure is reported, never crashes,
//    and still releases the radio when the check owned it;
//  - leaving the book (or sleeping) while the check runs must not kill it:
//    the job stays valid until the task finishes, and from the moment it is
//    abandoned the task makes no further radio changes, because the automatic
//    upload that starts on leaving the book may already be using WiFi.
#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/activities/reader/AutomaticProgressCheckJob.h"

using Job = AutomaticProgressCheckJob;
using Status = AutomaticProgressCheckJob::Status;

namespace {

struct FakeSteps {
  bool wifiAvailable = true;
  std::string hash = "0123456789abcdef0123456789abcdef";
  KOReaderSyncClient::Error fetchResult = KOReaderSyncClient::OK;
  float remotePercentage = 0.42f;
  // Hooks that run "while" a step is in progress (e.g. the user leaves the book).
  std::function<void()> duringConnect;
  std::function<void()> duringFetch;

  std::vector<std::string> calls;

  bool connect(const std::atomic<bool>& abandoned) {
    calls.push_back("connect");
    if (duringConnect) duringConnect();
    return wifiAvailable && !abandoned.load();
  }
  std::string documentHash(const std::string&) {
    calls.push_back("hash");
    return hash;
  }
  KOReaderSyncClient::Error fetch(const std::string&, KOReaderProgress& out) {
    calls.push_back("fetch");
    if (duringFetch) duringFetch();
    out.percentage = remotePercentage;
    return fetchResult;
  }
  void releaseRadio() { calls.push_back("release"); }

  int count(const std::string& name) const {
    int n = 0;
    for (const auto& c : calls) n += c == name;
    return n;
  }
};

void prepare(Job& j) { j.epubPath = "/Hugh Howey - Zmiana.epub"; }

}  // namespace

TEST(AutomaticCheck, SuccessReportsRemotePositionAndReleasesRadio) {
  Job job;
  prepare(job);
  FakeSteps steps;
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(job.status.load(), Status::DONE_OK);
  EXPECT_FLOAT_EQ(job.remoteProgress.percentage, 0.42f);
  EXPECT_EQ(steps.count("release"), 1);
}

TEST(AutomaticCheck, NoServerRecordIsReportedAndRadioReleased) {
  Job job;
  prepare(job);
  FakeSteps steps;
  steps.fetchResult = KOReaderSyncClient::NOT_FOUND;
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(job.status.load(), Status::DONE_NOT_FOUND);
  EXPECT_EQ(steps.count("release"), 1);
}

TEST(AutomaticCheck, ServerFailureIsReportedAndRadioReleased) {
  Job job;
  prepare(job);
  FakeSteps steps;
  steps.fetchResult = KOReaderSyncClient::SERVER_ERROR;
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(job.status.load(), Status::DONE_ERROR);
  EXPECT_EQ(job.error, KOReaderSyncClient::SERVER_ERROR);
  EXPECT_EQ(steps.count("release"), 1);
}

TEST(AutomaticCheck, NoWifiEndsWithoutFetching) {
  Job job;
  prepare(job);
  FakeSteps steps;
  steps.wifiAvailable = false;
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(job.status.load(), Status::DONE_ERROR);
  EXPECT_EQ(steps.count("fetch"), 0);
}

TEST(AutomaticCheck, UnreadableBookEndsWithoutFetching) {
  Job job;
  prepare(job);
  FakeSteps steps;
  steps.hash.clear();
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(job.status.load(), Status::DONE_ERROR);
  EXPECT_EQ(steps.count("fetch"), 0);
  EXPECT_EQ(steps.count("release"), 1);
}

TEST(AutomaticCheck, AbandonedBeforeStartNeverTouchesTheRadio) {
  Job job;
  prepare(job);
  job.abandoned = true;
  FakeSteps steps;
  runAutomaticProgressCheck(job, steps);
  EXPECT_TRUE(steps.calls.empty());
  EXPECT_NE(job.status.load(), Status::RUNNING);
}

TEST(AutomaticCheck, LeavingTheBookDuringConnectStopsFurtherRadioWork) {
  Job job;
  prepare(job);
  FakeSteps steps;
  steps.duringConnect = [&] { job.abandoned = true; };
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(steps.count("fetch"), 0);
  EXPECT_EQ(steps.count("release"), 0);
  EXPECT_NE(job.status.load(), Status::RUNNING);
}

TEST(AutomaticCheck, LeavingTheBookDuringFetchLeavesTheRadioToTheUpload) {
  Job job;
  prepare(job);
  FakeSteps steps;
  steps.duringFetch = [&] { job.abandoned = true; };
  runAutomaticProgressCheck(job, steps);
  EXPECT_EQ(steps.count("release"), 0);
  EXPECT_NE(job.status.load(), Status::DONE_OK);  // stale result is not offered
  EXPECT_NE(job.status.load(), Status::RUNNING);
}

TEST(AutomaticCheck, JobOutlivesTheReaderThatAbandonedIt) {
  auto readerRef = std::make_shared<Job>();
  prepare(*readerRef);
  auto taskRef = readerRef;  // what the task holds
  std::weak_ptr<Job> watch = readerRef;

  FakeSteps steps;
  steps.duringFetch = [&] {
    readerRef->abandoned = true;  // reader leaves...
    readerRef.reset();            // ...and is destroyed mid-fetch
  };
  runAutomaticProgressCheck(*taskRef, steps);  // must still run on valid memory
  EXPECT_FALSE(watch.expired());
  taskRef.reset();  // task ends
  EXPECT_TRUE(watch.expired());
}

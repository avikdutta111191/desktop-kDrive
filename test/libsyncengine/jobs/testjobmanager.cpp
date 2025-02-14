/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testjobmanager.h"

#include "config.h"
#include "db/parmsdb.h"
#include "jobs/jobmanager.h"
#include "jobs/network/createdirjob.h"
#include "jobs/network/deletejob.h"
#include "jobs/network/getfilelistjob.h"
#include "jobs/network/uploadjob.h"
#include "network/proxy.h"
#include "libcommon/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <unordered_set>

using namespace CppUnit;

namespace KDC {

static const SyncPath localTestDirPath(std::wstring(L"" TEST_DIR) + L"/test_ci");
static const SyncPath localTestDirPath_manyFiles(std::wstring(L"" TEST_DIR) + L"/test_ci/many_files_dir");
static const SyncPath localTestDirPath_pictures(std::wstring(L"" TEST_DIR) + L"/test_ci/test_pictures");
static const SyncPath localTestDirPath_bigFiles(std::wstring(L"" TEST_DIR) + L"/test_ci/big_file_dir");

void KDC::TestJobManager::setUp() {
    const std::string userIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_USER_ID");
    const std::string accountIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_ACCOUNT_ID");
    const std::string driveIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_DRIVE_ID");
    const std::string remoteDirIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_DIR_ID");
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");

    if (userIdStr.empty() || accountIdStr.empty() || driveIdStr.empty() || remoteDirIdStr.empty() || apiTokenStr.empty()) {
        throw std::runtime_error("Some environment variables are missing!");
    }

    // Insert api token into keystore
    std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiTokenStr);

    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    int userId(atoi(userIdStr.c_str()));
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);

    int accountId(atoi(accountIdStr.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    int driveId = atoi(driveIdStr.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    NodeId remoteDirId = std::string(remoteDirIdStr);

    // Setup proxy
    Parameters parameters;
    bool found;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    SyncName dirName = Str("testJobManager_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
    CreateDirJob job(_driveDbId, dirName, remoteDirId, dirName);
    job.runSynchronously();

    // Extract file ID
    Poco::JSON::Object::Ptr resObj = job.jsonRes();
    if (resObj) {
        Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
        if (dataObj) {
            _dirId = dataObj->get(idKey).toString();
        }
    }

    if (_dirId.empty()) {
        throw std::runtime_error("Failed to create test directory!");
    }
}

void KDC::TestJobManager::tearDown() {
    ParmsDb::instance()->close();
    DeleteJob job(_driveDbId, _dirId, "", "");  // TODO : this test needs to be fixed, local ID and path are now mandatory
    job.runSynchronously();
}

void TestJobManager::testWithoutCallback() {
    // Upload all files in testDir
    ulong counter = 0;
    for (auto &dirEntry : std::filesystem::directory_iterator(localTestDirPath_manyFiles)) {
        if (dirEntry.path().filename() == ".DS_Store") {
            continue;
        }

        std::shared_ptr<UploadJob> job =
            std::make_shared<UploadJob>(_driveDbId, dirEntry.path(), dirEntry.path().filename().native(), _dirId, 0);
        JobManager::instance()->queueAsyncJob(job);
        counter++;
    }

    Utility::msleep(10000);  // Wait 10sec

    GetFileListJob fileListJob(_driveDbId, _dirId);
    fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    size_t total = data->size();
    CPPUNIT_ASSERT(counter == total);
}

std::unordered_map<uint64_t, std::shared_ptr<UploadJob>> ongoingJobs;
void callback(uint64_t jobId) {
    ongoingJobs.erase(jobId);
}

void TestJobManager::testWithCallback() {
    // Upload all files in testDir
    ulong counter = 0;
    for (auto &dirEntry : std::filesystem::directory_iterator(localTestDirPath_manyFiles)) {
        if (dirEntry.path().filename() == ".DS_Store") {
            continue;
        }

        std::shared_ptr<UploadJob> job =
            std::make_shared<UploadJob>(_driveDbId, dirEntry.path(), dirEntry.path().filename().native(), _dirId, 0);
        JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
        counter++;
        ongoingJobs.insert({job->jobId(), job});
    }

    Utility::msleep(10000);  // Wait 10sec

    GetFileListJob fileListJob(_driveDbId, _dirId);
    fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    size_t total = data->size();
    CPPUNIT_ASSERT(counter == total);
    CPPUNIT_ASSERT(ongoingJobs.empty());
}

void cancelAllJobs() {
    for (auto job : ongoingJobs) {
        job.second->abort();
    }
    ongoingJobs.clear();
}

void TestJobManager::testCancelJobs() {
    // Upload all files in testDir
    ulong jobCounter = 0;
    for (auto &dirEntry : std::filesystem::directory_iterator(localTestDirPath_manyFiles)) {
        std::shared_ptr<UploadJob> job =
            std::make_shared<UploadJob>(_driveDbId, dirEntry.path(), dirEntry.path().filename().native(), _dirId, 0);
        JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
        jobCounter++;
        ongoingJobs.insert({job->jobId(), job});
    }

    Utility::msleep(1000);  // Wait 1sec

    cancelAllJobs();

    Utility::msleep(10000);  // Wait 10sec

    GetFileListJob fileListJob(_driveDbId, _dirId);
    fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    size_t total = data->size();
    CPPUNIT_ASSERT(jobCounter != total);
    CPPUNIT_ASSERT(total > 0);
    CPPUNIT_ASSERT(ongoingJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_managedJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_queuedJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_runningJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_pendingJobs.empty());
}

std::queue<int64_t> finishedJobs;
void callbackJobDependency(int64_t jobId) {
    finishedJobs.push(jobId);
}

void TestJobManager::testJobDependencies() {
    SyncPath pict1Path = localTestDirPath_pictures / "picture-1.jpg";
    SyncPath pict2Path = localTestDirPath_pictures / "picture-2.jpg";
    SyncPath pict3Path = localTestDirPath_pictures / "picture-3.jpg";
    SyncPath pict4Path = localTestDirPath_pictures / "picture-4.jpg";
    SyncPath pict5Path = localTestDirPath_pictures / "picture-5.jpg";

    // Upload all files in testDir
    std::shared_ptr<UploadJob> job1 =
        std::make_shared<UploadJob>(_driveDbId, pict1Path, pict1Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job2 =
        std::make_shared<UploadJob>(_driveDbId, pict2Path, pict2Path.filename().native(), _dirId, 0);
    job2->setParentJobId(job1->jobId());
    std::shared_ptr<UploadJob> job3 =
        std::make_shared<UploadJob>(_driveDbId, pict3Path, pict3Path.filename().native(), _dirId, 0);
    job3->setParentJobId(job2->jobId());
    std::shared_ptr<UploadJob> job4 =
        std::make_shared<UploadJob>(_driveDbId, pict4Path, pict4Path.filename().native(), _dirId, 0);
    job4->setParentJobId(job3->jobId());
    std::shared_ptr<UploadJob> job5 =
        std::make_shared<UploadJob>(_driveDbId, pict5Path, pict5Path.filename().native(), _dirId, 0);
    job5->setParentJobId(job4->jobId());
    JobManager::instance()->queueAsyncJob(job1, Poco::Thread::PRIO_NORMAL, callbackJobDependency);
    JobManager::instance()->queueAsyncJob(job2, Poco::Thread::PRIO_NORMAL, callbackJobDependency);
    JobManager::instance()->queueAsyncJob(job3, Poco::Thread::PRIO_NORMAL, callbackJobDependency);
    JobManager::instance()->queueAsyncJob(job4, Poco::Thread::PRIO_NORMAL, callbackJobDependency);
    JobManager::instance()->queueAsyncJob(job5, Poco::Thread::PRIO_NORMAL, callbackJobDependency);

    Utility::msleep(10000);  // Wait 10sec

    while (1) {
        UniqueId prevId = finishedJobs.front();
        finishedJobs.pop();
        if (finishedJobs.empty()) {
            break;
        }
        UniqueId currId = finishedJobs.front();
        CPPUNIT_ASSERT(prevId < currId);
    }
}

void TestJobManager::testJobPriority() {
    SyncPath pict1Path = localTestDirPath_pictures / "picture-1.jpg";
    SyncPath pict2Path = localTestDirPath_pictures / "picture-2.jpg";
    SyncPath pict3Path = localTestDirPath_pictures / "picture-3.jpg";
    SyncPath pict4Path = localTestDirPath_pictures / "picture-4.jpg";
    SyncPath pict5Path = localTestDirPath_pictures / "picture-5.jpg";

    // Upload all files in testDir
    std::shared_ptr<UploadJob> job1 =
        std::make_shared<UploadJob>(_driveDbId, pict1Path, pict1Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job2 =
        std::make_shared<UploadJob>(_driveDbId, pict2Path, pict2Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job3 =
        std::make_shared<UploadJob>(_driveDbId, pict3Path, pict3Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job4 =
        std::make_shared<UploadJob>(_driveDbId, pict4Path, pict4Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job5 =
        std::make_shared<UploadJob>(_driveDbId, pict5Path, pict5Path.filename().native(), _dirId, 0);
    JobManager::instance()->queueAsyncJob(job1, Poco::Thread::PRIO_LOWEST);
    JobManager::instance()->queueAsyncJob(job2, Poco::Thread::PRIO_LOW);
    JobManager::instance()->queueAsyncJob(job3, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job4, Poco::Thread::PRIO_HIGH);
    JobManager::instance()->queueAsyncJob(job5, Poco::Thread::PRIO_HIGHEST);

    Utility::msleep(10000);  // Wait 10sec

    // Don't know how to test it but logs looks good...
}

void TestJobManager::testJobPriority2() {
    SyncPath pict1Path = localTestDirPath_pictures / "picture-1.jpg";
    SyncPath pict2Path = localTestDirPath_pictures / "picture-2.jpg";
    SyncPath pict3Path = localTestDirPath_pictures / "picture-3.jpg";
    SyncPath pict4Path = localTestDirPath_pictures / "picture-4.jpg";
    SyncPath pict5Path = localTestDirPath_pictures / "picture-5.jpg";

    // Upload all files in testDir

    std::shared_ptr<UploadJob> job1 =
        std::make_shared<UploadJob>(_driveDbId, pict1Path, pict1Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job2 =
        std::make_shared<UploadJob>(_driveDbId, pict2Path, pict2Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job3 =
        std::make_shared<UploadJob>(_driveDbId, pict3Path, pict3Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job4 =
        std::make_shared<UploadJob>(_driveDbId, pict4Path, pict4Path.filename().native(), _dirId, 0);
    std::shared_ptr<UploadJob> job5 =
        std::make_shared<UploadJob>(_driveDbId, pict5Path, pict5Path.filename().native(), _dirId, 0);

    JobManager::instance()->queueAsyncJob(job1, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job2, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job3, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job4, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job5, Poco::Thread::PRIO_NORMAL);

    Utility::msleep(10000);  // Wait 10sec

    // Don't know how to test it but logs looks good...
}

void TestJobManager::testJobPriority3() {
    SyncPath pict5Path = localTestDirPath_pictures / "picture-5.jpg";

    // Upload all files in testDir
    for (int i = 0; i < 100; i++) {
        std::shared_ptr<UploadJob> job = std::make_shared<UploadJob>(
            _driveDbId, pict5Path, pict5Path.filename().native() + Str2SyncName(std::to_string(i)), _dirId, 0);
        JobManager::instance()->queueAsyncJob(job, i % 2 ? Poco::Thread::PRIO_HIGHEST : Poco::Thread::PRIO_NORMAL);
        Utility::msleep(10);
    }

    Utility::msleep(10000);  // Wait 10sec

    // Don't know how to test it but logs looks good...
}

static const Poco::URI testUri("https://api.kdrive.infomaniak.com/2/drive/102489/files/56850/directory");
void sendTestRequest(Poco::Net::HTTPSClientSession &session, const bool resetSession) {
    bool connected = session.socket().impl()->initialized();
    std::cout << "socket connected: " << connected << std::endl;
    std::cout << "session connected: " << session.connected() << std::endl;

    std::cout << "sending request" << std::endl;
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, testUri.toString(), Poco::Net::HTTPMessage::HTTP_1_1);
    request.setContentLength(0);
    session.sendRequest(request);

    connected = session.socket().impl()->initialized();
    std::cout << "socket connected: " << connected << std::endl;
    std::cout << "socket address: " << session.socket().address().toString().c_str() << std::endl;
    std::cout << "session connected: " << session.connected() << std::endl;

    std::cout << "receiving response" << std::endl;
    Poco::Net::HTTPResponse response;
    session.receiveResponse(response);

    std::cout << "socket connected: " << connected << std::endl;
    std::cout << "session connected: " << session.connected() << std::endl;

    if (resetSession) {
        std::cout << "reset session" << std::endl;
        session.reset();
    }
    std::cout << "*********************" << std::endl;
}

void TestJobManager::testReuseSocket()
{
    Poco::Net::Context::Ptr context = new Poco::Net::Context(
        Poco::Net::Context::TLS_CLIENT_USE, "", "", "",
        Poco::Net::Context::VERIFY_NONE);
    context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
    context->enableSessionCache(true);

    // Session
    Poco::Net::HTTPSClientSession session(testUri.getHost(), testUri.getPort(), context);
    session.setKeepAlive(true);

    std::cout << "***** Test keep connection ***** " << std::endl;
    sendTestRequest(session, false);
    CPPUNIT_ASSERT(session.socket().impl()->initialized());
    sendTestRequest(session, false);        // Doing twice, so we can see in console that the socket is still connected
    CPPUNIT_ASSERT(session.socket().impl()->initialized());

    std::cout << "***** Test with new connection ***** " << std::endl;
    Poco::Net::HTTPSClientSession session1(testUri.getHost(), testUri.getPort(), context);
    sendTestRequest(session1, false);
    CPPUNIT_ASSERT(session1.socket().impl() != session.socket().impl());

    std::cout << "***** Test reset connection ***** " << std::endl;
    sendTestRequest(session, true);
    CPPUNIT_ASSERT(!session.socket().impl()->initialized());
    sendTestRequest(session, true);        // Doing twice, so we can see in console that the socket is not connected anymore
    CPPUNIT_ASSERT(!session.socket().impl()->initialized());
}

}  // namespace KDC

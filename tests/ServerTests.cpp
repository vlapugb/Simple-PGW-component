#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../src/Client/UdpClient.h"
#include "../src/Common/ConfigLoader.h"
#include "../src/Pgw/BlackListStorer.h"
#include "../src/Pgw/CdrWriter.h"
#include "../src/Pgw/SessionManager.h"
#include "../src/Pgw/UdpServer.h"
#include "nlohmann/json.hpp"
#include "spdlog/sinks/null_sink.h"


static std::shared_ptr<spdlog::logger> make_null_logger (const std::string& name = "null") {
    return std::make_shared<spdlog::logger> (name, std::make_shared<spdlog::sinks::null_sink_mt> ());
}

static std::vector<uint8_t> encode_imsi_bcd (std::string imsi) {
    if (imsi.size () % 2 != 0)
        imsi.push_back ('F');
    std::vector<uint8_t> bytes;
    for (std::size_t i = 0; i < imsi.size (); i += 2) {
        const uint8_t low  = (imsi[i] == 'F' ? 0xF : imsi[i] - '0');
        const uint8_t high = (imsi[i + 1] == 'F' ? 0xF : imsi[i + 1] - '0');
        bytes.push_back (static_cast<uint8_t> ((high << 4) | low));
    }
    return bytes;
}

static std::filesystem::path temp_dir () {
    return std::filesystem::temp_directory_path ();
}

TEST (UdpClientTest, GenerateDefaultLength15) {
    const std::string imsi = client::UdpClient::generate_imsi ();
    EXPECT_EQ (imsi.size (), 15);
    for (auto&& c : imsi)
        EXPECT_TRUE (std::isdigit (c));
}

TEST (UdpClientTest, GenerateCustomLength12) {
    const std::string imsi = client::UdpClient::generate_imsi (250, 99, 12);
    EXPECT_EQ (imsi.size (), 12);
}

TEST (UdpClientTest, GenerateInvalidMccThrows) {
    EXPECT_THROW (client::UdpClient::generate_imsi (42, 99), std::invalid_argument);
}

TEST (UdpClientTest, GenerateInvalidTotalLenThrows) {
    EXPECT_THROW (client::UdpClient::generate_imsi (250, 99, 11), std::invalid_argument);
}

TEST (BlackListStorerTest, LookupPositive) {
    const auto logger = make_null_logger ();
    BlackListStorer bl (128, logger);
    const std::unordered_set<std::string> s{ "250990000000001" };
    bl.store (s);
    EXPECT_TRUE (bl.is_in_blacklist ("250990000000001"));
}

TEST (BlackListStorerTest, LookupNegative) {
    auto logger = make_null_logger ();
    BlackListStorer bl (128, logger);
    bl.store ({ "250990000000001" });
    EXPECT_FALSE (bl.is_in_blacklist ("250990000000002"));
}

class SessionManagerFixture : public ::testing::Test {
    protected:
    void SetUp () override {
        Common::ServerSettings s{};
        s.cdr_file = (temp_dir () / "cdr_test.log").string ();
        _writer    = std::make_unique<CdrWriter> (s, make_null_logger ());
        _manager   = std::make_unique<SessionManager> (0 /*timeout*/, *_writer, make_null_logger ());
        _imsi1     = "250991234567890";
        _imsi2     = "250991234567891";
    }

    std::unique_ptr<CdrWriter> _writer;
    std::unique_ptr<SessionManager> _manager;
    std::string _imsi1, _imsi2;
};

TEST_F (SessionManagerFixture, CreateSessionReturnsTrueFirstTime) {
    EXPECT_TRUE (_manager->create_session (_imsi1));
    EXPECT_EQ (_manager->session_count (), 1u);
}

TEST_F (SessionManagerFixture, CreateSessionReturnsFalseOnDuplicate) {
    _manager->create_session (_imsi1);
    EXPECT_FALSE (_manager->create_session (_imsi1));
    EXPECT_EQ (_manager->session_count (), 1u);
}

TEST_F (SessionManagerFixture, RemoveSessionPresent) {
    _manager->create_session (_imsi1);
    EXPECT_TRUE (_manager->remove_session (_imsi1, "manual"));
    EXPECT_EQ (_manager->session_count (), 0u);
}

TEST_F (SessionManagerFixture, RemoveTimeoutImmediately) {
    _manager->create_session (_imsi1);
    _manager->remove_timeout ();
    EXPECT_EQ (_manager->session_count (), 0u);
}

TEST_F (SessionManagerFixture, PopOneRemovesAndReturns) {
    _manager->create_session (_imsi1);
    _manager->create_session (_imsi2);
    std::string victim;
    EXPECT_TRUE (_manager->pop_one (victim));
    EXPECT_TRUE (victim == _imsi1 || victim == _imsi2);
    EXPECT_EQ (_manager->session_count (), 1u);
}

TEST (CdrWriterTest, WriteAndFlushCreatesLine) {
    Common::ServerSettings s{};
    s.cdr_file = (temp_dir () / "cdr_single_test.log").string ();
    {
        CdrWriter writer (s, make_null_logger ());
        writer.write ("250990000000003", "created");
        writer.flush ();
    }
    std::ifstream in (s.cdr_file);
    std::string line;
    std::getline (in, line);
    EXPECT_NE (line.find ("250990000000003"), std::string::npos);
    EXPECT_NE (line.find ("created"), std::string::npos);
}

TEST (UdpServerTest, BcdToImsiEvenDigits) {
    const std::string imsi    = "12345678901234";
    const auto bcd            = encode_imsi_bcd (imsi);
    const std::string decoded = Pgw::UdpServer::bcd_to_imsi (bcd.data (), bcd.size ());
    EXPECT_EQ (decoded, imsi);
}

TEST (UdpServerTest, BcdToImsiOddDigits) {
    const std::string imsi    = "123456789012345";
    const auto bcd            = encode_imsi_bcd (imsi);
    const std::string decoded = Pgw::UdpServer::bcd_to_imsi (bcd.data (), bcd.size ());
    EXPECT_EQ (decoded, imsi);
}

TEST (SettingsLoaderTest, LoadServerSettingsFromJson) {
    Common::ServerSettings reference{};
    reference.udp_ip                 = "0.0.0.0";
    reference.udp_port               = 9000;
    reference.session_timeout_sec    = 10;
    reference.cdr_file               = "test_cdr.log";
    reference.http_port              = 8081;
    reference.graceful_shutdown_rate = 5;
    reference.log_file               = "test_pgw.log";
    reference.log_level              = spdlog::level::info;
    reference.blacklist              = { "001010111111111", "001010222222222" };

    nlohmann::json j = { { "udp_ip", reference.udp_ip }, { "udp_port", reference.udp_port },
        { "session_timeout_sec", reference.session_timeout_sec }, { "cdr_file", reference.cdr_file },
        { "http_port", reference.http_port }, { "graceful_shutdown_rate", reference.graceful_shutdown_rate },
        { "log_file", reference.log_file }, { "log_level", "info" },
        { "blacklist", nlohmann::json::array ({ "001010111111111", "001010222222222" }) } };

    auto cfg_path = temp_dir () / "server_settings.json";
    std::ofstream (cfg_path) << j.dump ();

    setenv ("SERVER_CFG", cfg_path.c_str (), 1);
    const auto [udp_ip, udp_port, session_timeout_sec, cdr_file, http_port, graceful_shutdown_rate, log_file, log_level,
    blacklist] = Common::SettingsLoader::load<Common::ServerSettings> ("SERVER_CFG");

    EXPECT_EQ (udp_ip, reference.udp_ip);
    EXPECT_EQ (udp_port, reference.udp_port);
    EXPECT_EQ (session_timeout_sec, reference.session_timeout_sec);
    EXPECT_EQ (cdr_file, reference.cdr_file);
    EXPECT_EQ (http_port, reference.http_port);
    EXPECT_EQ (graceful_shutdown_rate, reference.graceful_shutdown_rate);
    EXPECT_EQ (log_file, reference.log_file);
    EXPECT_EQ (log_level, reference.log_level);
    EXPECT_EQ (blacklist, reference.blacklist);
}

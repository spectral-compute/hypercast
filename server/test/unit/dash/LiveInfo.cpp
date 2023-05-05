#include "configuration/configuration.hpp"

#include "server/Path.hpp"

#include <gtest/gtest.h>

namespace Dash
{

std::string getLiveInfo(const Config::Channel &config, const Server::Path &uidPath);

} // namespace Dash

TEST(DashLiveInfo, Simple)
{
    std::string infoJson = Dash::getLiveInfo({
        .qualities = {
            {
                .video = {
                    .width = 1920,
                    .height = 1080,
                    .bitrate = 2048
                },
                .audio = {
                    .sampleRate = 48000
                },
                .clientBufferControl = {
                    .minBuffer = 500,
                    .extraBuffer = 180,
                    .initialBuffer = 1000,
                    .seekBuffer = 250,
                    .minimumInitTime = 2000
                }
            }
        }
    }, "/live/test");
    EXPECT_EQ("{\"audioConfigs\":[{\"bitrate\":64,\"codec\":\"aac\"}],\"avMap\":[[0,1]],\"segmentDuration\":15000,"
              "\"segmentPreavailability\":4000,\"videoConfigs\":[{\"bitrate\":2048,"
              "\"bufferCtrl\":{\"extraBuffer\":180,\"initialBuffer\":1000,\"minBuffer\":500,"
                              "\"minimumInitTime\":2000,"
              "\"seekBuffer\":250},\"codec\":\"h264\",\"height\":1080,\"width\":1920}]}", infoJson);
}

TEST(DashLiveInfo, Complex)
{
    std::string infoJson = Dash::getLiveInfo({
        .qualities = {
            {
                .video = {
                    .width = 1920,
                    .height = 1080,
                    .bitrate = 2048
                },
                .audio = {
                    .sampleRate = 48000
                },
                .clientBufferControl = {
                    .minBuffer = 500,
                    .extraBuffer = 180,
                    .initialBuffer = 1000,
                    .seekBuffer = 250,
                    .minimumInitTime = 2000
                }
            },
            {
                .video = {
                    .width = 1280,
                    .height = 720,
                    .bitrate = 1024
                },
                .clientBufferControl = {
                    .minBuffer = 1000,
                    .extraBuffer = 360,
                    .initialBuffer = 2000,
                    .seekBuffer = 500,
                    .minimumInitTime = 4000
                }
            },
            {
                .video = {
                    .width = 640,
                    .height = 360,
                    .bitrate = 512
                },
                .audio = {
                    .sampleRate = 48000
                },
                .clientBufferControl = {
                    .minBuffer = 2000,
                    .extraBuffer = 500,
                    .initialBuffer = 2000,
                    .seekBuffer = 500,
                    .minimumInitTime = 4000
                }
            }
        }
    }, "/live/test");
    EXPECT_EQ("{\"audioConfigs\":[{\"bitrate\":64,\"codec\":\"aac\"},{\"bitrate\":64,\"codec\":\"aac\"}],"
              "\"avMap\":[[0,3],[1,null],[2,4]],\"segmentDuration\":15000,\"segmentPreavailability\":4000,"
              "\"videoConfigs\":[{\"bitrate\":2048,"
                                 "\"bufferCtrl\":{\"extraBuffer\":180,\"initialBuffer\":1000,\"minBuffer\":500,"
                                                 "\"minimumInitTime\":2000,\"seekBuffer\":250},"
                                 "\"codec\":\"h264\",\"height\":1080,\"width\":1920},"
                                "{\"bitrate\":1024,"
                                 "\"bufferCtrl\":{\"extraBuffer\":360,\"initialBuffer\":2000,\"minBuffer\":1000,"
                                                 "\"minimumInitTime\":4000,\"seekBuffer\":500},"
                                 "\"codec\":\"h264\",\"height\":720,\"width\":1280},"
                                "{\"bitrate\":512,"
                                 "\"bufferCtrl\":{\"extraBuffer\":500,\"initialBuffer\":2000,\"minBuffer\":2000,"
                                                 "\"minimumInitTime\":4000,\"seekBuffer\":500},"
                                 "\"codec\":\"h264\",\"height\":360,\"width\":640}]}", infoJson);
}

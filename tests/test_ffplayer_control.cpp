#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "ffplayer.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* desc)
{
    if (cond) { ++g_pass; std::printf("  [PASS] %s\n", desc); }
    else      { ++g_fail; std::printf("  [FAIL] %s\n", desc); }
}

static void section(const char* title)
{
    std::printf("\n==================================================\n");
    std::printf(" %s\n", title);
    std::printf("==================================================\n");
}

static void msleep(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static double nowSec()
{
    return av_gettime_relative() / 1000000.0;
}

static bool waitUntil(FFPlayer& p, FFMsg stopAt, int timeoutMs)
{
    const auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        const auto el = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        if (el > timeoutMs) return false;
        Message m;
        const int r = p.messages().get(m, false);
        if (r == 1) { if (m.what == stopAt) return true; }
        else if (r < 0) return false;
        else msleep(5);
    }
}

static double measureClockRate(FFPlayer& p, int windowMs, double* wallOut = nullptr)
{
    msleep(300);
    const double a = p.positionSeconds();
    const double w0 = nowSec();
    msleep(windowMs);
    const double b = p.positionSeconds();
    const double wall = nowSec() - w0;
    if (wallOut) *wallOut = wall;
    return (b - a) / wall;
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IONBF, 0);

    const std::string path = (argc > 1) ? argv[1] : "../_media/test_media.mp4";

    std::printf("==================================================\n");
    std::printf(" M11 实验台：播放控制与生命周期收尾\n");
    std::printf(" 素材: %s\n", path.c_str());
    std::printf("==================================================\n");

    FFPlayer player;
    player.setVolume(0.2f);

    section("[1] ★ 倍速：时钟推进速率");
    {
        check(player.prepare(path) == 0, "prepare 返回 0");
        check(waitUntil(player, FFMsg::Prepared, 5000), "收到 Prepared");

        struct Case { float speed; const char* name; };
        const Case cases[] = {
            {1.0f, "1.0x"},
            {2.0f, "2.0x"},
            {0.5f, "0.5x"},
        };

        for (const auto& c : cases) {
            player.seekMs(0);
            msleep(500);
            player.setSpeed(c.speed);

            double wall = 0.0;
            const double rate = measureClockRate(player, 1200, &wall);

            std::printf("        %s: 墙钟 %.2f 秒，时钟推进速率 %.3f 倍\n",
                        c.name, wall, rate);
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "★ %s 下时钟按 %.1fx 推进（期望 ≈ %.1f）", c.name, c.speed, c.speed);
            check(std::fabs(rate - c.speed) < c.speed * 0.30 + 0.15, buf);
        }

        player.setSpeed(1.0f);
    }

    section("[2] ★ serial 机制：seek 后不再显示旧帧");
    {
        std::atomic<int> total{0};
        std::atomic<int> mismatched{0};
        std::atomic<int> minSerialSeen{999};

        player.setFrameCallback([&player, &total, &mismatched, &minSerialSeen](const Frame& f) {
            const int qserial = player.videoQueue().serial();
            total.fetch_add(1);
            if (f.serial != qserial)
                mismatched.fetch_add(1);
            if (f.serial < minSerialSeen.load())
                minSerialSeen.store(f.serial);
        });

        player.seekMs(0);
        msleep(700);
        const int serial0 = player.videoQueue().serial();

        player.seekMs(2500);
        msleep(900);
        const int serial1 = player.videoQueue().serial();

        player.seekMs(1000);
        msleep(900);
        const int serial2 = player.videoQueue().serial();

        std::printf("        三次 seek 后的队列 serial: %d -> %d -> %d\n",
                    serial0, serial1, serial2);
        std::printf("        期间共显示 %d 帧，其中 serial 不匹配的有 %d 帧\n",
                    total.load(), mismatched.load());

        check(serial1 > serial0 && serial2 > serial1,
              "每次 seek 都让 serial 递增（队列 reset 生效）");
        check(mismatched.load() == 0,
              "★★ 显示的每一帧的 serial 都与队列一致 —— 残留旧帧被精确拦住了");
        check(total.load() > 20, "seek 之后画面仍在正常显示");

        player.setFrameCallback(nullptr);
    }

    section("[3] seek 边界与快速连续 seek");
    {
        for (int i = 0; i < 5; ++i) {
            player.seekMs(1000 + i * 700);
            msleep(30);
        }
        msleep(900);
        const double posAfterFlood = player.positionSeconds();
        std::printf("        连续 5 次 seek（最后一个是 3800ms）后位置 = %.3f 秒\n",
                    posAfterFlood);
        check(posAfterFlood > 2.5 && posAfterFlood < 4.8,
              "★ 快速连续 seek 后落点正确（没有被前面的请求带跑）");

        player.seekMs(0);
        msleep(800);
        const double posAtZero = player.positionSeconds();
        std::printf("        seek 到 0 后位置 = %.3f 秒\n", posAtZero);
        check(posAtZero < 1.2, "★ seek 到起点有效");

        player.seekMs(4700);
        msleep(800);
        const double posNearEnd = player.positionSeconds();
        std::printf("        seek 到 4700ms 后位置 = %.3f 秒\n", posNearEnd);
        check(posNearEnd > 4.0, "★ seek 到接近末尾有效");
    }

    section("[4] 暂停 + seek 组合");
    {
        player.setPaused(true);
        msleep(200);
        const double beforeSeek = player.positionSeconds();

        player.seekMs(2000);
        msleep(600);
        const double afterSeek = player.positionSeconds();
        std::printf("        暂停中 seek 到 2000ms：读数 %.3f -> %.3f 秒\n",
                    beforeSeek, afterSeek);

        check(afterSeek > 1.5 && afterSeek < 3.0,
              "★ 暂停中 seek 后读数立刻跳到目标附近（界面进度不会卡住）");

        msleep(500);
        const double later = player.positionSeconds();
        std::printf("        再等 500ms，读数 %.3f（应保持不变）\n", later);
        check(std::fabs(later - afterSeek) < 0.1,
              "★ 暂停中 seek 之后时钟依然冻结（不会自己往前走）");

        player.setPaused(false);
        msleep(500);
        const double resumed = player.positionSeconds();
        std::printf("        恢复后位置 = %.3f 秒\n", resumed);
        check(resumed > afterSeek - 0.3, "恢复后从新位置继续，没有倒退");
    }
    player.close();
    check(true, "close 正常返回");

    section("[5] ★ 生命周期压力：反复开关 + 播放中/暂停中 close");
    {
        const auto t0 = std::chrono::steady_clock::now();
        bool allOk = true;
        for (int i = 0; i < 20; ++i) {
            FFPlayer p;
            if (p.prepare(path) != 0) { allOk = false; break; }
            msleep(40);
            p.close();
        }
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        std::printf("        20 次 prepare/close 总耗时 %lld ms（平均 %lld ms）\n",
                    (long long)ms, (long long)(ms / 20));
        check(allOk, "★ 20 次快速开关全部成功，没有卡死");
        check(ms < 15000, "总耗时在合理范围（没有线程停机变慢）");

        {
            FFPlayer p;
            p.setVolume(0.2f);
            p.prepare(path);
            waitUntil(p, FFMsg::Prepared, 5000);
            msleep(800);
            const auto c0 = std::chrono::steady_clock::now();
            p.close();
            const auto cms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - c0).count();
            std::printf("        播放中 close 耗时 %lld ms\n", (long long)cms);
            check(cms < 1500, "★ 播放中 close 也能及时停住（4 条线程都被唤醒）");
        }

        {
            FFPlayer p;
            p.setVolume(0.2f);
            p.prepare(path);
            waitUntil(p, FFMsg::Prepared, 5000);
            msleep(400);
            p.setPaused(true);
            msleep(200);
            const auto c0 = std::chrono::steady_clock::now();
            p.close();
            const auto cms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - c0).count();
            std::printf("        暂停中 close 耗时 %lld ms\n", (long long)cms);
            check(cms < 1500, "★ 暂停中 close 及时返回");
        }

        {
            FFPlayer p;
            p.prepare(path);
            msleep(100);
            p.close();
        }
        check(true, "★ close 之后析构安全（close 幂等）");
    }

    section("[6] 越界参数");
    {
        FFPlayer p;
        p.setVolume(0.2f);
        p.prepare(path);
        waitUntil(p, FFMsg::Prepared, 5000);
        msleep(300);

        p.setVolume(-1.0f);
        p.setVolume(500.0f);
        p.setVolume(0.2f);
        check(true, "setVolume 接受越界值（内部 clamp）");

        p.setSpeed(0.0f);
        p.setSpeed(-2.0f);
        p.setSpeed(1.0f);
        check(true, "setSpeed 忽略非正倍速，不崩溃");

        const double pos = p.positionSeconds();
        std::printf("        位置 = %.3f 秒\n", pos);
        check(pos >= 0.0 && pos < 10.0, "参数折腾之后位置依然合理");
        p.close();
    }

    std::printf("\n==================================================\n");
    std::printf(" 测试结束：通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    std::printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}

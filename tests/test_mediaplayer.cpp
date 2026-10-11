#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QThread>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "mediaplayer.h"

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

static bool pumpUntil(const std::function<bool()>& pred, int timeoutMs)
{
    const auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (pred())
            return true;
        const auto el = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        if (el > timeoutMs)
            return false;
        msleep(3);
    }
}

struct Recorder {
    QThread* guiThread = nullptr;

    std::atomic<int> wrongThreadSeen{0};
    std::atomic<int> frameCount{0};
    std::atomic<int> posCount{0};
    std::atomic<int> durCount{0};
    std::atomic<bool> gotPrepared{false};
    std::atomic<bool> gotCompleted{false};
    std::atomic<bool> gotError{false};

    std::vector<PlaybackState> states;
    std::vector<QString> errors;
    int  badSizeFrames = 0;
    int  blankFrames   = 0;
    int  firstW = 0, firstH = 0;
    qint64 lastDuration = -1;
    qint64 lastPosition = -1;

    void noteThread() {
        if (QThread::currentThread() != guiThread)
            wrongThreadSeen.fetch_add(1);
    }
};

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IONBF, 0);

    QCoreApplication app(argc, argv);

    const std::string path = (argc > 1) ? argv[1] : "../_media/test_media.mp4";

    std::printf("==================================================\n");
    std::printf(" M12 实验台：MediaPlayer 门面层（Qt 世界的第一层）\n");
    std::printf(" 素材: %s\n", path.c_str());
    std::printf("==================================================\n");

    Recorder rec;
    rec.guiThread = QThread::currentThread();
    std::printf("  GUI 线程 = %p\n", static_cast<void*>(rec.guiThread));

    MediaPlayer mp;
    mp.setVolume(20);

    QObject guiContext;
    std::printf("  GUI 线程 = %p（guiContext 也活在这个线程）\n",
                static_cast<void*>(rec.guiThread));

    QObject::connect(&mp, &MediaPlayer::stateChanged, &guiContext, [&rec](PlaybackState s) {
        rec.noteThread();
        rec.states.push_back(s);
    });
    QObject::connect(&mp, &MediaPlayer::prepared, &guiContext, [&rec] {
        rec.noteThread();
        rec.gotPrepared = true;
    });
    QObject::connect(&mp, &MediaPlayer::completed, &guiContext, [&rec] {
        rec.noteThread();
        rec.gotCompleted = true;
    });
    QObject::connect(&mp, &MediaPlayer::errorOccurred, &guiContext,
                     [&rec](const QString& e) {
        rec.noteThread();
        rec.gotError = true;
        rec.errors.push_back(e);
    });
    QObject::connect(&mp, &MediaPlayer::frameReady, &guiContext,
                     [&rec](const QImage& img) {
        rec.noteThread();
        rec.frameCount.fetch_add(1);

        if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
            ++rec.badSizeFrames;
            return;
        }
        if (rec.firstW == 0) { rec.firstW = img.width(); rec.firstH = img.height(); }
        if (img.width() != 640 || img.height() != 480)
            ++rec.badSizeFrames;

        int bright = 0;
        for (int y = 0; y < img.height(); y += 20)
            for (int x = 0; x < img.width(); x += 20) {
                const QRgb p = img.pixel(x, y);
                if (qRed(p) + qGreen(p) + qBlue(p) > 40) ++bright;
            }
        if (bright == 0) ++rec.blankFrames;
    });
    QObject::connect(&mp, &MediaPlayer::positionChanged, &guiContext, [&rec](qint64 ms) {
        rec.noteThread();
        rec.posCount.fetch_add(1);
        rec.lastPosition = ms;
    });
    QObject::connect(&mp, &MediaPlayer::durationChanged, &guiContext, [&rec](qint64 ms) {
        rec.noteThread();
        rec.durCount.fetch_add(1);
        rec.lastDuration = ms;
    });

    section("[1] 信号序列与状态迁移");
    {
        mp.setDataSource(QString::fromUtf8(path.c_str()));
        mp.play();

        const bool prepared = pumpUntil([&rec] { return rec.gotPrepared.load(); }, 6000);
        check(prepared, "收到 prepared 信号");
        check(rec.states.size() >= 2
              && rec.states[0] == PlaybackState::Preparing
              && rec.states[1] == PlaybackState::Playing,
              "★ 状态迁移序列正确：Preparing -> Playing");
        std::printf("        stateChanged 序列前 3 个: ");
        for (size_t i = 0; i < rec.states.size() && i < 3; ++i)
            std::printf("%d ", static_cast<int>(rec.states[i]));
        std::printf("\n");
    }

    section("[2] ★ frameReady：QImage 的数量 / 尺寸 / 内容");
    {
        pumpUntil([&rec] { return rec.frameCount.load() > 60; }, 4000);

        std::printf("        已收到 %d 帧 QImage（首帧 %dx%d）\n",
                    rec.frameCount.load(), rec.firstW, rec.firstH);
        check(rec.frameCount.load() > 40, "★ 持续收到视频帧");
        check(rec.badSizeFrames == 0, "★ 每帧尺寸都是 640x480（转换结果正确）");
        check(rec.blankFrames == 0,
              "★ 没有全黑的帧 —— 说明 RGB 数据确实被拷进 QImage 了（不是空指针包装）");
    }

    section("[3] ★★ 线程归属：信号必须在 GUI 线程被投递");
    {
        std::printf("        在非 GUI 线程收到信号的次数: %d\n", rec.wrongThreadSeen.load());
        check(rec.wrongThreadSeen.load() == 0,
              "★★ 所有信号的槽都在 GUI 线程执行 —— 跨线程投递机制生效");
        std::printf("        （frameReady 是在内核刷新线程 emit 的，\n");
        std::printf("          stateChanged 是在事件线程里 invokeMethod 出来的，\n");
        std::printf("          两者如果没有正确切线程，这里就会 > 0）\n");
    }

    section("[4] 位置与时长上报");
    {
        std::printf("        位置信号 %d 次（最后一次 %lld ms），时长信号 %d 次（%lld ms）\n",
                    rec.posCount.load(), (long long)rec.lastPosition,
                    rec.durCount.load(), (long long)rec.lastDuration);
        check(rec.posCount.load() > 2, "周期性收到 positionChanged（200ms 定时器在工作）");
        check(rec.lastDuration > 4000 && rec.lastDuration < 6000,
              "★ durationChanged 上报的时长约 5 秒");
        check(rec.lastPosition > 0, "位置在推进");
    }

    section("[5] ★★ 重复播放（测 teardown 修掉的那处竞态）");
    {
        mp.seek(4600);
        pumpUntil([&rec] { return rec.gotCompleted.load(); }, 5000);
        rec.gotCompleted = false;

        int ok = 0;
        for (int i = 0; i < 12; ++i) {
            rec.gotPrepared  = false;
            rec.gotCompleted = false;

            mp.play();
            if (!pumpUntil([&rec] { return rec.gotPrepared.load(); }, 4000))
                break;

            mp.seek(4600);
            if (!pumpUntil([&rec] { return rec.gotCompleted.load(); }, 5000))
                break;

            ++ok;
            if ((i + 1) % 4 == 0 || i == 0)
                std::printf("          第 %2d 轮完成（帧总数 %d）\n", i + 1, rec.frameCount.load());
        }
        check(ok == 12, "★★ 连续 12 轮 play->完成 全部成功，没有崩溃或挂死");
        std::printf("        （参考工程在这条路径上有 use-after-free 风险：\n");
        std::printf("           play() 先析构旧 FFPlayer，后 join 事件线程）\n");
    }

    section("[6] 错误路径");
    {
        mp.stop();
        rec.gotError = false;
        rec.errors.clear();
        mp.setDataSource(QStringLiteral("no_such_file_12345.mp4"));
        mp.play();

        const bool gotErr = pumpUntil([&rec] { return rec.gotError.load(); }, 4000);
        check(gotErr, "★ 打开不存在的文件会发出 errorOccurred 信号");
        if (!rec.errors.empty())
            std::printf("        错误内容: %s\n", rec.errors[0].toUtf8().constData());
        check(mp.state() == PlaybackState::Error, "状态被置为 Error");
    }

    section("[7] stop 回到 Idle");
    {
        mp.stop();
        check(mp.state() == PlaybackState::Idle, "stop 之后状态是 Idle");
        check(!mp.isPlaying(), "isPlaying() 为 false");

        mp.setDataSource(QString::fromUtf8(path.c_str()));
        rec.gotPrepared = false;
        mp.play();
        check(pumpUntil([&rec] { return rec.gotPrepared.load(); }, 5000),
              "stop 之后还能重新播放");
        mp.stop();
        check(true, "再次 stop 安全");
    }

    std::printf("\n==================================================\n");
    std::printf(" 测试结束：通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    std::printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}

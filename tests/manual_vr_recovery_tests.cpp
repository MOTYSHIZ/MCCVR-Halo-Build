#include "../src/common/manual_vr_recovery_logic.h"
#include <cstdio>
#include <initializer_list>

int main()
{
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    for (GameTitle title : {GameTitle::HaloCE, GameTitle::Halo2, GameTitle::Halo3,
            GameTitle::Halo3ODST, GameTitle::HaloReach, GameTitle::Halo4})
    {
        const auto token = ManualVrRecoveryToken(title, 17);
        check(ManualVrRecoveryMatches(token, title, 17), "each game accepts its exact epoch");
        check(!ManualVrRecoveryMatches(token, title, 18), "reload cannot inherit recovery");
        for (GameTitle other : {GameTitle::HaloCE, GameTitle::Halo2, GameTitle::Halo3,
                GameTitle::Halo3ODST, GameTitle::HaloReach, GameTitle::Halo4})
            check(ManualVrRecoveryMatches(token, other, 17) == (other == title),
                "equal generation numbers cannot transfer override between titles");
    }
    check(!ManualVrRecoveryToken(GameTitle::None, 17) &&
          !ManualVrRecoveryToken(GameTitle::Unknown, 17) &&
          !ManualVrRecoveryToken(GameTitle::Halo3, 0), "shell, ambiguity and missing epoch cannot force hooks");
    uint32_t request = 17;
    int retires = 0, resets = 0;
    bool cleanupComplete = false;
    auto retire = [&] { ++retires; return cleanupComplete; };
    auto reset = [&] { ++resets; };
    check(PollManualVrRecovery(request, 17, true, false, retire, reset) == ManualVrRecoveryPoll::Waiting &&
        request == 17 && retires == 0 && resets == 0, "loading preserves explicit retry until module proof is ready");
    check(PollManualVrRecovery(request, 17, true, true, retire, reset) == ManualVrRecoveryPoll::Waiting &&
        request == 17 && resets == 0, "in-flight callback retains retry and blocks install reset");
    cleanupComplete = true;
    check(PollManualVrRecovery(request, 17, true, true, retire, reset) == ManualVrRecoveryPoll::Retired &&
        request == 0 && retires == 2 && resets == 1, "quiescent retry clears latch exactly once");
    check(PollManualVrRecovery(request, 17, true, true, retire, reset) == ManualVrRecoveryPoll::None &&
        retires == 2 && resets == 1, "ordinary next poll leaves working core alone");
    request = 17;
    check(PollManualVrRecovery(request, 18, true, true, retire, reset) == ManualVrRecoveryPoll::None &&
        request == 0 && retires == 2 && resets == 1, "stale request never retires replacement core");
    request = 17;
    check(PollManualVrRecovery(request, 17, false, true, retire, reset) == ManualVrRecoveryPoll::None &&
        request == 0 && retires == 2 && resets == 1, "title exit or runtime failure cannot be bypassed");
    return failures ? 1 : 0;
}

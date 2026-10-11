// How a look-steered driver steers (src/common/vehicle_steering_logic.h): the choice of hand, stick or wheel
// apart from how the view turns, and the wheel handed over when it is the chosen way.
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../src/common/vehicle_steering_logic.h"

using namespace vehicle_steering;

static unsigned g_checks = 0;
static void Check(bool condition, const char* message)
{
    ++g_checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void TestWhoSteers()
{
    // Automatic is the rule the mod had before the choice: it must agree with it seat for seat.
    for (const bool follows : {false, true})
    {
        Check(Authored(Mode::Automatic, true, follows) ==
                  Halo3SeatAuthorsSteering(Halo3VehicleId::Warthog, 0, false, follows),
              "automatic steers a look-steered driver as before: the stick or wheel when the view follows");
        Check(Authored(Mode::Automatic, false, follows) ==
                  Halo3SeatAuthorsSteering(Halo3VehicleId::Scorpion, 0, false, follows),
              "and leaves a tank driver's turret to the hand as before");
    }
    Check(!Authored(Mode::Automatic, true, false) && Authored(Mode::Automatic, true, true),
          "automatic: the hand in a still view, the stick or wheel in a following one");

    for (const bool follows : {false, true})
    {
        Check(!Authored(Mode::Motion, true, follows), "motion: the hand steers, however the view turns");
        Check(Authored(Mode::Stick, true, follows), "stick: the stick steers, however the view turns");
        Check(Authored(Mode::Wheel, true, follows), "wheel: the wheel or stick steers, however the view turns");
        for (const Mode mode : {Mode::Automatic, Mode::Motion, Mode::Stick, Mode::Wheel})
            Check(!Authored(mode, false, follows), "no mode steers a seat that is not a look-steered driver's");
    }

    Check(FromConfig(0) == Mode::Automatic && FromConfig(1) == Mode::Motion && FromConfig(2) == Mode::Stick &&
              FromConfig(3) == Mode::Wheel,
          "the config's numbers");
    Check(FromConfig(-1) == Mode::Automatic && FromConfig(4) == Mode::Automatic && FromConfig(999) == Mode::Automatic,
          "a number that names no mode is automatic");
}

static void TestWheelOnOffer()
{
    Halo3Wheel wheel;
    WheelOffer offer;
    Check(!TakeWheelOnOffer(wheel, offer, false, true) && !wheel.engaged, "no wheel is taken in a seat that offers none");
    Check(!TakeWheelOnOffer(wheel, offer, true, false) && !wheel.engaged, "nor when the wheel is not the chosen way");

    Check(TakeWheelOnOffer(wheel, offer, true, true) && wheel.engaged, "the chosen wheel is taken when the seat offers it");
    Check(!TakeWheelOnOffer(wheel, offer, true, true) && wheel.engaged, "once: the next poll takes nothing more");

    // The player lets go with the double-click; the offer is spent, so the wheel stays let go.
    auto click = [&wheel](uint64_t at) {
        Halo3UpdateWheelToggle(wheel, true, 0.9f, 0.9f, at);
        return Halo3UpdateWheelToggle(wheel, true, 0.0f, 0.0f, at + 5);
    };
    click(1000);
    Check(click(1100) && !wheel.engaged, "a double-click lets go of it");
    Check(!TakeWheelOnOffer(wheel, offer, true, true) && !wheel.engaged, "and it is not handed straight back");
    click(2000);
    Check(click(2100) && wheel.engaged, "a double-click takes it again, as before");

    // Leaving the seat ends the offer; the next seat makes a new one.
    Halo3UpdateWheelToggle(wheel, false, 0.0f, 0.0f, 3000);
    Check(!wheel.engaged && !TakeWheelOnOffer(wheel, offer, false, true), "leaving the seat lets go");
    Check(TakeWheelOnOffer(wheel, offer, true, true) && wheel.engaged, "and the next seat hands the wheel over again");

    // Choosing another way to steer ends the offer too.
    Check(!TakeWheelOnOffer(wheel, offer, true, false), "another way chosen takes nothing");
    wheel.engaged = false;
    Check(TakeWheelOnOffer(wheel, offer, true, true) && wheel.engaged, "and choosing the wheel again hands it over");

    // A wheel already in hand is left as it is, and the offer is spent all the same.
    Halo3Wheel held;
    held.engaged = true;
    WheelOffer fresh;
    Check(!TakeWheelOnOffer(held, fresh, true, true) && held.engaged && fresh.made, "a wheel already held is not taken twice");
}

int main()
{
    TestWhoSteers();
    TestWheelOnOffer();
    std::printf("PASS: %u vehicle steering checks\n", g_checks);
    return 0;
}

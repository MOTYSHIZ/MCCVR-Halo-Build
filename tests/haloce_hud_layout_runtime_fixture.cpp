// Initialize only native-hook admission for the production runtime test.
// All numeric raster observation/replay/restoration remains production code.
#include "../src/dll/haloce_hud_layout.cpp"

void ConfigureCeHudLayoutRuntimeFixture(uint32_t gen,bool enabledForTest,bool installedForTest)
{
    installed=enabledForTest&&installedForTest; active=enabledForTest; retiring=false;
    generation=gen; observationAvailable=enabledForTest;
}

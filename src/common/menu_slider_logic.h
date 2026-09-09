#pragma once
#include <algorithm>
#include <cmath>

// One click changes the last displayed digit, including sliders whose menu
// value is a percentage or another conversion of the saved setting.
inline double MenuSliderStep(const char* format) noexcept
{
    if(!format) return 0.001;
    for(const char* p=format; *p; ++p)
    {
        if(*p!='%') continue;
        if(p[1]=='%') { ++p; continue; }
        for(++p; *p && *p!='f' && *p!='F' && *p!='d' && *p!='i'; ++p)
        {
            if(*p!='.') continue;
            unsigned precision=0;
            for(++p; *p>='0' && *p<='9'; ++p)
                precision=std::min(6u,precision*10+unsigned(*p-'0'));
            double step=1;
            while(precision--) step*=0.1;
            return step;
        }
        return 1.0;
    }
    return 1.0;
}

inline float MenuSliderNudge(float value, float minimum, float maximum,
    double step, int direction) noexcept
{
    if(!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum) ||
        minimum>maximum || !std::isfinite(step) || step<=0 || !direction)
        return value;
    const double tick=std::round(double(value)/step)+(direction>0 ? 1 : -1);
    return float(std::clamp(tick*step,double(minimum),double(maximum)));
}

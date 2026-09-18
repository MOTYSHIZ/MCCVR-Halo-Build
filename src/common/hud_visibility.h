#pragma once

// Owned native gameplay HUD callbacks still execute their complete engine
// work. Only their GPU draws are omitted. Never use this around menu or world
// rendering. Nesting and exception cleanup belong to each native callback.
namespace hud_visibility
{
inline thread_local unsigned depth=0;
inline bool Hidden() noexcept { return depth!=0; }
}

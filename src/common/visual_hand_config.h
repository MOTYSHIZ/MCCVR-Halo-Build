#pragma once
#include "config.h"
#include "visual_hand_offset.h"
inline visual_hand::Offset VisualLeftHandOffset(const Config& config) noexcept
{return {config.left_hand_mesh_x_m,config.left_hand_mesh_y_m,config.left_hand_mesh_z_m};}
inline visual_hand::Offset VisualRightHandOffset(const Config& config) noexcept
{return {config.right_hand_mesh_x_m,config.right_hand_mesh_y_m,config.right_hand_mesh_z_m};}
inline bool VisualHandOffsetsActive(const Config& config) noexcept
{return visual_hand::Active(VisualLeftHandOffset(config))||visual_hand::Active(VisualRightHandOffset(config));}

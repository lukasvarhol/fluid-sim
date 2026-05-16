#pragma once

enum class SDFType { Box, Capsule, Cylinder };

struct SDFCollider {
  SDFType type;
  float restitution;

  

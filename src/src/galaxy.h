#ifndef GOURCE_GALAXY_H
#define GOURCE_GALAXY_H

#include "core/display.h"
#include "core/vectors.h"

class RDirNode;

// Galaxy build: dirs are pulled onto logarithmic spiral arms around the root
// and the background is a procedural nebula / spiral galaxy that uses the very
// same arm parameters, so the arms drawn behind the tree line up with the tree.
struct GalaxyConfig {
    bool  layout;      // pull directories onto spiral arms
    bool  nebula;      // draw the nebula background
    int   arms;        // number of spiral arms
    float twist;       // radians of winding per e-fold of radius
    float r0;          // world radius where the arms start winding
    float pull;        // strength of the pull onto an arm
    float width;       // half width of an arm (radians); no pull inside it
    float spin;        // arm rotation speed (radians per second)
    float brightness;  // nebula brightness multiplier
    float time;        // accumulated simulation time
    float yaw;         // camera orbit around the galaxy, degrees
    float pitch;       // camera tilt away from top-down, degrees

    GalaxyConfig();
};

extern GalaxyConfig gGalaxy;

// angle of arm number 'arm' (0..arms-1, may be fractional) at world radius r
float galaxyArmAngle(float arm, float r);

// extra acceleration for a directory at 'pos' that belongs to top level dir 'top'
vec2 galaxyForce(const RDirNode* top, int depth, const vec2& centre, const vec2& pos);

class GalaxyBackground {
    unsigned int program;
    bool failed;

    vec3  cam, right, up, fwd;   // camera in world space
    vec2  tanhalf;               // tan of half the field of view, x and y
    vec2  gc;                    // galaxy centre in the world plane
    float radius;                // galaxy radius, world units
    bool  projected;

    bool compile();
public:
    GalaxyBackground();

    // call once per frame after the camera is set up
    void project(const vec2& root_pos, const GLdouble* modelview, const GLdouble* projection);

    // world radius of the visible structure
    void setRadius(float r);

    void draw(int width, int height);
};

extern GalaxyBackground gGalaxyBackground;

#endif

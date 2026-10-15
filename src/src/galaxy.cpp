#include "galaxy.h"
#include "dirnode.h"

#include <map>
#include <string>
#include <cmath>
#include <cstdio>

GalaxyConfig gGalaxy;
GalaxyBackground gGalaxyBackground;

GalaxyConfig::GalaxyConfig() {
    layout     = true;
    nebula     = true;
    arms       = 4;
    twist      = 2.0f;
    r0         = 90.0f;
    pull       = 1.5f;
    width      = 0.28f;
    spin       = 0.0f;
    brightness = 1.0f;
    time       = 0.0f;
}

//---------------------------------------------------------------- layout

static const float TWO_PI = 6.28318530718f;
static const float RING_SPACING = 80.0f; // world units of arm radius per level of directory depth

float galaxyArmAngle(float arm, float r) {
    return arm * (TWO_PI / (float) gGalaxy.arms)
         + gGalaxy.twist * logf(1.0f + r / gGalaxy.r0)
         + gGalaxy.spin * gGalaxy.time;
}

static float hash01(const std::string& s) {
    unsigned int h = 2166136261u;
    for(size_t i = 0; i < s.size(); i++) {
        h ^= (unsigned char) s[i];
        h *= 16777619u;
    }
    return (float)(h & 0xffffff) / (float) 0x1000000;
}

// each top level directory is dealt out to the arms in turn, with a little
// sideways scatter so that dirs sharing an arm do not stack on one line
static float armOf(const RDirNode* top) {
    static std::map<std::string, float> assigned;
    static int next_arm = 0;

    const std::string& key = top->getPath();

    std::map<std::string, float>::iterator it = assigned.find(key);
    if(it != assigned.end()) return it->second;

    float scatter = (hash01(key) - 0.5f) * 2.0f * gGalaxy.width * 0.8f * ((float) gGalaxy.arms / TWO_PI);
    float arm     = (float)(next_arm % gGalaxy.arms) + scatter;
    next_arm++;

    assigned[key] = arm;
    return arm;
}

vec2 galaxyForce(const RDirNode* top, int depth, const vec2& centre, const vec2& pos) {
    vec2 d = pos - centre;
    float r = glm::length(d);

    if(r < 1.0f) return vec2(0.0f);

    // The target angle depends only on the depth of the dir in the tree, never on where
    // it currently is. (Aiming at the arm angle for the dir's own radius makes growing
    // branches chase a moving target and swing around like orbiting blobs.)
    float want = galaxyArmAngle(armOf(top), (float) depth * RING_SPACING);
    float cur  = atan2f(d.y, d.x);

    float diff = fmodf(want - cur, TWO_PI);
    if(diff >  TWO_PI * 0.5f) diff -= TWO_PI;
    if(diff < -TWO_PI * 0.5f) diff += TWO_PI;

    // free inside the arm, pulled back once outside it
    float w = gGalaxy.width;
    float excess = diff - std::max(-w, std::min(w, diff));

    vec2 tangent(-d.y / r, d.x / r);

    return tangent * (excess * r * gGalaxy.pull);
}

//---------------------------------------------------------------- background

static const char* GALAXY_VERT =
    "#version 120\n"
    "void main() { gl_Position = gl_Vertex; }\n";

// A volume raymarched through the real Gource camera. The galaxy lies in the
// z = 0 plane of the world, the same plane the tree is laid out in, so a camera looking down on it
// sees a disc with a bulge and dust lanes,
// and the arms stay under the directories that follow them.
static const char* GALAXY_FRAG =
    "#version 120\n"
    "uniform vec2  uRes;\n"
    "uniform float uTime;\n"
    "uniform vec3  uCam;\n"
    "uniform vec3  uRight;\n"
    "uniform vec3  uUp;\n"
    "uniform vec3  uFwd;\n"
    "uniform vec2  uTan;\n"
    "uniform vec2  uGC;\n"
    "uniform float uRadius;\n"
    "uniform float uArms;\n"
    "uniform float uTwist;\n"
    "uniform float uR0;\n"
    "uniform float uRot;\n"
    "uniform float uBright;\n"
    "\n"
    "float hash31(vec3 p) { p = fract(p * 0.1031); p += dot(p, p.yzx + 33.33); return fract((p.x + p.y) * p.z); }\n"
    "\n"
    "float noise3(vec3 p) {\n"
    "    vec3 i = floor(p), f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(mix(hash31(i),                  hash31(i + vec3(1.0, 0.0, 0.0)), f.x),\n"
    "                   mix(hash31(i + vec3(0.0, 1.0, 0.0)), hash31(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),\n"
    "               mix(mix(hash31(i + vec3(0.0, 0.0, 1.0)), hash31(i + vec3(1.0, 0.0, 1.0)), f.x),\n"
    "                   mix(hash31(i + vec3(0.0, 1.0, 1.0)), hash31(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);\n"
    "}\n"
    "\n"
    "float fbm3(vec3 p) {\n"
    "    float a = 0.5, s = 0.0;\n"
    "    for(int i = 0; i < 3; i++) { s += a * noise3(p); p = p * 2.03 + vec3(1.7, 9.2, 3.1); a *= 0.5; }\n"
    "    return s / 0.875;\n"
    "}\n"
    "\n"
    "float fbm5(vec3 p) {\n"
    "    float a = 0.5, s = 0.0;\n"
    "    for(int i = 0; i < 5; i++) { s += a * noise3(p); p = p * 2.03 + vec3(1.7, 9.2, 3.1); a *= 0.5; }\n"
    "    return s / 0.96875;\n"
    "}\n"
    "\n"
    "vec3 starLayer(vec3 d, float scale, float density, float size, float seed) {\n"
    "    vec3 p  = d * scale;\n"
    "    vec3 id = floor(p);\n"
    "    vec3 f  = fract(p) - 0.5;\n"
    "    float h = hash31(id + seed);\n"
    "    if(h > density) return vec3(0.0);\n"
    "    vec3 off = (vec3(hash31(id + seed + 1.3), hash31(id + seed + 2.7), hash31(id + seed + 5.1)) - 0.5) * 0.6;\n"
    "    float dd = length(f - off);\n"
    "    float tw = 0.75 + 0.25 * sin(uTime * (0.8 + 3.0 * h / density) + h * 90.0);\n"
    "    float s  = smoothstep(size, 0.0, dd) * tw;\n"
    "    vec3 col = mix(vec3(0.62, 0.76, 1.0), vec3(1.0, 0.86, 0.66), hash31(id + 9.1 + seed));\n"
    "    return col * s * (0.4 + 1.0 * hash31(id + 4.4 + seed));\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 ndc = (gl_FragCoord.xy / uRes) * 2.0 - 1.0;\n"
    "    vec3 rd  = normalize(uFwd + ndc.x * uTan.x * uRight + ndc.y * uTan.y * uUp);\n"
    "    vec3 ro  = uCam - vec3(uGC, 0.0);\n"
    "\n"
    "    // sky: distant gas and stars, fixed in direction so they hold still as the camera moves\n"
    "    float g1 = fbm5(rd * 2.2 + 3.0);\n"
    "    float g2 = fbm5(rd * 4.0 + g1 * 1.6 + 11.0);\n"
    "    vec3 col = vec3(0.0);\n"
    "    col += vec3(0.20, 0.06, 0.40) * smoothstep(0.38, 0.85, g1);\n"
    "    col += vec3(0.02, 0.17, 0.34) * smoothstep(0.42, 0.90, g2);\n"
    "    col += vec3(0.42, 0.09, 0.26) * pow(smoothstep(0.50, 0.95, g1 * g2 * 2.0), 2.0) * 0.7;\n"
    "    col *= 0.8;\n"
    "\n"
    "    col += starLayer(rd, 70.0,  0.12, 0.22, 1.0) * 0.9;\n"
    "    col += starLayer(rd, 120.0, 0.09, 0.20, 2.0) * 1.1;\n"
    "    col += starLayer(rd, 210.0, 0.07, 0.20, 3.0) * 1.3;\n"
    "\n"
    "    // galaxy volume\n"
    "    float R = max(uRadius, 60.0);\n"
    "    vec3 b  = vec3(1.2 * R, 1.2 * R, 0.30 * R);\n"
    "    vec3 inv = 1.0 / (rd + vec3(1e-6));\n"
    "    vec3 t0 = (-b - ro) * inv;\n"
    "    vec3 t1 = ( b - ro) * inv;\n"
    "    float tmin = max(max(min(t0.x, t1.x), min(t0.y, t1.y)), min(t0.z, t1.z));\n"
    "    float tmax = min(min(max(t0.x, t1.x), max(t0.y, t1.y)), max(t0.z, t1.z));\n"
    "    tmin = max(tmin, 0.0);\n"
    "\n"
    "    if(tmax > tmin) {\n"
    "        const int STEPS = 44;\n"
    "        float dt = (tmax - tmin) / float(STEPS);\n"
    "        float t  = tmin + dt * hash31(vec3(gl_FragCoord.xy, uTime * 60.0));\n"
    "        float ds = dt / R;\n"
    "        float T  = 1.0;\n"
    "        vec3 acc = vec3(0.0);\n"
    "\n"
    "        for(int i = 0; i < STEPS; i++) {\n"
    "            vec3 p  = ro + rd * t;\n"
    "            float r  = length(p.xy);\n"
    "            float rn = r / R;\n"
    "            float th = atan(p.y, p.x);\n"
    "\n"
    "            float Hd   = R * (0.016 + 0.040 * rn);\n"
    "            float vert = exp(-abs(p.z) / Hd);\n"
    "\n"
    "            float ph   = th - uTwist * log(1.0 + r / uR0) - uRot;\n"
    "            float warp = (noise3(p / 120.0 + 5.0) - 0.5) * 0.9 * smoothstep(0.0, 0.5, rn);\n"
    "            float a    = uArms * (ph + warp);\n"
    "            float arm  = pow(0.5 + 0.5 * cos(a), 2.5);\n"
    "            float fall = exp(-rn * 1.8);\n"
    "            float n    = fbm3(p / 28.0);\n"
    "\n"
    "            float disc  = fall * (0.12 + 1.3 * arm) * (0.35 + 1.1 * n) * vert;\n"
    "            float bulge = exp(-length(vec3(p.xy, p.z * 2.2)) / (R * 0.10));\n"
    "            float lane  = pow(0.5 + 0.5 * cos(a - 0.9), 6.0) * smoothstep(0.08, 0.35, rn) * vert;\n"
    "            float dust  = lane * (0.3 + 1.4 * n) * fall;\n"
    "\n"
    "            vec3 armCol = mix(vec3(0.95, 0.70, 0.50), vec3(0.35, 0.50, 1.0), smoothstep(0.05, 0.6, rn));\n"
    "            vec3 em = armCol * disc;\n"
    "            float knots = smoothstep(0.58, 0.82, n) * arm * fall * vert * smoothstep(0.08, 0.3, rn);\n"
    "            em += vec3(1.0, 0.25, 0.5) * knots * 2.2;\n"
    "            em += vec3(1.0, 0.82, 0.6) * bulge * 2.4;\n"
    "\n"
    "            acc += T * em * ds * 7.0;\n"
    "            T   *= exp(-dust * ds * 40.0 - (disc + bulge) * ds * 1.2);\n"
    "            if(T < 0.02) break;\n"
    "            t += dt;\n"
    "        }\n"
    "\n"
    "        // the galaxy hides the sky behind it, and gets its own faint stars\n"
    "        col = col * T + acc;\n"
    "    }\n"
    "\n"
    "    vec2 v = (gl_FragCoord.xy / uRes - 0.5) * vec2(1.1, 1.0);\n"
    "    col *= 1.0 - 0.45 * dot(v, v) * 1.6;\n"
    "\n"
    "    col = 1.0 - exp(-col * 1.15 * uBright);\n"
    "    gl_FragColor = vec4(col * 0.7, 1.0);\n"
    "}\n";

GalaxyBackground::GalaxyBackground() {
    program   = 0;
    failed    = false;
    radius    = 300.0f;
    projected = false;
    cam       = vec3(0.0f, 0.0f, -300.0f);
    right     = vec3(1.0f, 0.0f, 0.0f);
    up        = vec3(0.0f, 1.0f, 0.0f);
    fwd       = vec3(0.0f, 0.0f, 1.0f);
    tanhalf   = vec2(1.0f, 1.0f);
    gc        = vec2(0.0f);
}

static GLuint buildShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    if(!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), 0, log);
        fprintf(stderr, "galaxy shader: %s\n", log);
        glDeleteShader(s);
        return 0;
    }

    return s;
}

bool GalaxyBackground::compile() {
    if(!GLEW_VERSION_2_0) return false;

    GLuint vs = buildShader(GL_VERTEX_SHADER, GALAXY_VERT);
    GLuint fs = buildShader(GL_FRAGMENT_SHADER, GALAXY_FRAG);

    if(!vs || !fs) return false;

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);

    if(!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), 0, log);
        fprintf(stderr, "galaxy shader link: %s\n", log);
        glDeleteProgram(program);
        program = 0;
        return false;
    }

    return true;
}

// take the camera straight from the modelview / projection matrices Gource just set up
void GalaxyBackground::project(const vec2& root_pos, const GLdouble* mv, const GLdouble* pr) {
    right = vec3((float) mv[0], (float) mv[4], (float) mv[8]);
    up    = vec3((float) mv[1], (float) mv[5], (float) mv[9]);
    vec3 back((float) mv[2], (float) mv[6], (float) mv[10]);

    fwd = -back;
    cam = -((float) mv[12] * right + (float) mv[13] * up + (float) mv[14] * back);

    if(fabs(pr[0]) < 1e-9 || fabs(pr[5]) < 1e-9) return;

    tanhalf   = vec2((float)(1.0 / pr[0]), (float)(1.0 / pr[5]));
    gc        = root_pos;
    projected = true;
}

void GalaxyBackground::setRadius(float r) {
    radius = r;
}

void GalaxyBackground::draw(int width, int height) {
    if(failed) return;

    if(!program) {
        if(!compile()) {
            failed = true;
            return;
        }
    }

    // before the first frame has a camera, look straight down on the centre
    vec3 c  = projected ? cam : vec3(0.0f, 0.0f, -radius * 2.0f);
    vec2 tn = projected ? tanhalf : vec2(1.0f, (float) height / (float) width);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    glUseProgram(program);

    glUniform2f(glGetUniformLocation(program, "uRes"), (float) width, (float) height);
    glUniform1f(glGetUniformLocation(program, "uTime"), gGalaxy.time);
    glUniform3f(glGetUniformLocation(program, "uCam"), c.x, c.y, c.z);
    glUniform3f(glGetUniformLocation(program, "uRight"), right.x, right.y, right.z);
    glUniform3f(glGetUniformLocation(program, "uUp"), up.x, up.y, up.z);
    glUniform3f(glGetUniformLocation(program, "uFwd"), fwd.x, fwd.y, fwd.z);
    glUniform2f(glGetUniformLocation(program, "uTan"), tn.x, tn.y);
    glUniform2f(glGetUniformLocation(program, "uGC"), gc.x, gc.y);
    glUniform1f(glGetUniformLocation(program, "uRadius"), radius);
    glUniform1f(glGetUniformLocation(program, "uArms"), (float) gGalaxy.arms);
    glUniform1f(glGetUniformLocation(program, "uTwist"), gGalaxy.twist);
    glUniform1f(glGetUniformLocation(program, "uR0"), gGalaxy.r0);
    glUniform1f(glGetUniformLocation(program, "uRot"), gGalaxy.spin * gGalaxy.time);
    glUniform1f(glGetUniformLocation(program, "uBright"), gGalaxy.brightness);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f( 1.0f, -1.0f);
        glVertex2f( 1.0f,  1.0f);
        glVertex2f(-1.0f,  1.0f);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glUseProgram(0);

    glPopAttrib();
}

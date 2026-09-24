#version 330 core

// ============================================================
// terrain.frag - 地形片元着色器
// 按高度分层着色 + Lambert 漫反射 + 环境光 + 指数雾
// ============================================================
in VS_OUT {
    vec3  worldPos;
    vec3  normal;
    float heightRatio;
} fs_in;

out vec4 FragColor;

uniform vec3  uLightDir;        // 指向光源的单位向量
uniform vec3  uLightColor;
uniform vec3  uAmbientColor;
uniform vec3  uViewPos;
uniform vec3  uFogColor;
uniform float uFogDensity;
uniform float uSnowLine;        // 雪线高度比
uniform float uWaterLevel;      // 水位高度比

// 按高度比插值分层配色：深水 → 浅水 → 沙滩 → 草地 → 岩石 → 雪
vec3 terrainPalette(float t) {
    // 各层颜色（线性空间近似值）
    const vec3 cDeepWater  = vec3(0.016, 0.075, 0.196);
    const vec3 cShallow    = vec3(0.043, 0.243, 0.482);
    const vec3 cSand       = vec3(0.780, 0.714, 0.478);
    const vec3 cGrassLow   = vec3(0.216, 0.400, 0.169);
    const vec3 cGrassHigh  = vec3(0.353, 0.451, 0.216);
    const vec3 cRock       = vec3(0.373, 0.345, 0.310);
    const vec3 cSnow       = vec3(0.925, 0.941, 0.965);

    // 分段线性插值的关键点（位置, 颜色）
    // 注意：t 是相对 [0,1] 的高度比，水位以下为水下地形
    if (t < 0.06) {
        return mix(cDeepWater, cShallow, smoothstep(0.0, 0.06, t));
    } else if (t < 0.10) {
        return mix(cShallow, cSand, smoothstep(0.06, 0.10, t));
    } else if (t < 0.16) {
        return mix(cSand, cGrassLow, smoothstep(0.10, 0.16, t));
    } else if (t < 0.45) {
        return mix(cGrassLow, cGrassHigh, smoothstep(0.16, 0.45, t));
    } else if (t < 0.68) {
        return mix(cGrassHigh, cRock, smoothstep(0.45, 0.68, t));
    } else {
        return mix(cRock, cSnow, smoothstep(0.68, uSnowLine, t));
    }
}

void main() {
    vec3 N = normalize(fs_in.normal);
    // 双面光照：背面法线翻转，避免线框/薄壁出现全黑
    if (!gl_FrontFacing) N = -N;

    vec3 L = normalize(-uLightDir);

    // Lambert 漫反射 + 半球环境光（天空色偏蓝、地面反射偏暖）
    float diff = max(dot(N, L), 0.0);
    float hemi = 0.5 + 0.5 * N.y;
    vec3 ambient = uAmbientColor * mix(0.55, 1.0, hemi);

    // 简单高光，让岩层与雪有金属/冰面质感
    vec3 V = normalize(uViewPos - fs_in.worldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 48.0) * 0.12;

    vec3 base = terrainPalette(fs_in.heightRatio);
    vec3 color = base * (ambient + diff * uLightColor) + vec3(spec);

    // 水位线附近加一条亮边，突出海岸线
    float waterEdge = 1.0 - smoothstep(0.0, 0.012, abs(fs_in.heightRatio - uWaterLevel));
    color = mix(color, vec3(0.55, 0.80, 0.95), waterEdge * 0.35);

    // 垂直方向渐变雾，远处融入天空色
    float dist = length(uViewPos - fs_in.worldPos);
    float fog = 1.0 - exp(-pow(dist * uFogDensity, 2.0));
    fog = clamp(fog, 0.0, 1.0);
    color = mix(color, uFogColor, fog);

    // 色调映射 + gamma 校正
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}

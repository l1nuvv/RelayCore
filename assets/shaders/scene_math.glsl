float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 rotate_vec2(vec2 value, float angle)
{
    float sine = sin(angle);
    float cosine = cos(angle);
    return vec2(
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine
    );
}

vec3 safe_normalize_vec3(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (length_squared <= 0.000001)
    {
        return fallback;
    }

    return value * inversesqrt(length_squared);
}

#version 450 core

const vec2 pos[6] =
    vec2[6](
        vec2(-1.0, +1.0),
        vec2(+1.0, +1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0, -1.0),
        vec2(+1.0, +1.0),
        vec2(+1.0, -1.0));

//  B ------ D
//  | Object |
//  A ------ C

layout (location = 0) out vec2 uv;

void main()
{
    gl_Position = vec4(pos[gl_VertexIndex], 0, 1.0);
    uv = pos[gl_VertexIndex] * 0.5 + 0.5;
}

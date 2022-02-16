#version 450 core

const vec2 pos[4] =
    vec2[4](vec2(-1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(1.0, -1.0));

//  B ------ D
//  | Object |
//  A ------ C

void main()
{
    gl_Position = vec4(pos[gl_VertexIndex], 0, 1.0);
}
